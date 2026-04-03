/**
 * @file dynarray.h
 * @brief Dynamic array (vector) with automatic memory management.
 *
 * A safe, GC-managed dynamic array that grows automatically.
 * All operations are bounds-checked.
 */
#ifndef CURIUM_DYNARRAY_H
#define CURIUM_DYNARRAY_H

#include "core.h"
#include "memory.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Dynamic array with automatic resizing.
 */
typedef struct {
    void** data;           /* Array of pointers */
    size_t size;          /* Current number of elements */
    size_t capacity;      /* Allocated capacity */
    curium_ptr_t gc_handle;
} curium_dynarray_t;

/**
 * @brief Create a new dynamic array.
 * @param initial_capacity Initial capacity (0 for default).
 * @return New array or NULL on error.
 */
curium_dynarray_t* curium_dynarray_new(size_t initial_capacity);

/**
 * @brief Free a dynamic array.
 * @param arr The array to free.
 */
void curium_dynarray_free(curium_dynarray_t* arr);

/**
 * @brief Push element to end of array.
 * @param arr The array.
 * @param data The element to push.
 * @return 0 on success, -1 on error.
 */
int curium_dynarray_push(curium_dynarray_t* arr, void* data);

/**
 * @brief Pop element from end of array.
 * @param arr The array.
 * @return The popped element or NULL if empty.
 */
void* curium_dynarray_pop(curium_dynarray_t* arr);

/**
 * @brief Get element at index.
 * @param arr The array.
 * @param index The index.
 * @return The element or NULL if out of bounds.
 */
void* curium_dynarray_get(curium_dynarray_t* arr, size_t index);

/**
 * @brief Set element at index.
 * @param arr The array.
 * @param index The index.
 * @param data The element.
 * @return 0 on success, -1 on error.
 */
int curium_dynarray_set(curium_dynarray_t* arr, size_t index, void* data);

/**
 * @brief Insert element at index.
 * @param arr The array.
 * @param index The index to insert at.
 * @param data The element.
 * @return 0 on success, -1 on error.
 */
int curium_dynarray_insert(curium_dynarray_t* arr, size_t index, void* data);

/**
 * @brief Remove element at index.
 * @param arr The array.
 * @param index The index.
 * @return 0 on success, -1 on error.
 */
int curium_dynarray_remove(curium_dynarray_t* arr, size_t index);

/**
 * @brief Resize array to new capacity.
 * @param arr The array.
 * @param new_capacity The new capacity.
 * @return 0 on success, -1 on error.
 */
int curium_dynarray_resize(curium_dynarray_t* arr, size_t new_capacity);

/**
 * @brief Get array size.
 * @param arr The array.
 * @return Number of elements.
 */
size_t curium_dynarray_size(curium_dynarray_t* arr);

/**
 * @brief Get array capacity.
 * @param arr The array.
 * @return Current capacity.
 */
size_t curium_dynarray_capacity(curium_dynarray_t* arr);

/**
 * @brief Check if array is empty.
 * @param arr The array.
 * @return 1 if empty, 0 otherwise.
 */
int curium_dynarray_empty(curium_dynarray_t* arr);

/**
 * @brief Clear all elements.
 * @param arr The array.
 */
void curium_dynarray_clear(curium_dynarray_t* arr);

/**
 * @brief Find index of element.
 * @param arr The array.
 * @param data The element to find (by pointer).
 * @return Index or -1 if not found.
 */
long curium_dynarray_index_of(curium_dynarray_t* arr, void* data);

/**
 * @brief Check if array contains element.
 * @param arr The array.
 * @param data The element.
 * @return 1 if found, 0 otherwise.
 */
int curium_dynarray_contains(curium_dynarray_t* arr, void* data);

/**
 * @brief Iterate over array.
 * @param arr The array.
 * @param callback Function called for each element.
 * @param userdata Optional userdata.
 */
typedef void (*curium_dynarray_iter_cb)(void* data, size_t index, void* userdata);
void curium_dynarray_foreach(curium_dynarray_t* arr, curium_dynarray_iter_cb callback, void* userdata);

#ifdef __cplusplus
}
#endif

#endif /* CURIUM_DYNARRAY_H */
