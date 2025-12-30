#ifndef PSR_RESULT_H
#define PSR_RESULT_H

#include "psr_database.h"

#ifdef __cplusplus
extern "C" {
#endif

// Value types
typedef enum {
    PSR_TYPE_NULL = 0,
    PSR_TYPE_INTEGER = 1,
    PSR_TYPE_FLOAT = 2,
    PSR_TYPE_TEXT = 3,
    PSR_TYPE_BLOB = 4,
} psr_value_type_t;

// Result functions
PSR_C_API void psr_result_free(psr_result_t* result);
PSR_C_API size_t psr_result_row_count(psr_result_t* result);
PSR_C_API size_t psr_result_column_count(psr_result_t* result);
PSR_C_API const char* psr_result_column_name(psr_result_t* result, size_t col);
PSR_C_API psr_value_type_t psr_result_get_type(psr_result_t* result, size_t row, size_t col);
PSR_C_API int psr_result_is_null(psr_result_t* result, size_t row, size_t col);
PSR_C_API psr_error_t psr_result_get_int(psr_result_t* result, size_t row, size_t col, int64_t* value);
PSR_C_API psr_error_t psr_result_get_double(psr_result_t* result, size_t row, size_t col, double* value);
PSR_C_API const char* psr_result_get_string(psr_result_t* result, size_t row, size_t col);
PSR_C_API const uint8_t* psr_result_get_blob(psr_result_t* result, size_t row, size_t col, size_t* size);

#ifdef __cplusplus
}
#endif

#endif // PSR_RESULT_H
