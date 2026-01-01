#include "psr/database.h"

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <limits>
#include <set>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <sqlite3.h>
#include <sstream>
#include <stdexcept>

namespace {

std::atomic<uint64_t> g_logger_counter{0};

spdlog::level::level_enum to_spdlog_level(psr::LogLevel level) {
    switch (level) {
    case psr::LogLevel::debug:
        return spdlog::level::debug;
    case psr::LogLevel::info:
        return spdlog::level::info;
    case psr::LogLevel::warn:
        return spdlog::level::warn;
    case psr::LogLevel::error:
        return spdlog::level::err;
    case psr::LogLevel::off:
        return spdlog::level::off;
    default:
        return spdlog::level::info;
    }
}

std::shared_ptr<spdlog::logger> create_database_logger(const std::string& db_path, psr::LogLevel console_level) {
    namespace fs = std::filesystem;

    // Generate unique logger name for multiple Database instances
    uint64_t id = g_logger_counter.fetch_add(1);
    std::string logger_name = "psr_database_" + std::to_string(id);

    // Create console sink (thread-safe)
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(to_spdlog_level(console_level));

    // Determine log file path
    std::string log_file_path;

    if (db_path == ":memory:" || db_path.empty()) {
        // In-memory database: use current working directory
        log_file_path = (fs::current_path() / "psr_database.log").string();
    } else {
        // File-based database: use database directory
        fs::path db_dir = fs::path(db_path).parent_path();
        if (db_dir.empty()) {
            // Database file in current directory (no path separator)
            db_dir = fs::current_path();
        }
        log_file_path = (db_dir / "psr_database.log").string();
    }

    // Create file sink (thread-safe)
    std::shared_ptr<spdlog::sinks::basic_file_sink_mt> file_sink;
    try {
        file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_file_path, true);
        file_sink->set_level(spdlog::level::debug);
    } catch (const spdlog::spdlog_ex& ex) {
        // If file sink creation fails, continue with console-only logging
        auto logger = std::make_shared<spdlog::logger>(logger_name, console_sink);
        logger->set_level(spdlog::level::debug);
        logger->warn("Failed to create file sink: {}. Logging to console only.", ex.what());
        return logger;
    }

    // Create logger with both sinks
    std::vector<spdlog::sink_ptr> sinks{console_sink, file_sink};
    auto logger = std::make_shared<spdlog::logger>(logger_name, sinks.begin(), sinks.end());
    logger->set_level(spdlog::level::debug);

    return logger;
}

}  // anonymous namespace

namespace psr {

struct Database::Impl {
    sqlite3* db = nullptr;
    std::string path;
    std::string schema_path;
    std::string last_error;
    std::shared_ptr<spdlog::logger> logger;

    ~Impl() {
        if (db) {
            sqlite3_close(db);
        }
    }
};

Database::Database(const std::string& path, LogLevel console_level) : impl_(std::make_unique<Impl>()) {
    impl_->path = path;
    impl_->logger = create_database_logger(path, console_level);

    impl_->logger->debug("Opening database: {}", path);

    int rc = sqlite3_open(path.c_str(), &impl_->db);
    if (rc != SQLITE_OK) {
        std::string error = sqlite3_errmsg(impl_->db);
        impl_->logger->error("Failed to open database: {}", error);
        sqlite3_close(impl_->db);
        impl_->db = nullptr;
        throw std::runtime_error("Failed to open database: " + error);
    }

    // Enable foreign keys
    sqlite3_exec(impl_->db, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);
    impl_->logger->debug("Database opened successfully, foreign keys enabled");
}

Database::~Database() = default;

Database::Database(Database&& other) noexcept = default;

Database& Database::operator=(Database&& other) noexcept = default;

bool Database::is_open() const {
    return impl_ && impl_->db != nullptr;
}

void Database::close() {
    if (impl_ && impl_->db) {
        sqlite3_close(impl_->db);
        impl_->db = nullptr;
    }
}

Result Database::execute(const std::string& sql) {
    return execute(sql, {});
}

Result Database::execute(const std::string& sql, const std::vector<Value>& params) {
    if (!is_open()) {
        throw std::runtime_error("Database is not open");
    }

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(impl_->db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare statement: " + std::string(sqlite3_errmsg(impl_->db)));
    }

    // Bind parameters
    for (size_t i = 0; i < params.size(); ++i) {
        int idx = static_cast<int>(i + 1);
        const auto& param = params[i];

        std::visit(
            [&](auto&& arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, std::nullptr_t>) {
                    sqlite3_bind_null(stmt, idx);
                } else if constexpr (std::is_same_v<T, int64_t>) {
                    sqlite3_bind_int64(stmt, idx, arg);
                } else if constexpr (std::is_same_v<T, double>) {
                    sqlite3_bind_double(stmt, idx, arg);
                } else if constexpr (std::is_same_v<T, std::string>) {
                    sqlite3_bind_text(stmt, idx, arg.c_str(), static_cast<int>(arg.size()), SQLITE_TRANSIENT);
                } else if constexpr (std::is_same_v<T, std::vector<uint8_t>>) {
                    sqlite3_bind_blob(stmt, idx, arg.data(), static_cast<int>(arg.size()), SQLITE_TRANSIENT);
                }
            },
            param);
    }

