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

// Log levels for console output
typedef enum {
    PSR_LOG_DEBUG = 0,
    PSR_LOG_INFO = 1,
    PSR_LOG_WARN = 2,
    PSR_LOG_ERROR = 3,
    PSR_LOG_OFF = 4,
} psr_log_level_t;

// Opaque handle types
typedef struct psr_database psr_database_t;
typedef struct psr_result psr_result_t;
typedef struct psr_element psr_element_t;
typedef struct psr_time_series psr_time_series_t;

// Database functions
PSR_C_API psr_database_t* psr_database_open(const char* path, psr_log_level_t console_level, psr_error_t* error);
PSR_C_API psr_database_t* psr_database_from_schema(const char* db_path, const char* schema_path,
                                                   psr_log_level_t console_level, psr_error_t* error);
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

// Element builder functions (for dynamic row creation)
PSR_C_API psr_element_t* psr_element_create(void);
PSR_C_API void psr_element_free(psr_element_t* elem);
PSR_C_API psr_error_t psr_element_set_null(psr_element_t* elem, const char* column);
PSR_C_API psr_error_t psr_element_set_int(psr_element_t* elem, const char* column, int64_t value);
PSR_C_API psr_error_t psr_element_set_double(psr_element_t* elem, const char* column, double value);
PSR_C_API psr_error_t psr_element_set_string(psr_element_t* elem, const char* column, const char* value);
PSR_C_API psr_error_t psr_element_set_blob(psr_element_t* elem, const char* column, const uint8_t* data, size_t size);

// Vector setters for element builder
PSR_C_API psr_error_t psr_element_set_int_array(psr_element_t* elem, const char* column, const int64_t* values,
                                                size_t count);
PSR_C_API psr_error_t psr_element_set_double_array(psr_element_t* elem, const char* column, const double* values,
                                                   size_t count);
PSR_C_API psr_error_t psr_element_set_string_array(psr_element_t* elem, const char* column, const char** values,
                                                   size_t count);

// Time series functions
PSR_C_API psr_time_series_t* psr_time_series_create(void);
PSR_C_API void psr_time_series_free(psr_time_series_t* ts);
PSR_C_API psr_error_t psr_time_series_add_int_column(psr_time_series_t* ts, const char* name, const int64_t* values,
                                                     size_t count);
PSR_C_API psr_error_t psr_time_series_add_double_column(psr_time_series_t* ts, const char* name, const double* values,
                                                        size_t count);
PSR_C_API psr_error_t psr_time_series_add_string_column(psr_time_series_t* ts, const char* name, const char** values,
                                                        size_t count);
PSR_C_API psr_error_t psr_element_add_time_series(psr_element_t* elem, const char* group, psr_time_series_t* ts);

// Element creation
PSR_C_API int64_t psr_database_create_element(psr_database_t* db, const char* table, psr_element_t* elem,
                                              psr_error_t* error);

// Element lookup
PSR_C_API int64_t psr_database_get_element_id(psr_database_t* db, const char* collection, const char* label,
                                              psr_error_t* error);

// Utility functions
PSR_C_API const char* psr_error_string(psr_error_t error);
PSR_C_API const char* psr_version(void);

#ifdef __cplusplus
}
#endif

#endif  // PSR_DATABASE_H
