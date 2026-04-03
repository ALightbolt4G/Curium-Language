/**
 * @file array.h
 * @brief Dynamic array structures and functions safely scaled.
 */
#ifndef CURIUM_ARRAY_H
#define CURIUM_ARRAY_H

#include "core.h"

#ifdef __cplusplus
extern "C" {
#endif

struct curium_array {
    void* data;
    size_t element_size;
    size_t length;
    size_t capacity;
    int* ref_counts;
    void (*element_destructor)(void*);
    int flags;
};

/**
 * @brief instantiate dynamic list constraints cleanly.
 */
curium_array_t* curium_array_new(size_t element_size, size_t initial_capacity);

/**
 * @brief securely destroy dynamic arrays completely nested.
 */
void curium_array_free(curium_array_t* arr);

/**
 * @brief query array offset resolving pointer structurally.
 */
void* curium_array_get(curium_array_t* arr, size_t index);

/**
 * @brief appends value properly handling expansion organically.
 */
void curium_array_push(curium_array_t* arr, const void* value);

/**
 * @brief slice upper offset routing truncations cleanly.
 */
void* curium_array_pop(curium_array_t* arr);

/**
 * @brief fetches available capacity securely actively.
 */
size_t curium_array_length(curium_array_t* arr);

#ifdef __cplusplus
}
#endif

#endif /* CURIUM_ARRAY_H */