    // Get column info
    std::vector<std::string> columns;
    int col_count = sqlite3_column_count(stmt);
    columns.reserve(col_count);
    for (int i = 0; i < col_count; ++i) {
        const char* name = sqlite3_column_name(stmt, i);
        columns.emplace_back(name ? name : "");
    }

    // Execute and fetch results
    std::vector<Row> rows;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        std::vector<Value> values;
        values.reserve(col_count);

        for (int i = 0; i < col_count; ++i) {
            int type = sqlite3_column_type(stmt, i);
            switch (type) {
            case SQLITE_INTEGER:
                values.emplace_back(sqlite3_column_int64(stmt, i));
                break;
            case SQLITE_FLOAT:
                values.emplace_back(sqlite3_column_double(stmt, i));
                break;
            case SQLITE_TEXT: {
                const char* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, i));
                values.emplace_back(std::string(text ? text : ""));
                break;
            }
            case SQLITE_BLOB: {
                const uint8_t* data = reinterpret_cast<const uint8_t*>(sqlite3_column_blob(stmt, i));
                int size = sqlite3_column_bytes(stmt, i);
                values.emplace_back(std::vector<uint8_t>(data, data + size));
                break;
            }
            case SQLITE_NULL:
            default:
                values.emplace_back(nullptr);
                break;
            }
        }
        rows.emplace_back(std::move(values));
    }

    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        throw std::runtime_error("Failed to execute statement: " + std::string(sqlite3_errmsg(impl_->db)));
    }

    return Result(std::move(columns), std::move(rows));
}

int64_t Database::last_insert_rowid() const {
    if (!is_open()) {
        return 0;
    }
    return sqlite3_last_insert_rowid(impl_->db);
}

int Database::changes() const {
    if (!is_open()) {
        return 0;
    }
    return sqlite3_changes(impl_->db);
}

void Database::begin_transaction() {
    execute("BEGIN TRANSACTION");
}

void Database::commit() {
    execute("COMMIT");
}

void Database::rollback() {
    execute("ROLLBACK");
}

const std::string& Database::path() const {
    return impl_->path;
}

std::string Database::error_message() const {
    if (!impl_ || !impl_->db) {
        return "Database not open";
    }
    return sqlite3_errmsg(impl_->db);
}

Database Database::from_schema(const std::string& database_path, const std::string& schema_path,
                               LogLevel console_level) {
    // Validate schema path before creating database
    if (!std::filesystem::exists(schema_path)) {
        throw std::runtime_error("Schema path does not exist: " + schema_path);
    }
    if (!std::filesystem::is_directory(schema_path)) {
        throw std::runtime_error("Schema path is not a directory: " + schema_path);
    }

    Database db(database_path, console_level);
    db.impl_->schema_path = schema_path;

    db.impl_->logger->info("Opening database from schema: db={}, schema={}", database_path, schema_path);
    db.impl_->logger->debug("Database opened, current version: {}", db.current_version());

    db.migrate_up();

    db.impl_->logger->info("Database ready, version: {}", db.current_version());
    return db;
}

int64_t Database::current_version() {
    if (!is_open()) {
        throw std::runtime_error("Database is not open");
    }

    auto result = execute("PRAGMA user_version");
    if (result.empty()) {
        return 0;
    }

    auto version = result[0].get_int(0);
    return version.value_or(0);
}

void Database::set_version(int64_t version) {
    if (!is_open()) {
        throw std::runtime_error("Database is not open");
    }

    execute("PRAGMA user_version = " + std::to_string(version));
}

