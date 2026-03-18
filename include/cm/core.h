/**
 * @file core.h
 * @brief Core definitions, macros, and types for the CM library.
 */
#ifndef CM_CORE_H
#define CM_CORE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#ifndef _WIN32
#include <pthread.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define CM_VERSION "5.0.0"
#define CM_AUTHOR "Adham Hossam"

/**
 * @brief Standard error codes returned by CM library functions.
 */
typedef enum {
    CM_SUCCESS = 0,
    CM_ERROR_MEMORY = 1,
    CM_ERROR_NULL_POINTER = 2,
    CM_ERROR_OUT_OF_BOUNDS = 3,
    CM_ERROR_DIVISION_BY_ZERO = 4,
    CM_ERROR_OVERFLOW = 5,
    CM_ERROR_UNDERFLOW = 6,
    CM_ERROR_INVALID_ARGUMENT = 7,
    CM_ERROR_NOT_FOUND = 8,
    CM_ERROR_ALREADY_EXISTS = 9,
    CM_ERROR_PERMISSION_DENIED = 10,
    CM_ERROR_IO = 11,
    CM_ERROR_NETWORK = 12,
    CM_ERROR_TIMEOUT = 13,
    CM_ERROR_THREAD = 14,
    CM_ERROR_SYNC = 15,
    CM_ERROR_PARSE = 16,
    CM_ERROR_TYPE = 17,
    CM_ERROR_UNIMPLEMENTED = 18,
    CM_ERROR_UNKNOWN = 19
} cm_error_code_t;

/* Base Opaque Types */
typedef struct cm_object cm_object_t;
typedef struct cm_string cm_string_t;
typedef struct cm_array  cm_array_t;
typedef struct cm_map    cm_map_t;

/**
 * @brief Memory-safe pointer handle targeting specific generations of allocations.
 */
typedef struct {
    uint32_t index;      /* Index in the internal handle table */
    uint64_t generation; /* Unique ID for the specific allocation */
} cm_ptr_t;

/**
 * @brief Sets the seed for random generation operations.
 */
void cm_random_seed(unsigned int seed);

/**
 * @brief Fills a buffer with random alphanumeric characters safely.
 * @param buffer Output buffer
 * @param length Size of the target buffer
 */
void cm_random_string(char* buffer, size_t length);

/**
 * @brief Initialize the CM library, GC, and console for UTF-8.
 */
void cm_init(void);

/**
 * @brief Shutdown the CM library and cleanup resources.
 */
void cm_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif /* CM_CORE_H */
