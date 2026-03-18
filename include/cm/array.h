/**
 * @file array.h
 * @brief Dynamic array structures and functions safely scaled.
 */
#ifndef CM_ARRAY_H
#define CM_ARRAY_H

#include "core.h"

#ifdef __cplusplus
extern "C" {
#endif

struct cm_array {
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
cm_array_t* cm_array_new(size_t element_size, size_t initial_capacity);

/**
 * @brief securely destroy dynamic arrays completely nested.
 */
void cm_array_free(cm_array_t* arr);

/**
 * @brief query array offset resolving pointer structurally.
 */
void* cm_array_get(cm_array_t* arr, size_t index);

/**
 * @brief appends value properly handling expansion organically.
 */
void cm_array_push(cm_array_t* arr, const void* value);

/**
 * @brief slice upper offset routing truncations cleanly.
 */
void* cm_array_pop(cm_array_t* arr);

/**
 * @brief fetches available capacity securely actively.
 */
size_t cm_array_length(cm_array_t* arr);

#ifdef __cplusplus
}
#endif

#endif /* CM_ARRAY_H */