void Database::migrate_up() {
    if (!is_open()) {
        throw std::runtime_error("Database is not open");
    }

    if (impl_->schema_path.empty()) {
        impl_->logger->debug("No schema path set, skipping migrations");
        return;
    }

    namespace fs = std::filesystem;

    // Collect all numbered migration directories
    std::vector<int64_t> versions;
    for (const auto& entry : fs::directory_iterator(impl_->schema_path)) {
        if (!entry.is_directory()) {
            continue;
        }

        const auto& dirname = entry.path().filename().string();
        try {
            size_t pos = 0;
            int64_t version = std::stoll(dirname, &pos);
            if (pos == dirname.size() && version > 0) {
                versions.push_back(version);
            }
        } catch (const std::exception&) {
            // Not a numeric directory name, skip
        }
    }

    // Sort versions ascending
    std::sort(versions.begin(), versions.end());

    int64_t current = current_version();
    impl_->logger->debug("Found {} migrations, current version: {}", versions.size(), current);

    // Apply each pending migration
    for (int64_t version : versions) {
        if (version <= current) {
            continue;
        }

        fs::path migration_dir = fs::path(impl_->schema_path) / std::to_string(version);
        fs::path up_sql_path = migration_dir / "up.sql";

        if (!fs::exists(up_sql_path)) {
            impl_->logger->error("Migration file not found: {}", up_sql_path.string());
            throw std::runtime_error("Migration file not found: " + up_sql_path.string());
        }

        // Read the SQL file
        std::ifstream file(up_sql_path);
        if (!file) {
            impl_->logger->error("Failed to open migration file: {}", up_sql_path.string());
            throw std::runtime_error("Failed to open migration file: " + up_sql_path.string());
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string sql = buffer.str();

        // Apply migration in a transaction
        impl_->logger->info("Applying migration {}", version);
        begin_transaction();
        try {
            execute(sql);
            set_version(version);
            commit();
            impl_->logger->debug("Migration {} applied successfully", version);
        } catch (const std::exception& e) {
            rollback();
            impl_->logger->error("Migration {} failed: {}", version, e.what());
            throw std::runtime_error("Migration " + std::to_string(version) + " failed: " + e.what());
        }
    }
}

const std::string& Database::schema_path() const {
    return impl_->schema_path;
}

// Helper to check if a Value is a vector type
namespace {

bool is_vector_value(const psr::Value& v) {
    return std::holds_alternative<std::vector<int64_t>>(v) || std::holds_alternative<std::vector<double>>(v) ||
           std::holds_alternative<std::vector<std::string>>(v);
}

// Convert a scalar Value to a bindable Value (handles only scalar types)
psr::Value to_scalar_value(const psr::Value& v) {
    return std::visit(
        [](auto&& arg) -> psr::Value {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, std::vector<int64_t>> || std::is_same_v<T, std::vector<double>> ||
                          std::is_same_v<T, std::vector<std::string>>) {
                throw std::runtime_error("Cannot convert vector to scalar value");
            } else {
                return arg;
            }
        },
        v);
}

// Get vector size from a Value
size_t get_vector_size(const psr::Value& v) {
    return std::visit(
        [](auto&& arg) -> size_t {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, std::vector<int64_t>>) {
                return arg.size();
            } else if constexpr (std::is_same_v<T, std::vector<double>>) {
                return arg.size();
            } else if constexpr (std::is_same_v<T, std::vector<std::string>>) {
                return arg.size();
            } else {
                return 0;
            }
        },
        v);
}

// Get element at index from a vector Value
psr::Value get_vector_element(const psr::Value& v, size_t index) {
    return std::visit(
        [index](auto&& arg) -> psr::Value {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, std::vector<int64_t>>) {
                return arg[index];
            } else if constexpr (std::is_same_v<T, std::vector<double>>) {
                return arg[index];
            } else if constexpr (std::is_same_v<T, std::vector<std::string>>) {
                return arg[index];
            } else {
                throw std::runtime_error("Not a vector type");
            }
        },
        v);
}

}  // anonymous namespace

// Schema introspection methods

std::vector<std::string> Database::get_vector_tables(const std::string& collection) const {
    if (!is_open()) {
        return {};
    }

    std::string prefix = collection + "_vector_";
    auto result = const_cast<Database*>(this)->execute(
        "SELECT name FROM sqlite_master WHERE type='table' AND name LIKE ?", {prefix + "%"});

    std::vector<std::string> tables;
    for (const auto& row : result) {
        auto name = row.get_string(0);
        if (name) {
            tables.push_back(*name);
        }
    }
    return tables;
}

