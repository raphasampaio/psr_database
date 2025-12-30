#ifndef PSR_DATABASE_DATABASE_H
#define PSR_DATABASE_DATABASE_H

#include "export.h"
#include "result.h"

#include <memory>
#include <string>
#include <vector>

struct sqlite3;

namespace psr {

class PSR_API Database {
public:
    explicit Database(const std::string& path);
    ~Database();

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

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace psr

#endif  // PSR_DATABASE_DATABASE_H
