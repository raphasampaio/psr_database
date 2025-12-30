#include "psr/c/database.h"
#include "psr/c/result.h"
#include "psr/database.h"
#include "psr/result.h"

#include <new>
#include <string>
#include <vector>

// Internal struct definitions
struct psr_database {
    psr::Database db;
    std::string last_error;
    explicit psr_database(const std::string& path) : db(path) {}
};

struct psr_result {
    psr::Result result;
    explicit psr_result(psr::Result r) : result(std::move(r)) {}
};

extern "C" {

// Database functions

PSR_C_API psr_database_t* psr_database_open(const char* path, psr_error_t* error) {
    if (!path) {
        if (error)
            *error = PSR_ERROR_INVALID_ARGUMENT;
        return nullptr;
    }

    try {
        auto* db = new psr_database(path);
        if (error)
            *error = PSR_OK;
        return db;
    } catch (const std::bad_alloc&) {
        if (error)
            *error = PSR_ERROR_NO_MEMORY;
        return nullptr;
    } catch (const std::exception&) {
        if (error)
            *error = PSR_ERROR_DATABASE;
        return nullptr;
    }
}

PSR_C_API void psr_database_close(psr_database_t* db) {
    delete db;
}

PSR_C_API int psr_database_is_open(psr_database_t* db) {
    if (!db)
        return 0;
    return db->db.is_open() ? 1 : 0;
}

PSR_C_API psr_result_t* psr_database_execute(psr_database_t* db, const char* sql,
                                             psr_error_t* error) {
    if (!db) {
        if (error)
            *error = PSR_ERROR_INVALID_ARGUMENT;
        return nullptr;
    }
    if (!sql) {
        if (error)
            *error = PSR_ERROR_INVALID_ARGUMENT;
        return nullptr;
    }

    try {
        auto result = db->db.execute(sql);
        auto* res = new psr_result(std::move(result));
        if (error)
            *error = PSR_OK;
        return res;
    } catch (const std::bad_alloc&) {
        if (error)
            *error = PSR_ERROR_NO_MEMORY;
        db->last_error = "Out of memory";
        return nullptr;
    } catch (const std::exception& e) {
        if (error)
            *error = PSR_ERROR_QUERY;
        db->last_error = e.what();
        return nullptr;
    }
}

PSR_C_API int64_t psr_database_last_insert_rowid(psr_database_t* db) {
    if (!db)
        return 0;
    return db->db.last_insert_rowid();
}

PSR_C_API int psr_database_changes(psr_database_t* db) {
    if (!db)
        return 0;
    return db->db.changes();
}

PSR_C_API psr_error_t psr_database_begin_transaction(psr_database_t* db) {
    if (!db)
        return PSR_ERROR_INVALID_ARGUMENT;
    try {
        db->db.begin_transaction();
        return PSR_OK;
    } catch (const std::exception& e) {
        db->last_error = e.what();
        return PSR_ERROR_QUERY;
    }
}

PSR_C_API psr_error_t psr_database_commit(psr_database_t* db) {
    if (!db)
        return PSR_ERROR_INVALID_ARGUMENT;
    try {
        db->db.commit();
        return PSR_OK;
    } catch (const std::exception& e) {
        db->last_error = e.what();
        return PSR_ERROR_QUERY;
    }
}

PSR_C_API psr_error_t psr_database_rollback(psr_database_t* db) {
    if (!db)
        return PSR_ERROR_INVALID_ARGUMENT;
    try {
        db->db.rollback();
        return PSR_OK;
    } catch (const std::exception& e) {
        db->last_error = e.what();
        return PSR_ERROR_QUERY;
    }
}

PSR_C_API const char* psr_database_error_message(psr_database_t* db) {
    if (!db)
        return "Invalid database handle";
    if (!db->last_error.empty()) {
        return db->last_error.c_str();
    }
    return db->db.error_message().c_str();
}

PSR_C_API const char* psr_error_string(psr_error_t error) {
    switch (error) {
    case PSR_OK:
        return "Success";
    case PSR_ERROR_INVALID_ARGUMENT:
        return "Invalid argument";
    case PSR_ERROR_DATABASE:
        return "Database error";
    case PSR_ERROR_QUERY:
        return "Query error";
    case PSR_ERROR_NO_MEMORY:
        return "Out of memory";
    case PSR_ERROR_NOT_OPEN:
        return "Database not open";
    case PSR_ERROR_INDEX_OUT_OF_RANGE:
        return "Index out of range";
    default:
        return "Unknown error";
    }
}

PSR_C_API const char* psr_version(void) {
    return PSR_VERSION;
}

// Result functions

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
