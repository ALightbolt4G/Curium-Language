#include "curium/option.h"
#include "curium/memory.h"
#include <string.h>
#include <stdlib.h>

/* ============================================================================
 * CM v2 Option Implementation
 * ==========================================================================*/

/* Create None option */
curium_option_t curium_option_none(void) {
    curium_option_t opt = CURIUM_OPTION_NONE_INIT;
    return opt;
}

/* Create Some option */
curium_option_t curium_option_some(const void* value, size_t value_size) {
    if (!value || value_size == 0) {
        return curium_option_none();
    }
    
    void* value_copy = curium_alloc(value_size, "option_value");
    if (!value_copy) {
        return curium_option_none();
    }
    
    memcpy(value_copy, value, value_size);
    
    curium_option_t opt;
    opt.kind = CURIUM_OPTION_SOME;
    opt.value = value_copy;
    opt.value_size = value_size;
    
    return opt;
}

/* Check if option is Some */
int curium_option_is_some(const curium_option_t* opt) {
    return opt && opt->kind == CURIUM_OPTION_SOME;
}

/* Check if option is None */
int curium_option_is_none(const curium_option_t* opt) {
    return !opt || opt->kind == CURIUM_OPTION_NONE;
}

/* Get value from Some option (returns NULL if None) */
void* curium_option_unwrap(const curium_option_t* opt) {
    if (curium_option_is_none(opt)) {
        return NULL;
    }
    return opt->value;
}

/* Get value from Some option or return default if None */
void* curium_option_unwrap_or(const curium_option_t* opt, const void* default_value, void* output, size_t output_size) {
    if (curium_option_is_some(opt)) {
        if (output && opt->value) {
            memcpy(output, opt->value, output_size < opt->value_size ? output_size : opt->value_size);
        }
        return output ? output : opt->value;
    }
    
    if (output && default_value) {
        memcpy(output, default_value, output_size);
    }
    return output;
}

/* Map function over Some value */
curium_option_t curium_option_map(const curium_option_t* opt, void* (*mapper)(const void*), size_t result_size) {
    if (curium_option_is_none(opt) || !mapper) {
        return curium_option_none();
    }
    
    void* result = mapper(opt->value);
    if (!result) {
        return curium_option_none();
    }
    
    curium_option_t mapped = curium_option_some(result, result_size);
    curium_free(result);
    
    return mapped;
}

/* Free option value */
void curium_option_free(curium_option_t* opt) {
    if (!opt) return;
    
    if (opt->kind == CURIUM_OPTION_SOME && opt->value) {
        curium_free(opt->value);
        opt->value = NULL;
    }
    
    opt->kind = CURIUM_OPTION_NONE;
    opt->value_size = 0;
}

/* Clone option */
curium_option_t curium_option_clone(const curium_option_t* opt) {
    if (curium_option_is_none(opt)) {
        return curium_option_none();
    }
    
    return curium_option_some(opt->value, opt->value_size);
}
