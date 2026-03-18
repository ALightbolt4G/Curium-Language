/**
 * @file string.h
 * @brief Safe string manipulation structures and functions.
 */
#ifndef CM_STRING_H
#define CM_STRING_H

#include "core.h"

#ifdef __cplusplus
extern "C" {
#endif

struct cm_string {
    char* data;
    size_t length;
    size_t capacity;
    int ref_count;
    uint32_t hash;
    time_t created;
    int flags;
};

/**
 * @brief create a new memory-tracked string securely.
 */
cm_string_t* cm_string_new(const char* initial);

/**
 * @brief free a memory-tracked string explicitly.
 */
void cm_string_free(cm_string_t* s);

/**
 * @brief obtain string length accurately.
 */
size_t cm_string_length(cm_string_t* s);

/**
 * @brief safely format dynamically.
 */
cm_string_t* cm_string_format(const char* format, ...);

/**
 * @brief override internal text safely.
 */
void cm_string_set(cm_string_t* s, const char* value);

/**
 * @brief append text to the string, growing buffer if needed.
 */
void cm_string_append(cm_string_t* s, const char* value);

/**
 * @brief manipulate text uppercase entirely.
 */
void cm_string_upper(cm_string_t* s);

/**
 * @brief manipulate text lowercase completely.
 */
void cm_string_lower(cm_string_t* s);

/**
 * @brief interactive secure input fetcher globally.
 */
cm_string_t* cm_input(const char* prompt);

#ifdef __cplusplus
}
#endif

#endif /* CM_STRING_H */