std::vector<std::string> Database::get_set_tables(const std::string& collection) const {
    if (!is_open()) {
        return {};
    }

    std::string prefix = collection + "_set_";
    auto result = const_cast<Database*>(this)->execute(
        "SELECT name FROM sqlite_master WHERE type='table' AND name LIKE ?", {prefix + "%"});

    std::vector<std::string> tables;
    for (const auto& row : result) {
        auto name = row.get_string(0);
        if (name) {
            tables.push_back(*name);
        }
    }
    return tables;
}

std::vector<std::string> Database::get_time_series_tables(const std::string& collection) const {
    if (!is_open()) {
        return {};
    }

    std::string prefix = collection + "_time_series_";
    auto result = const_cast<Database*>(this)->execute(
        "SELECT name FROM sqlite_master WHERE type='table' AND name LIKE ?", {prefix + "%"});

    std::vector<std::string> tables;
    for (const auto& row : result) {
        auto name = row.get_string(0);
        if (name) {
            tables.push_back(*name);
        }
    }
    return tables;
}

std::vector<std::string> Database::get_table_columns(const std::string& table) const {
    if (!is_open()) {
        return {};
    }

    auto result = const_cast<Database*>(this)->execute("PRAGMA table_info(\"" + table + "\")");

    std::vector<std::string> columns;
    for (const auto& row : result) {
        auto name = row.get_string(1);  // Column name is at index 1
        if (name) {
            columns.push_back(*name);
        }
    }
    return columns;
}

std::vector<Database::ForeignKeyInfo> Database::get_foreign_keys(const std::string& table) const {
    if (!is_open()) {
        return {};
    }

    auto result = const_cast<Database*>(this)->execute("PRAGMA foreign_key_list(\"" + table + "\")");

    std::vector<ForeignKeyInfo> fks;
    for (const auto& row : result) {
        ForeignKeyInfo fk;
        fk.target_table = row.get_string(2).value_or("");   // table
        fk.column = row.get_string(3).value_or("");         // from
        fk.target_column = row.get_string(4).value_or("");  // to
        if (!fk.column.empty()) {
            fks.push_back(fk);
        }
    }
    return fks;
}

std::string Database::get_column_type(const std::string& table, const std::string& column) const {
    if (!is_open()) {
        return "";
    }

    auto result = const_cast<Database*>(this)->execute("PRAGMA table_info(\"" + table + "\")");

    for (const auto& row : result) {
        auto col_name = row.get_string(1);  // name is at index 1
        if (col_name && *col_name == column) {
            return row.get_string(2).value_or("");  // type is at index 2
        }
    }
    return "";
}

void Database::validate_value_type(const std::string& table, const std::string& column, const Value& value) {
    std::string col_type = get_column_type(table, column);
    if (col_type.empty()) {
        return;  // Column not found, let SQLite handle it
    }

    // Skip validation for foreign key columns - they accept string labels that get resolved to IDs
    auto fks = get_foreign_keys(table);
    for (const auto& fk : fks) {
        if (fk.column == column) {
            return;  // This is a FK column, skip type validation
        }
    }

    // Null is always valid
    if (std::holds_alternative<std::nullptr_t>(value)) {
        return;
    }

    bool valid = true;
    std::string actual_type;

    if (std::holds_alternative<int64_t>(value)) {
        actual_type = "INTEGER";
    } else if (std::holds_alternative<double>(value)) {
        actual_type = "REAL";
    } else if (std::holds_alternative<std::string>(value)) {
        actual_type = "TEXT";
    } else if (std::holds_alternative<std::vector<uint8_t>>(value)) {
        actual_type = "BLOB";
    } else {
        return;  // Vector types are handled separately
    }

    // Check type compatibility
    if (col_type == "TEXT") {
        valid = (actual_type == "TEXT");
    } else if (col_type == "INTEGER") {
        valid = (actual_type == "INTEGER");
    } else if (col_type == "REAL") {
        valid = (actual_type == "REAL" || actual_type == "INTEGER");  // INTEGER can be REAL
    } else if (col_type == "BLOB") {
        valid = (actual_type == "BLOB");
    }

    if (!valid) {
        throw std::runtime_error("Type mismatch for column '" + column + "': expected " + col_type + " but got " +
                                 actual_type);
    }
}

