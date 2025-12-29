#include "psr_database/result.h"

#include <stdexcept>

namespace psr {

Row::Row(std::vector<Value> values) : values_(std::move(values)) {}

size_t Row::column_count() const {
    return values_.size();
}

const Value& Row::operator[](size_t index) const {
    if (index >= values_.size()) {
        throw std::out_of_range("Column index out of range");
    }
    return values_[index];
}

std::optional<int64_t> Row::get_int(size_t index) const {
    if (index >= values_.size() || is_null(index)) {
        return std::nullopt;
    }
    if (auto* val = std::get_if<int64_t>(&values_[index])) {
        return *val;
    }
    return std::nullopt;
}

std::optional<double> Row::get_double(size_t index) const {
    if (index >= values_.size() || is_null(index)) {
        return std::nullopt;
    }
    if (auto* val = std::get_if<double>(&values_[index])) {
        return *val;
    }
    return std::nullopt;
}

std::optional<std::string> Row::get_string(size_t index) const {
    if (index >= values_.size() || is_null(index)) {
        return std::nullopt;
    }
    if (auto* val = std::get_if<std::string>(&values_[index])) {
        return *val;
    }
    return std::nullopt;
}

std::optional<std::vector<uint8_t>> Row::get_blob(size_t index) const {
    if (index >= values_.size() || is_null(index)) {
        return std::nullopt;
    }
    if (auto* val = std::get_if<std::vector<uint8_t>>(&values_[index])) {
        return *val;
    }
    return std::nullopt;
}

bool Row::is_null(size_t index) const {
    if (index >= values_.size()) {
        return true;
    }
    return std::holds_alternative<std::nullptr_t>(values_[index]);
}

Result::Result(std::vector<std::string> columns, std::vector<Row> rows)
    : columns_(std::move(columns)), rows_(std::move(rows)) {}

bool Result::empty() const {
    return rows_.empty();
}

size_t Result::row_count() const {
    return rows_.size();
}

size_t Result::column_count() const {
    return columns_.size();
}

const std::vector<std::string>& Result::columns() const {
    return columns_;
}

const Row& Result::operator[](size_t index) const {
    if (index >= rows_.size()) {
        throw std::out_of_range("Row index out of range");
    }
    return rows_[index];
}

}  // namespace psr
