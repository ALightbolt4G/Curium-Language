#ifndef CURIUM_ERROR_DETAIL_H
#define CURIUM_ERROR_DETAIL_H

#include "curium/core.h"
#include "curium/error.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Enhanced Error Detail System
 * Provides detailed error information with context and suggestions
 * ========================================================================== */

typedef enum {
    CURIUM_SEVERITY_WARNING,
    CURIUM_SEVERITY_ERROR,
    CURIUM_SEVERITY_FATAL
} curium_error_severity_t;

typedef struct {
    curium_error_code_t code;
    curium_error_severity_t severity;
    char file[256];
    int line;
    int column;
    char object_name[128];
    char object_type[64];
    char message[512];
    char suggestion[256];
    
    /* Context lines for syntax errors */
    char context_before[3][256];
    char context_line[256];
    char context_after[3][256];
    int context_count;
} curium_error_detail_t;

/* Initialize error detail with defaults */
void curium_error_detail_init(curium_error_detail_t* detail);

/* Set basic error info */
void curium_error_detail_set_location(curium_error_detail_t* detail, const char* file, int line, int column);
void curium_error_detail_set_object(curium_error_detail_t* detail, const char* type, const char* name);
void curium_error_detail_set_message(curium_error_detail_t* detail, const char* fmt, ...);
void curium_error_detail_set_suggestion(curium_error_detail_t* detail, const char* fmt, ...);

/* Set context lines for syntax errors */
void curium_error_detail_set_context(curium_error_detail_t* detail, 
                                  const char* before[3], int before_count,
                                  const char* current,
                                  const char* after[3], int after_count);

/* Print formatted error to stderr */
void curium_error_detail_print(const curium_error_detail_t* detail);

/* Print syntax error with line numbers and caret */
void curium_error_detail_print_syntax(const curium_error_detail_t* detail);

/* Print runtime error with stack trace */
void curium_error_detail_print_runtime(const curium_error_detail_t* detail);

/* Convert to JSON string */
curium_string_t* curium_error_detail_to_json(const curium_error_detail_t* detail);

/* Current error detail (thread-local) */
curium_error_detail_t* curium_error_detail_current(void);
void curium_error_detail_clear(void);

#ifdef __cplusplus
}
#endif

#endif /* CURIUM_ERROR_DETAIL_H */
