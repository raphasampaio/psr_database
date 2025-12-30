#ifndef PSR_DATABASE_RESULT_H
#define PSR_DATABASE_RESULT_H

#include "export.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace psr {

using Value = std::variant<std::nullptr_t, int64_t, double, std::string, std::vector<uint8_t>>;

class PSR_API Row {
public:
    Row() = default;
    explicit Row(std::vector<Value> values);

    size_t column_count() const;
    const Value& operator[](size_t index) const;

    std::optional<int64_t> get_int(size_t index) const;
    std::optional<double> get_double(size_t index) const;
    std::optional<std::string> get_string(size_t index) const;
    std::optional<std::vector<uint8_t>> get_blob(size_t index) const;
    bool is_null(size_t index) const;

private:
    std::vector<Value> values_;
};

class PSR_API Result {
public:
    Result() = default;
    Result(std::vector<std::string> columns, std::vector<Row> rows);

    bool empty() const;
    size_t row_count() const;
    size_t column_count() const;

    const std::vector<std::string>& columns() const;
    const Row& operator[](size_t index) const;

    // Iterator support
    auto begin() const { return rows_.begin(); }
    auto end() const { return rows_.end(); }

private:
    std::vector<std::string> columns_;
    std::vector<Row> rows_;
};

}  // namespace psr

#endif  // PSR_DATABASE_RESULT_H
