#include "psr_result.h"
#include "psr_database/result.h"

#include <string>
#include <vector>

// Internal struct definition
struct psr_result {
    psr::Result result;
    explicit psr_result(psr::Result r) : result(std::move(r)) {}
};

extern "C" {

PSR_C_API void psr_result_free(psr_result_t* result) {
    delete result;
}

PSR_C_API size_t psr_result_row_count(psr_result_t* result) {
    if (!result)
        return 0;
    return result->result.row_count();
}

PSR_C_API size_t psr_result_column_count(psr_result_t* result) {
    if (!result)
        return 0;
    return result->result.column_count();
}

PSR_C_API const char* psr_result_column_name(psr_result_t* result, size_t col) {
    if (!result || col >= result->result.column_count())
        return nullptr;
    return result->result.columns()[col].c_str();
}

PSR_C_API psr_value_type_t psr_result_get_type(psr_result_t* result, size_t row, size_t col) {
    if (!result || row >= result->result.row_count() || col >= result->result.column_count()) {
        return PSR_TYPE_NULL;
    }

    const auto& value = result->result[row][col];
    if (std::holds_alternative<std::nullptr_t>(value)) {
        return PSR_TYPE_NULL;
    } else if (std::holds_alternative<int64_t>(value)) {
        return PSR_TYPE_INTEGER;
    } else if (std::holds_alternative<double>(value)) {
        return PSR_TYPE_FLOAT;
    } else if (std::holds_alternative<std::string>(value)) {
        return PSR_TYPE_TEXT;
    } else if (std::holds_alternative<std::vector<uint8_t>>(value)) {
        return PSR_TYPE_BLOB;
    }
    return PSR_TYPE_NULL;
}

PSR_C_API int psr_result_is_null(psr_result_t* result, size_t row, size_t col) {
    if (!result || row >= result->result.row_count() || col >= result->result.column_count()) {
        return 1;
    }
    return result->result[row].is_null(col) ? 1 : 0;
}

PSR_C_API psr_error_t psr_result_get_int(psr_result_t* result, size_t row, size_t col,
                                         int64_t* value) {
    if (!result || !value)
        return PSR_ERROR_INVALID_ARGUMENT;
    if (row >= result->result.row_count() || col >= result->result.column_count()) {
        return PSR_ERROR_INDEX_OUT_OF_RANGE;
    }

    auto opt = result->result[row].get_int(col);
    if (!opt)
        return PSR_ERROR_INVALID_ARGUMENT;
    *value = *opt;
    return PSR_OK;
}

PSR_C_API psr_error_t psr_result_get_double(psr_result_t* result, size_t row, size_t col,
                                            double* value) {
    if (!result || !value)
        return PSR_ERROR_INVALID_ARGUMENT;
    if (row >= result->result.row_count() || col >= result->result.column_count()) {
        return PSR_ERROR_INDEX_OUT_OF_RANGE;
    }

    auto opt = result->result[row].get_double(col);
    if (!opt)
        return PSR_ERROR_INVALID_ARGUMENT;
    *value = *opt;
    return PSR_OK;
}

PSR_C_API const char* psr_result_get_string(psr_result_t* result, size_t row, size_t col) {
    if (!result || row >= result->result.row_count() || col >= result->result.column_count()) {
        return nullptr;
    }

    const auto& value = result->result[row][col];
    if (const auto* str = std::get_if<std::string>(&value)) {
        return str->c_str();
    }
    return nullptr;
}

PSR_C_API const uint8_t* psr_result_get_blob(psr_result_t* result, size_t row, size_t col,
                                             size_t* size) {
    if (!result || row >= result->result.row_count() || col >= result->result.column_count()) {
        if (size)
            *size = 0;
        return nullptr;
    }

    const auto& value = result->result[row][col];
    if (const auto* blob = std::get_if<std::vector<uint8_t>>(&value)) {
        if (size)
            *size = blob->size();
        return blob->data();
    }
    if (size)
        *size = 0;
    return nullptr;
}

} // extern "C"
