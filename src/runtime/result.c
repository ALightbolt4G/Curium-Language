#include "curium/result.h"
#include "curium/memory.h"
#include "curium/error.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ============================================================================
 * CM v2 Result Implementation
 * ==========================================================================*/

/* Create Ok result */
curium_result_t curium_result_ok(const void* value, size_t value_size) {
    if (!value || value_size == 0) {
        curium_result_t empty = {CURIUM_RESULT_OK, NULL, NULL, 0, 0};
        return empty;
    }
    
    void* value_copy = curium_alloc(value_size, "result_value");
    if (!value_copy) {
        curium_result_t empty = {CURIUM_RESULT_ERR, NULL, NULL, 0, 0};
        return empty;
    }
    
    memcpy(value_copy, value, value_size);
    
    curium_result_t result;
    result.kind = CURIUM_RESULT_OK;
    result.value = value_copy;
    result.error = NULL;
    result.value_size = value_size;
    result.error_size = 0;
    
    return result;
}

/* Create Err result */
curium_result_t curium_result_err(const void* error, size_t error_size) {
    if (!error || error_size == 0) {
        curium_result_t empty = {CURIUM_RESULT_ERR, NULL, NULL, 0, 0};
        return empty;
    }
    
    void* error_copy = curium_alloc(error_size, "result_error");
    if (!error_copy) {
        curium_result_t empty = {CURIUM_RESULT_ERR, NULL, NULL, 0, 0};
        return empty;
    }
    
    memcpy(error_copy, error, error_size);
    
    curium_result_t result;
    result.kind = CURIUM_RESULT_ERR;
    result.value = NULL;
    result.error = error_copy;
    result.value_size = 0;
    result.error_size = error_size;
    
    return result;
}

/* Check if result is Ok */
int curium_result_is_ok(const curium_result_t* result) {
    return result && result->kind == CURIUM_RESULT_OK;
}

/* Check if result is Err */
int curium_result_is_err(const curium_result_t* result) {
    return !result || result->kind == CURIUM_RESULT_ERR;
}

/* Get value from Ok result (returns NULL if Err) */
void* curium_result_unwrap(const curium_result_t* result) {
    if (curium_result_is_err(result)) {
        return NULL;
    }
    return result->value;
}

/* Get error from Err result (returns NULL if Ok) */
void* curium_result_unwrap_err(const curium_result_t* result) {
    if (curium_result_is_ok(result)) {
        return NULL;
    }
    return result->error;
}

/* Get value from Ok result or panic if Err */
void* curium_result_expect(const curium_result_t* result, const char* message) {
    if (curium_result_is_err(result)) {
        fprintf(stderr, "\n=== RESULT EXPECTATION FAILED ===\n");
        fprintf(stderr, "Message: %s\n", message ? message : "Expected Ok but got Err");
        if (result->error) {
            fprintf(stderr, "Error: ");
            // Try to print error as string if it looks like one
            char* error_str = (char*)result->error;
            if (result->error_size > 0 && error_str[result->error_size - 1] == '\0') {
                fprintf(stderr, "%s\n", error_str);
            } else {
                fprintf(stderr, "<binary data %zu bytes>\n", result->error_size);
            }
        }
        fprintf(stderr, "=================================\n");
        abort();
    }
    return result->value;
}

/* Map function over Ok value */
curium_result_t curium_result_map(const curium_result_t* result, void* (*mapper)(const void*), size_t result_size) {
    if (curium_result_is_err(result) || !mapper) {
        return curium_result_err(result->error, result->error_size);
    }
    
    void* mapped_value = mapper(result->value);
    if (!mapped_value) {
        return curium_result_err("map function failed", 19);
    }
    
    curium_result_t mapped = curium_result_ok(mapped_value, result_size);
    curium_free(mapped_value);
    
    return mapped;
}

/* Map error */
curium_result_t curium_result_map_err(const curium_result_t* result, void* (*mapper)(const void*), size_t error_size) {
    if (curium_result_is_ok(result) || !mapper) {
        return curium_result_ok(result->value, result->value_size);
    }
    
    void* mapped_error = mapper(result->error);
    if (!mapped_error) {
        return curium_result_err("error map function failed", 26);
    }
    
    curium_result_t mapped = curium_result_err(mapped_error, error_size);
    curium_free(mapped_error);
    
    return mapped;
}

/* Chain results */
curium_result_t curium_result_and_then(const curium_result_t* result, curium_result_t (*binder)(const void*)) {
    if (curium_result_is_err(result) || !binder) {
        return curium_result_err(result->error, result->error_size);
    }
    
    return binder(result->value);
}

/* Free result */
void curium_result_free(curium_result_t* result) {
    if (!result) return;
    
    if (result->kind == CURIUM_RESULT_OK && result->value) {
        curium_free(result->value);
        result->value = NULL;
    }
    
    if (result->kind == CURIUM_RESULT_ERR && result->error) {
        curium_free(result->error);
        result->error = NULL;
    }
    
    result->kind = CURIUM_RESULT_ERR; // Set to invalid state
    result->value_size = 0;
    result->error_size = 0;
}

/* Clone result */
curium_result_t curium_result_clone(const curium_result_t* result) {
    if (curium_result_is_ok(result)) {
        return curium_result_ok(result->value, result->value_size);
    } else {
        return curium_result_err(result->error, result->error_size);
    }
}
