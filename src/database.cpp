#include "psr/database.h"

#include <sqlite3.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace psr {

struct Database::Impl {
    sqlite3* db = nullptr;
    std::string path;
    std::string schema_path;
    std::string last_error;

    ~Impl() {
        if (db) {
            sqlite3_close(db);
        }
    }
};

Database::Database(const std::string& path) : impl_(std::make_unique<Impl>()) {
    impl_->path = path;

    int rc = sqlite3_open(path.c_str(), &impl_->db);
    if (rc != SQLITE_OK) {
        std::string error = sqlite3_errmsg(impl_->db);
        sqlite3_close(impl_->db);
        impl_->db = nullptr;
        throw std::runtime_error("Failed to open database: " + error);
    }

    // Enable foreign keys
    sqlite3_exec(impl_->db, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);
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
        throw std::runtime_error("Failed to prepare statement: " +
                                 std::string(sqlite3_errmsg(impl_->db)));
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
                    sqlite3_bind_text(stmt, idx, arg.c_str(), static_cast<int>(arg.size()),
                                      SQLITE_TRANSIENT);
                } else if constexpr (std::is_same_v<T, std::vector<uint8_t>>) {
                    sqlite3_bind_blob(stmt, idx, arg.data(), static_cast<int>(arg.size()),
                                      SQLITE_TRANSIENT);
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
                const uint8_t* data =
                    reinterpret_cast<const uint8_t*>(sqlite3_column_blob(stmt, i));
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
        throw std::runtime_error("Failed to execute statement: " +
                                 std::string(sqlite3_errmsg(impl_->db)));
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

Database Database::from_schema(const std::string& database_path, const std::string& schema_path) {
    if (!std::filesystem::exists(schema_path)) {
        throw std::runtime_error("Schema path does not exist: " + schema_path);
    }
    if (!std::filesystem::is_directory(schema_path)) {
        throw std::runtime_error("Schema path is not a directory: " + schema_path);
    }

    Database db(database_path);
    db.impl_->schema_path = schema_path;
    db.migrate_up();
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
        return;  // No schema path set, nothing to migrate
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

    // Apply each pending migration
    for (int64_t version : versions) {
        if (version <= current) {
            continue;
        }

        fs::path migration_dir = fs::path(impl_->schema_path) / std::to_string(version);
        fs::path up_sql_path = migration_dir / "up.sql";

        if (!fs::exists(up_sql_path)) {
            throw std::runtime_error("Migration file not found: " + up_sql_path.string());
        }

        // Read the SQL file
        std::ifstream file(up_sql_path);
        if (!file) {
            throw std::runtime_error("Failed to open migration file: " + up_sql_path.string());
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string sql = buffer.str();

        // Apply migration in a transaction
        begin_transaction();
        try {
            execute(sql);
            set_version(version);
            commit();
        } catch (const std::exception& e) {
            rollback();
            throw std::runtime_error("Migration " + std::to_string(version) + " failed: " + e.what());
        }
    }
}

const std::string& Database::schema_path() const {
    return impl_->schema_path;
}

}  // namespace psr
