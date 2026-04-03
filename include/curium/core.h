/**
 * @file core.h
 * @brief Core definitions, macros, and types for the Curium library.
 */
#ifndef CURIUM_CORE_H
#define CURIUM_CORE_H

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

#define CURIUM_VERSION "3.0.0"
#define CURIUM_AUTHOR "Adham Hossam"

/**
 * @brief Standard error codes returned by CM library functions.
 */
typedef enum {
    CURIUM_SUCCESS = 0,
    CURIUM_ERROR_MEMORY = 1,
    CURIUM_ERROR_NULL_POINTER = 2,
    CURIUM_ERROR_OUT_OF_BOUNDS = 3,
    CURIUM_ERROR_DIVISION_BY_ZERO = 4,
    CURIUM_ERROR_OVERFLOW = 5,
    CURIUM_ERROR_UNDERFLOW = 6,
    CURIUM_ERROR_INVALID_ARGUMENT = 7,
    CURIUM_ERROR_NOT_FOUND = 8,
    CURIUM_ERROR_ALREADY_EXISTS = 9,
    CURIUM_ERROR_PERMISSION_DENIED = 10,
    CURIUM_ERROR_IO = 11,
    CURIUM_ERROR_NETWORK = 12,
    CURIUM_ERROR_TIMEOUT = 13,
    CURIUM_ERROR_THREAD = 14,
    CURIUM_ERROR_SYNC = 15,
    CURIUM_ERROR_PARSE = 16,
    CURIUM_ERROR_TYPE = 17,
    CURIUM_ERROR_UNIMPLEMENTED = 18,
    CURIUM_ERROR_UNKNOWN = 19,
    CURIUM_ERROR_RUNTIME = 20,
    CURIUM_ERROR_UNINITIALIZED = 21
} curium_error_code_t;

/* Base Opaque Types */
typedef struct curium_object curium_object_t;
typedef struct curium_string curium_string_t;
typedef struct curium_array  curium_array_t;
typedef struct curium_map    curium_map_t;

/**
 * @brief Memory-safe pointer handle targeting specific generations of allocations.
 */
typedef struct {
    uint32_t index;      /* Index in the internal handle table */
    uint64_t generation; /* Unique ID for the specific allocation */
} curium_ptr_t;

/**
 * @brief Sets the seed for random generation operations.
 */
void curium_random_seed(unsigned int seed);

/**
 * @brief Fills a buffer with random alphanumeric characters safely.
 * @param buffer Output buffer
 * @param length Size of the target buffer
 */
void curium_random_string(char* buffer, size_t length);

/**
 * @brief Initialize the CM library, GC, and console for UTF-8.
 */
void curium_init(void);

/**
 * @brief Shutdown the CM library and cleanup resources.
 */
void curium_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif /* CURIUM_CORE_H */
