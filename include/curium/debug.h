#ifndef CURIUM_DEBUG_H
#define CURIUM_DEBUG_H

#include "curium/core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Runtime Debug & Variable Tracking
 * Tracks variable initialization, types, and values at runtime
 * ========================================================================== */

typedef enum {
    CURIUM_VAR_UNINITIALIZED,
    CURIUM_VAR_INITIALIZED,
    CURIUM_VAR_FREED
} curium_var_state_t;

typedef struct curium_var_track {
    char name[128];
    char type[64];
    void* address;
    curium_var_state_t state;
    int line_declared;
    char file_declared[256];
    struct curium_var_track* next;
} curium_var_track_t;

/* Initialize debug tracking */
void curium_debug_init(void);
void curium_debug_shutdown(void);

/* Variable tracking */
void curium_debug_var_declare(const char* name, const char* type, void* addr, 
                          const char* file, int line);
void curium_debug_var_init(void* addr);
void curium_debug_var_free(void* addr);

/* Check variable state */
int curium_debug_var_is_initialized(void* addr);
const char* curium_debug_var_get_name(void* addr);
const char* curium_debug_var_get_type(void* addr);

/* Safety checks */
void curium_debug_check_null(const void* ptr, const char* name, 
                         const char* file, int line);
void curium_debug_check_initialized(void* addr, const char* name,
                                const char* file, int line);
void curium_debug_check_bounds(size_t index, size_t size, const char* name,
                           const char* file, int line);

/* Memory tracking */
void curium_debug_track_alloc(void* ptr, size_t size, const char* file, int line);
void curium_debug_track_free(void* ptr);
int curium_debug_is_valid_ptr(void* ptr);

/* Stack trace */
void curium_debug_print_stack_trace(void);
void curium_debug_capture_stack_trace(char** buffer, int max_depth);

/* Macros for automatic tracking */
#define CURIUM_VAR_DECLARE(name, type, addr) \
    curium_debug_var_declare(name, type, addr, __FILE__, __LINE__)

#define CURIUM_VAR_INIT(addr) \
    curium_debug_var_init(addr)

#define CURIUM_CHECK_NULL(ptr) \
    curium_debug_check_null(ptr, #ptr, __FILE__, __LINE__)

#define CURIUM_CHECK_INIT(addr, name) \
    curium_debug_check_initialized(addr, name, __FILE__, __LINE__)

#define CURIUM_CHECK_BOUNDS(idx, size, name) \
    curium_debug_check_bounds(idx, size, name, __FILE__, __LINE__)

#ifdef __cplusplus
}
#endif

#endif /* CURIUM_DEBUG_H */
