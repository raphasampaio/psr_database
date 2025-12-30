#include "psr_database.h"
#include "psr_internal.h"

#include <new>

extern "C" {

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

} // extern "C"
