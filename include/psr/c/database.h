#ifndef PSR_DATABASE_H
#define PSR_DATABASE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Platform-specific export macros
#ifdef _WIN32
#ifdef PSR_DATABASE_C_EXPORTS
#define PSR_C_API __declspec(dllexport)
#else
#define PSR_C_API __declspec(dllimport)
#endif
#else
#define PSR_C_API __attribute__((visibility("default")))
#endif

// Error codes
typedef enum {
    PSR_OK = 0,
    PSR_ERROR_INVALID_ARGUMENT = -1,
    PSR_ERROR_DATABASE = -2,
    PSR_ERROR_QUERY = -3,
    PSR_ERROR_NO_MEMORY = -4,
    PSR_ERROR_NOT_OPEN = -5,
    PSR_ERROR_INDEX_OUT_OF_RANGE = -6,
    PSR_ERROR_MIGRATION = -7,
} psr_error_t;

// Opaque handle types
typedef struct psr_database psr_database_t;
typedef struct psr_result psr_result_t;

// Database functions
PSR_C_API psr_database_t* psr_database_open(const char* path, psr_error_t* error);
PSR_C_API psr_database_t* psr_database_from_schema(const char* db_path, const char* schema_path, psr_error_t* error);
PSR_C_API void psr_database_close(psr_database_t* db);
PSR_C_API int psr_database_is_open(psr_database_t* db);
PSR_C_API psr_result_t* psr_database_execute(psr_database_t* db, const char* sql, psr_error_t* error);
PSR_C_API int64_t psr_database_last_insert_rowid(psr_database_t* db);
PSR_C_API int psr_database_changes(psr_database_t* db);
PSR_C_API psr_error_t psr_database_begin_transaction(psr_database_t* db);
PSR_C_API psr_error_t psr_database_commit(psr_database_t* db);
PSR_C_API psr_error_t psr_database_rollback(psr_database_t* db);
PSR_C_API const char* psr_database_error_message(psr_database_t* db);

// Migration functions
PSR_C_API int64_t psr_database_current_version(psr_database_t* db);
PSR_C_API psr_error_t psr_database_set_version(psr_database_t* db, int64_t version);
PSR_C_API psr_error_t psr_database_migrate_up(psr_database_t* db);

// Utility functions
PSR_C_API const char* psr_error_string(psr_error_t error);
PSR_C_API const char* psr_version(void);

#ifdef __cplusplus
}
#endif

#endif // PSR_DATABASE_H
