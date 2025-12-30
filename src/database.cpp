#include "psr/database.h"

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <fstream>
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

int64_t Database::create_element(const std::string& table,
                                  const std::vector<std::pair<std::string, Value>>& fields) {
    if (!is_open()) {
        throw std::runtime_error("Database is not open");
    }

    if (table.empty()) {
        throw std::runtime_error("Table name cannot be empty");
    }

    if (fields.empty()) {
        throw std::runtime_error("Fields cannot be empty");
    }

    // Build INSERT statement: INSERT INTO table (col1, col2, ...) VALUES (?, ?, ...)
    std::string sql = "INSERT INTO \"" + table + "\" (";
    std::string placeholders;

    for (size_t i = 0; i < fields.size(); ++i) {
        if (i > 0) {
            sql += ", ";
            placeholders += ", ";
        }
        sql += "\"" + fields[i].first + "\"";
        placeholders += "?";
    }

    sql += ") VALUES (" + placeholders + ")";

    impl_->logger->debug("create_element SQL: {}", sql);

    // Extract values for binding
    std::vector<Value> values;
    values.reserve(fields.size());
    for (const auto& field : fields) {
        values.push_back(field.second);
    }

    // Execute the insert
    execute(sql, values);

    return last_insert_rowid();
}

}  // namespace psr
