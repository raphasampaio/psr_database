#ifndef PSR_DATABASE_C_H
#define PSR_DATABASE_C_H

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

// Opaque handle types
typedef struct psr_database psr_database_t;
typedef struct psr_result psr_result_t;

// Error codes
typedef enum {
    PSR_OK = 0,
    PSR_ERROR_INVALID_ARGUMENT = -1,
    PSR_ERROR_DATABASE = -2,
    PSR_ERROR_QUERY = -3,
    PSR_ERROR_NO_MEMORY = -4,
    PSR_ERROR_NOT_OPEN = -5,
    PSR_ERROR_INDEX_OUT_OF_RANGE = -6,
} psr_error_t;

// Value types
typedef enum {
    PSR_TYPE_NULL = 0,
    PSR_TYPE_INTEGER = 1,
    PSR_TYPE_FLOAT = 2,
    PSR_TYPE_TEXT = 3,
    PSR_TYPE_BLOB = 4,
} psr_value_type_t;

// ============================================================================
// Database Functions
// ============================================================================

/**
 * Open a database connection.
 * @param path Path to the database file (use ":memory:" for in-memory database)
 * @param error Output parameter for error code (can be NULL)
 * @return Database handle or NULL on error
 */
PSR_C_API psr_database_t* psr_database_open(const char* path, psr_error_t* error);

/**
 * Close a database connection.
 * @param db Database handle
 */
PSR_C_API void psr_database_close(psr_database_t* db);

/**
 * Check if database is open.
 * @param db Database handle
 * @return 1 if open, 0 if not
 */
PSR_C_API int psr_database_is_open(psr_database_t* db);

/**
 * Execute a SQL statement.
 * @param db Database handle
 * @param sql SQL statement
 * @param error Output parameter for error code (can be NULL)
 * @return Result handle or NULL on error (caller must free with psr_result_free)
 */
PSR_C_API psr_result_t* psr_database_execute(psr_database_t* db, const char* sql,
                                              psr_error_t* error);

/**
 * Get the rowid of the last inserted row.
 * @param db Database handle
 * @return Last insert rowid or 0 if not applicable
 */
PSR_C_API int64_t psr_database_last_insert_rowid(psr_database_t* db);

/**
 * Get the number of rows changed by the last statement.
 * @param db Database handle
 * @return Number of changed rows
 */
PSR_C_API int psr_database_changes(psr_database_t* db);

/**
 * Begin a transaction.
 * @param db Database handle
 * @return PSR_OK on success, error code otherwise
 */
PSR_C_API psr_error_t psr_database_begin_transaction(psr_database_t* db);

/**
 * Commit the current transaction.
 * @param db Database handle
 * @return PSR_OK on success, error code otherwise
 */
PSR_C_API psr_error_t psr_database_commit(psr_database_t* db);

/**
 * Rollback the current transaction.
 * @param db Database handle
 * @return PSR_OK on success, error code otherwise
 */
PSR_C_API psr_error_t psr_database_rollback(psr_database_t* db);

/**
 * Get the last error message.
 * @param db Database handle
 * @return Error message string (do not free)
 */
PSR_C_API const char* psr_database_error_message(psr_database_t* db);

// ============================================================================
// Result Functions
// ============================================================================

/**
 * Free a result handle.
 * @param result Result handle
 */
PSR_C_API void psr_result_free(psr_result_t* result);

/**
 * Get the number of rows in the result.
 * @param result Result handle
 * @return Number of rows
 */
PSR_C_API size_t psr_result_row_count(psr_result_t* result);

/**
 * Get the number of columns in the result.
 * @param result Result handle
 * @return Number of columns
 */
PSR_C_API size_t psr_result_column_count(psr_result_t* result);

/**
 * Get a column name.
 * @param result Result handle
 * @param col Column index (0-based)
 * @return Column name or NULL if index is out of range
 */
PSR_C_API const char* psr_result_column_name(psr_result_t* result, size_t col);

/**
 * Get the type of a value.
 * @param result Result handle
 * @param row Row index (0-based)
 * @param col Column index (0-based)
 * @return Value type
 */
PSR_C_API psr_value_type_t psr_result_get_type(psr_result_t* result, size_t row, size_t col);

/**
 * Check if a value is null.
 * @param result Result handle
 * @param row Row index (0-based)
 * @param col Column index (0-based)
 * @return 1 if null, 0 otherwise
 */
PSR_C_API int psr_result_is_null(psr_result_t* result, size_t row, size_t col);

/**
 * Get an integer value.
 * @param result Result handle
 * @param row Row index (0-based)
 * @param col Column index (0-based)
 * @param value Output parameter for the value
 * @return PSR_OK on success, error code otherwise
 */
PSR_C_API psr_error_t psr_result_get_int(psr_result_t* result, size_t row, size_t col,
                                          int64_t* value);

/**
 * Get a double value.
 * @param result Result handle
 * @param row Row index (0-based)
 * @param col Column index (0-based)
 * @param value Output parameter for the value
 * @return PSR_OK on success, error code otherwise
 */
PSR_C_API psr_error_t psr_result_get_double(psr_result_t* result, size_t row, size_t col,
                                             double* value);

/**
 * Get a string value.
 * @param result Result handle
 * @param row Row index (0-based)
 * @param col Column index (0-based)
 * @return String value or NULL if not a string or index out of range (do not free)
 */
PSR_C_API const char* psr_result_get_string(psr_result_t* result, size_t row, size_t col);

/**
 * Get a blob value.
 * @param result Result handle
 * @param row Row index (0-based)
 * @param col Column index (0-based)
 * @param size Output parameter for blob size
 * @return Pointer to blob data or NULL if not a blob (do not free)
 */
PSR_C_API const uint8_t* psr_result_get_blob(psr_result_t* result, size_t row, size_t col,
                                              size_t* size);

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * Get the error message for an error code.
 * @param error Error code
 * @return Error message string (do not free)
 */
PSR_C_API const char* psr_error_string(psr_error_t error);

/**
 * Get the library version.
 * @return Version string (do not free)
 */
PSR_C_API const char* psr_version(void);

#ifdef __cplusplus
}
#endif

#endif  // PSR_DATABASE_C_H
