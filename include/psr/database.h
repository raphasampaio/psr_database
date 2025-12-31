#ifndef PSR_DATABASE_DATABASE_H
#define PSR_DATABASE_DATABASE_H

#include "export.h"
#include "result.h"

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

struct sqlite3;

namespace psr {

enum class LogLevel { debug, info, warn, error, off };

class PSR_API Database {
public:
    explicit Database(const std::string& path, LogLevel console_level = LogLevel::info);
    ~Database();

    // Factory method for schema-based initialization
    static Database from_schema(const std::string& database_path, const std::string& schema_path,
                                LogLevel console_level = LogLevel::info);

    // Non-copyable
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    // Movable
    Database(Database&& other) noexcept;
    Database& operator=(Database&& other) noexcept;

    bool is_open() const;
    void close();

    Result execute(const std::string& sql);
    Result execute(const std::string& sql, const std::vector<Value>& params);

    int64_t last_insert_rowid() const;
    int changes() const;

    void begin_transaction();
    void commit();
    void rollback();

    const std::string& path() const;
    std::string error_message() const;

    // Migration methods
    int64_t current_version();
    void set_version(int64_t version);
    void migrate_up();
    const std::string& schema_path() const;

    // Element creation (supports scalars, vectors, and time series)
    int64_t create_element(const std::string& table, const std::vector<std::pair<std::string, Value>>& fields);

    // Element creation with time series support
    int64_t create_element(const std::string& table, const std::vector<std::pair<std::string, Value>>& fields,
                           const std::map<std::string, TimeSeries>& time_series);

    // Lookup element ID by label
    int64_t get_element_id(const std::string& collection, const std::string& label) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    // Schema introspection
    std::vector<std::string> get_vector_tables(const std::string& collection) const;
    std::vector<std::string> get_time_series_tables(const std::string& collection) const;
    std::vector<std::string> get_table_columns(const std::string& table) const;

    // Foreign key introspection
    struct ForeignKeyInfo {
        std::string column;
        std::string target_table;
        std::string target_column;
    };
    std::vector<ForeignKeyInfo> get_foreign_keys(const std::string& table) const;

    // Vector/time series insertion helpers
    void insert_vectors(const std::string& collection, int64_t element_id,
                        const std::vector<std::pair<std::string, Value>>& vector_fields);
    void insert_time_series(const std::string& collection, int64_t element_id,
                            const std::map<std::string, TimeSeries>& time_series);

    // Relation resolution
    Value resolve_relation(const std::string& table, const std::string& column, const Value& value);
};

}  // namespace psr

#endif  // PSR_DATABASE_DATABASE_H
