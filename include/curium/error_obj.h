#ifndef CURIUM_ERROR_OBJ_H
#define CURIUM_ERROR_OBJ_H

#include "curium/core.h"
#include "curium/error.h"
#include "curium/error_detail.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Error Object API
 * Programmatic error handling with rich error information
 * ========================================================================== */

typedef struct curium_error_obj curium_error_obj_t;

/* Create error object from detail */
curium_error_obj_t* curium_error_obj_create(const curium_error_detail_t* detail);
curium_error_obj_t* curium_error_obj_create_simple(curium_error_code_t code, const char* message);

/* Getters */
curium_error_code_t curium_error_obj_get_code(const curium_error_obj_t* err);
const char* curium_error_obj_get_message(const curium_error_obj_t* err);
const char* curium_error_obj_get_file(const curium_error_obj_t* err);
int curium_error_obj_get_line(const curium_error_obj_t* err);
int curium_error_obj_get_column(const curium_error_obj_t* err);
const char* curium_error_obj_get_object_name(const curium_error_obj_t* err);
const char* curium_error_obj_get_object_type(const curium_error_obj_t* err);
const char* curium_error_obj_get_suggestion(const curium_error_obj_t* err);
curium_error_severity_t curium_error_obj_get_severity(const curium_error_obj_t* err);

/* Check if error matches code */
int curium_error_obj_is(const curium_error_obj_t* err, curium_error_code_t code);

/* Print error */
void curium_error_obj_print(const curium_error_obj_t* err);

/* Convert to JSON */
curium_string_t* curium_error_obj_to_json(const curium_error_obj_t* err);

/* Free error object */
void curium_error_obj_free(curium_error_obj_t* err);

/* Error chain (multiple errors) */
typedef struct curium_error_chain curium_error_chain_t;

curium_error_chain_t* curium_error_chain_create(void);
void curium_error_chain_add(curium_error_chain_t* chain, curium_error_obj_t* err);
int curium_error_chain_count(const curium_error_chain_t* chain);
curium_error_obj_t* curium_error_chain_get(const curium_error_chain_t* chain, int index);
void curium_error_chain_print_all(const curium_error_chain_t* chain);
void curium_error_chain_free(curium_error_chain_t* chain);

#ifdef __cplusplus
}
#endif

#endif /* CURIUM_ERROR_OBJ_H */
