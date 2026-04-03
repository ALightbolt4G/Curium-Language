#ifndef CURIUM_OPTION_H
#define CURIUM_OPTION_H

#include "curium/core.h"
#include <stddef.h>

/* ============================================================================
 * CM v2 Option Type - Some(T) | None
 * ==========================================================================*/

/* Option discriminants */
typedef enum {
    CURIUM_OPTION_NONE,
    CURIUM_OPTION_SOME
} curium_option_kind_t;

/* Option type */
typedef struct {
    curium_option_kind_t kind;
    void* value;        /* Valid only when kind == CURIUM_OPTION_SOME */
    size_t value_size;   /* Size of the value */
} curium_option_t;

/* Macros for creating options */
#define CURIUM_OPTION_NONE_INIT    {CURIUM_OPTION_NONE, NULL, 0}
#define CURIUM_OPTION_SOME_INIT(v) {CURIUM_OPTION_SOME, &(v), sizeof(v)}

/* Function declarations */

/* Create None option */
curium_option_t curium_option_none(void);

/* Create Some option */
curium_option_t curium_option_some(const void* value, size_t value_size);

/* Check if option is Some */
int curium_option_is_some(const curium_option_t* opt);

/* Check if option is None */
int curium_option_is_none(const curium_option_t* opt);

/* Get value from Some option (returns NULL if None) */
void* curium_option_unwrap(const curium_option_t* opt);

/* Get value from Some option or return default if None */
void* curium_option_unwrap_or(const curium_option_t* opt, const void* default_value, void* output, size_t output_size);

/* Map function over Some value */
curium_option_t curium_option_map(const curium_option_t* opt, void* (*mapper)(const void*), size_t result_size);

/* Free option value */
void curium_option_free(curium_option_t* opt);

/* Clone option */
curium_option_t curium_option_clone(const curium_option_t* opt);

#endif
