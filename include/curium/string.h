/**
 * @file string.h
 * @brief Safe string manipulation structures and functions.
 */
#ifndef CURIUM_STRING_H
#define CURIUM_STRING_H

#include "core.h"

#ifdef __cplusplus
extern "C" {
#endif

struct curium_string {
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
curium_string_t* curium_string_new(const char* initial);

/**
 * @brief free a memory-tracked string explicitly.
 */
void curium_string_free(curium_string_t* s);

/**
 * @brief obtain string length accurately.
 */
size_t curium_string_length(curium_string_t* s);

/**
 * @brief obtain string length accurately by counting UTF-8 glyphs rather than raw bytes.
 */
size_t curium_string_length_utf8(curium_string_t* s);

/**
 * @brief safely format dynamically.
 */
curium_string_t* curium_string_format(const char* format, ...);

/**
 * @brief override internal text safely.
 */
void curium_string_set(curium_string_t* s, const char* value);

/**
 * @brief append text to the string, growing buffer if needed.
 */
void curium_string_append(curium_string_t* s, const char* value);

/**
 * @brief manipulate text uppercase entirely.
 */
void curium_string_upper(curium_string_t* s);

/**
 * @brief manipulate text lowercase completely.
 */
void curium_string_lower(curium_string_t* s);

/**
 * @brief interactive secure input fetcher globally.
 */
curium_string_t* curium_input(curium_string_t* prompt);


/**
 * @brief type-safe internal print routines.
 */
void curium_print_str(curium_string_t* s);
void curium_println_str(curium_string_t* s);
void curium_print_int(int x);
void curium_println_int(int x);
void curium_print_float(double x);
void curium_println_float(double x);
void curium_print_cstr(const char* x);
void curium_println_cstr(const char* x);

/**
 * @brief overload macros for curium_print and curium_println.
 */
#define curium_print(X) _Generic((X), \
    int: curium_print_int, \
    double: curium_print_float, \
    float: curium_print_float, \
    char*: curium_print_cstr, \
    const char*: curium_print_cstr, \
    default: curium_print_str \
)(X)

#define curium_println(X) _Generic((X), \
    int: curium_println_int, \
    double: curium_println_float, \
    float: curium_println_float, \
    char*: curium_println_cstr, \
    const char*: curium_println_cstr, \
    default: curium_println_str \
)(X)

#ifdef __cplusplus
}
#endif

#endif /* CURIUM_STRING_H */
