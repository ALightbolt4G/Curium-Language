#ifndef CURIUM_RESULT_H
#define CURIUM_RESULT_H

#include "curium/core.h"
#include <stddef.h>

/* ============================================================================
 * CM v2 Result Type - Ok(T) | Err(E)
 * ==========================================================================*/

/* Result discriminants */
typedef enum {
    CURIUM_RESULT_OK,
    CURIUM_RESULT_ERR
} curium_result_kind_t;

/* Result type */
typedef struct {
    curium_result_kind_t kind;
    void* value;        /* Valid only when kind == CURIUM_RESULT_OK */
    void* error;        /* Valid only when kind == CURIUM_RESULT_ERR */
    size_t value_size;   /* Size of the value */
    size_t error_size;   /* Size of the error */
} curium_result_t;

/* Macros for creating results */
#define CURIUM_RESULT_OK_INIT(v)    {CURIUM_RESULT_OK, &(v), NULL, sizeof(v), 0}
#define CURIUM_RESULT_ERR_INIT(e)   {CURIUM_RESULT_ERR, NULL, &(e), 0, sizeof(e)}

/* Function declarations */

/* Create Ok result */
curium_result_t curium_result_ok(const void* value, size_t value_size);

/* Create Err result */
curium_result_t curium_result_err(const void* error, size_t error_size);

/* Check if result is Ok */
int curium_result_is_ok(const curium_result_t* result);

/* Check if result is Err */
int curium_result_is_err(const curium_result_t* result);

/* Get value from Ok result (returns NULL if Err) */
void* curium_result_unwrap(const curium_result_t* result);

/* Get error from Err result (returns NULL if Ok) */
void* curium_result_unwrap_err(const curium_result_t* result);

/* Get value from Ok result or panic if Err */
void* curium_result_expect(const curium_result_t* result, const char* message);

/* Map function over Ok value */
curium_result_t curium_result_map(const curium_result_t* result, void* (*mapper)(const void*), size_t result_size);

/* Map error */
curium_result_t curium_result_map_err(const curium_result_t* result, void* (*mapper)(const void*), size_t error_size);

/* Chain results */
curium_result_t curium_result_and_then(const curium_result_t* result, curium_result_t (*binder)(const void*));

/* Free result */
void curium_result_free(curium_result_t* result);

/* Clone result */
curium_result_t curium_result_clone(const curium_result_t* result);

#endif
