/**
 * @file list.h
 * @brief Doubly linked list with automatic memory management.
 *
 * A safe, GC-managed doubly linked list implementation.
 * All operations are bounds-checked and memory-safe.
 */
#ifndef CURIUM_LIST_H
#define CURIUM_LIST_H

#include "core.h"
#include "memory.h"
#include "string.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct curium_list_node {
    void* data;
    struct curium_list_node* next;
    struct curium_list_node* prev;
} curium_list_node_t;

/**
 * @brief Doubly linked list with GC integration.
 */
typedef struct {
    curium_list_node_t* head;
    curium_list_node_t* tail;
    size_t count;
    curium_ptr_t gc_handle;
} curium_list_t;

/**
 * @brief Create a new empty list.
 * @return New list instance or NULL on error.
 */
curium_list_t* curium_list_new(void);

/**
 * @brief Free a list and all its nodes.
 * @param list The list to free.
 */
void curium_list_free(curium_list_t* list);

/**
 * @brief Append data to end of list.
 * @param list The list.
 * @param data The data to append.
 * @return 0 on success, -1 on error.
 */
int curium_list_append(curium_list_t* list, void* data);

/**
 * @brief Prepend data to beginning of list.
 * @param list The list.
 * @param data The data to prepend.
 * @return 0 on success, -1 on error.
 */
int curium_list_prepend(curium_list_t* list, void* data);

/**
 * @brief Remove first occurrence of data from list.
 * @param list The list.
 * @param data The data to remove (compares by pointer).
 * @return 0 if found and removed, -1 if not found.
 */
int curium_list_remove(curium_list_t* list, void* data);

/**
 * @brief Remove node at index.
 * @param list The list.
 * @param index The index to remove.
 * @return 0 on success, -1 on error.
 */
int curium_list_remove_at(curium_list_t* list, size_t index);

/**
 * @brief Get data at index.
 * @param list The list.
 * @param index The index.
 * @return The data or NULL if out of bounds.
 */
void* curium_list_get(curium_list_t* list, size_t index);

/**
 * @brief Set data at index.
 * @param list The list.
 * @param index The index.
 * @param data The new data.
 * @return 0 on success, -1 on error.
 */
int curium_list_set(curium_list_t* list, size_t index, void* data);

/**
 * @brief Find index of data in list.
 * @param list The list.
 * @param data The data to find (compares by pointer).
 * @return Index or -1 if not found.
 */
long curium_list_index_of(curium_list_t* list, void* data);

/**
 * @brief Check if list contains data.
 * @param list The list.
 * @param data The data to check.
 * @return 1 if found, 0 otherwise.
 */
int curium_list_contains(curium_list_t* list, void* data);

/**
 * @brief Get list size.
 * @param list The list.
 * @return Number of elements.
 */
size_t curium_list_size(curium_list_t* list);

/**
 * @brief Check if list is empty.
 * @param list The list.
 * @return 1 if empty, 0 otherwise.
 */
int curium_list_empty(curium_list_t* list);

/**
 * @brief Clear all elements from list.
 * @param list The list.
 */
void curium_list_clear(curium_list_t* list);

/**
 * @brief Iterate over list with callback.
 * @param list The list.
 * @param callback Function called for each element.
 * @param userdata Optional userdata.
 */
typedef void (*curium_list_iter_cb)(void* data, size_t index, void* userdata);
void curium_list_foreach(curium_list_t* list, curium_list_iter_cb callback, void* userdata);

/**
 * @brief Print list contents (for debugging).
 * @param list The list.
 * @param to_string Optional function to convert data to string.
 */
typedef curium_string_t* (*curium_list_to_string_fn)(void* data);
void curium_list_print(curium_list_t* list, curium_list_to_string_fn to_string);

#ifdef __cplusplus
}
#endif

#endif /* CURIUM_LIST_H */