int64_t Database::get_element_id(const std::string& collection, const std::string& label) const {
    if (!is_open()) {
        throw std::runtime_error("Database is not open");
    }

    auto result =
        const_cast<Database*>(this)->execute("SELECT id FROM \"" + collection + "\" WHERE label = ?", {label});

    if (result.empty()) {
        throw std::runtime_error("Element not found: " + label + " in " + collection);
    }

    auto id = result[0].get_int(0);
    if (!id) {
        throw std::runtime_error("Invalid ID for element: " + label);
    }
    return *id;
}

Value Database::resolve_relation(const std::string& table, const std::string& column, const Value& value) {
    // Get foreign key info for this column
    auto fks = get_foreign_keys(table);

    for (const auto& fk : fks) {
        if (fk.column == column) {
            // This column is a foreign key
            if (std::holds_alternative<std::string>(value)) {
                // Resolve label to ID
                const auto& label = std::get<std::string>(value);
                return get_element_id(fk.target_table, label);
            } else if (std::holds_alternative<std::vector<std::string>>(value)) {
                // Resolve vector of labels to vector of IDs
                const auto& labels = std::get<std::vector<std::string>>(value);
                std::vector<int64_t> ids;
                ids.reserve(labels.size());
                for (const auto& label : labels) {
                    if (label.empty()) {
                        // Empty string means NULL - use a sentinel value
                        ids.push_back(std::numeric_limits<int64_t>::min());
                    } else {
                        ids.push_back(get_element_id(fk.target_table, label));
                    }
                }
                return ids;
            }
            break;
        }
    }

    // Not a relation or already an ID, return as-is
    return value;
}

void Database::insert_vectors(const std::string& collection, int64_t element_id,
                              const std::vector<std::pair<std::string, Value>>& vector_fields) {
    if (vector_fields.empty()) {
        return;
    }

    // Group vectors by their table (check both _vector_ and _set_ tables)
    auto vector_tables = get_vector_tables(collection);
    auto set_tables = get_set_tables(collection);

    // Build a map of column -> table and track which are set tables
    std::map<std::string, std::string> column_to_table;
    std::set<std::string> is_set_table;

    for (const auto& table : vector_tables) {
        auto columns = get_table_columns(table);
        for (const auto& col : columns) {
            if (col != "id" && col != "vector_index") {
                column_to_table[col] = table;
            }
        }
    }

    for (const auto& table : set_tables) {
        is_set_table.insert(table);
        auto columns = get_table_columns(table);
        for (const auto& col : columns) {
            if (col != "id") {
                column_to_table[col] = table;
            }
        }
    }

    // Group fields by table
    std::map<std::string, std::vector<std::pair<std::string, Value>>> table_fields;
    for (const auto& [name, value] : vector_fields) {
        auto it = column_to_table.find(name);
        if (it == column_to_table.end()) {
            throw std::runtime_error("Vector column not found in schema: " + name);
        }
        table_fields[it->second].emplace_back(name, value);
    }

    // Insert into each table
    for (const auto& [table, fields] : table_fields) {
        bool is_set = is_set_table.count(table) > 0;

        // Validate all vectors have same size
        size_t vec_size = 0;
        for (const auto& [name, value] : fields) {
            size_t sz = get_vector_size(value);
            if (vec_size == 0) {
                vec_size = sz;
            } else if (sz != vec_size) {
                throw std::runtime_error("Vectors in same group must have same size: " + table);
            }
        }

        if (vec_size == 0) {
            // Empty vectors are allowed - just skip inserting anything
            continue;
        }

        // Resolve relations for this table
        std::vector<std::pair<std::string, Value>> resolved_fields;
        for (const auto& [name, value] : fields) {
            resolved_fields.emplace_back(name, resolve_relation(table, name, value));
        }

        // Get table columns to determine column order
        auto columns = get_table_columns(table);

        // Insert one row per vector index
        for (size_t i = 0; i < vec_size; ++i) {
            std::string sql;
            std::string placeholders;
            std::vector<Value> values;

            if (is_set) {
                // Set tables don't have vector_index
                sql = "INSERT INTO \"" + table + "\" (id";
                placeholders = "?";
                values.push_back(element_id);
            } else {
                // Vector tables have vector_index
                sql = "INSERT INTO \"" + table + "\" (id, vector_index";
                placeholders = "?, ?";
                values.push_back(element_id);
                values.push_back(static_cast<int64_t>(i + 1));  // 1-indexed
            }

            for (const auto& [name, value] : resolved_fields) {
                sql += ", \"" + name + "\"";
                placeholders += ", ?";

                Value elem = get_vector_element(value, i);
                // Handle sentinel value for NULL
                if (std::holds_alternative<int64_t>(elem)) {
                    int64_t v = std::get<int64_t>(elem);
                    if (v == std::numeric_limits<int64_t>::min()) {
                        values.push_back(nullptr);
                    } else {
                        values.push_back(elem);
                    }
                } else {
                    values.push_back(elem);
                }
            }

            sql += ") VALUES (" + placeholders + ")";
            execute(sql, values);
        }
    }
}

void Database::insert_time_series(const std::string& collection, int64_t element_id,
                                  const std::map<std::string, TimeSeries>& time_series) {
    if (time_series.empty()) {
        return;
    }

    for (const auto& [group, data] : time_series) {
        if (data.empty()) {
            continue;
        }

        std::string table = collection + "_time_series_" + group;

        // Check if table exists
        auto ts_tables = get_time_series_tables(collection);
        bool found = false;
        for (const auto& t : ts_tables) {
            if (t == table) {
                found = true;
                break;
            }
        }
        if (!found) {
            throw std::runtime_error("Time series group not found: " + group);
        }

        // Get row count from first column
        size_t row_count = 0;
        for (const auto& [col, values] : data) {
            row_count = values.size();
            break;
        }

        if (row_count == 0) {
            continue;
        }

        // Validate all columns have same length
        for (const auto& [col, values] : data) {
            if (values.size() != row_count) {
                throw std::runtime_error("Time series columns must have same length");
            }
        }

        // Insert each row
        for (size_t i = 0; i < row_count; ++i) {
            std::string sql = "INSERT INTO \"" + table + "\" (id";
            std::string placeholders = "?";
            std::vector<Value> values;
            values.push_back(element_id);

            for (const auto& [col, col_values] : data) {
                sql += ", \"" + col + "\"";
                placeholders += ", ?";
                values.push_back(to_scalar_value(col_values[i]));
            }

            sql += ") VALUES (" + placeholders + ")";
            execute(sql, values);
        }
    }
}

int64_t Database::create_element(const std::string& table, const std::vector<std::pair<std::string, Value>>& fields) {
    return create_element(table, fields, {});
}

int64_t Database::create_element(const std::string& table, const std::vector<std::pair<std::string, Value>>& fields,
                                 const std::map<std::string, TimeSeries>& time_series) {
    if (!is_open()) {
        throw std::runtime_error("Database is not open");
    }

    if (table.empty()) {
        throw std::runtime_error("Table name cannot be empty");
    }

    if (fields.empty()) {
        throw std::runtime_error("Fields cannot be empty");
    }

    // Separate scalar fields from vector fields
    std::vector<std::pair<std::string, Value>> scalar_fields;
    std::vector<std::pair<std::string, Value>> vector_fields;

    for (const auto& [name, value] : fields) {
        if (is_vector_value(value)) {
            vector_fields.emplace_back(name, value);
        } else {
            // Validate type before resolving relations
            validate_value_type(table, name, value);
            // Resolve scalar relations (string to ID for FK columns)
            scalar_fields.emplace_back(name, resolve_relation(table, name, value));
        }
    }

    // Build INSERT statement for scalar fields
    if (scalar_fields.empty()) {
        throw std::runtime_error("At least one scalar field is required");
    }

    std::string sql = "INSERT INTO \"" + table + "\" (";
    std::string placeholders;

    for (size_t i = 0; i < scalar_fields.size(); ++i) {
        if (i > 0) {
            sql += ", ";
            placeholders += ", ";
        }
        sql += "\"" + scalar_fields[i].first + "\"";
        placeholders += "?";
    }

    sql += ") VALUES (" + placeholders + ")";

    impl_->logger->debug("create_element SQL: {}", sql);

    // Extract values for binding
    std::vector<Value> values;
    values.reserve(scalar_fields.size());
    for (const auto& [name, value] : scalar_fields) {
        values.push_back(to_scalar_value(value));
    }

    // Execute the insert
    execute(sql, values);

    int64_t element_id = last_insert_rowid();

    // Insert vector fields
    insert_vectors(table, element_id, vector_fields);

    // Insert time series
    insert_time_series(table, element_id, time_series);

    return element_id;
}

}  // namespace psr
