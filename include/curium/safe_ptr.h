#ifndef CURIUM_SAFE_PTR_H
#define CURIUM_SAFE_PTR_H

#include "curium/core.h"
#include "curium/error.h"
#include <stddef.h>
#include <stdint.h>

/* ============================================================================
 * CM v2 Safe Pointer System with Bounds Checking
 * ==========================================================================*/

/* Safe pointer structure with metadata */
typedef struct {
    void* base;          /* Actual pointer to data */
    size_t size;         /* Size of allocation in bytes */
    size_t count;        /* Number of elements (for arrays) */
    size_t element_size; /* Size of each element */
    uint32_t magic;      /* Canary for use-after-free detection */
    uint32_t flags;      /* Flags for mutability, etc. */
} curium_safe_ptr_t;

/* Safe pointer flags */
#define CURIUM_PTR_FLAG_MUTABLE      (1 << 0)  /* Pointer is mutable */
#define CURIUM_PTR_FLAG_IMMUTABLE    (1 << 1)  /* Pointer is immutable */
#define CURIUM_PTR_FLAG_ARRAY        (1 << 2)  /* Pointer is an array */
#define CURIUM_PTR_FLAG_GC_MANAGED   (1 << 3)  /* Managed by garbage collector */

/* Magic number for corruption detection */
#define CURIUM_SAFE_PTR_MAGIC        0xDEADBEEF
#define CURIUM_SAFE_PTR_FREED        0xFEEDFACE

/* Bounds checking macro */
#define CURIUM_BOUNDS_CHECK(ptr, index) \
    do { \
        if (!(ptr) || (ptr)->magic != CURIUM_SAFE_PTR_MAGIC) { \
            curium_panic("use-after-free or invalid pointer"); \
        } \
        if ((size_t)(index) >= (ptr)->count) { \
            curium_panic("index out of bounds: %zu >= %zu", \
                     (size_t)(index), (ptr)->count); \
        } \
    } while(0)

/* Safe dereference macro */
#define CURIUM_SAFE_DEREF(ptr) \
    (((ptr) && (ptr)->magic == CURIUM_SAFE_PTR_MAGIC) ? (ptr)->base : NULL)

/* Safe address-of macro */
#define CURIUM_SAFE_ADDR_OF(var) \
    curium_safe_ptr_new(&(var), sizeof(var), 1, sizeof(var), CURIUM_PTR_FLAG_IMMUTABLE)

/* Function declarations */

/* Create a new safe pointer */
curium_safe_ptr_t* curium_safe_ptr_new(void* base, size_t size, size_t count, 
                               size_t element_size, uint32_t flags);

/* Create a safe pointer from malloc */
curium_safe_ptr_t* curium_safe_ptr_malloc(size_t count, size_t element_size, uint32_t flags);

/* Free a safe pointer */
void curium_safe_ptr_free(curium_safe_ptr_t* ptr);

/* Get element at index (with bounds checking) */
void* curium_safe_ptr_get(curium_safe_ptr_t* ptr, size_t index);

/* Set element at index (with bounds checking and mutability check) */
int curium_safe_ptr_set(curium_safe_ptr_t* ptr, size_t index, const void* value);

/* Get pointer to element (for passing to C functions) */
void* curium_safe_ptr_data(curium_safe_ptr_t* ptr);

/* Get array length */
size_t curium_safe_ptr_len(curium_safe_ptr_t* ptr);

/* Check if pointer is valid */
int curium_safe_ptr_is_valid(curium_safe_ptr_t* ptr);

/* Make pointer immutable */
void curium_safe_ptr_make_immutable(curium_safe_ptr_t* ptr);

/* Clone a safe pointer (deep copy if array) */
curium_safe_ptr_t* curium_safe_ptr_clone(curium_safe_ptr_t* ptr);

/* Slice a safe pointer (creates view into original) */
curium_safe_ptr_t* curium_safe_ptr_slice(curium_safe_ptr_t* ptr, size_t start, size_t len);

/* Debug functions */
void curium_safe_ptr_debug_print(curium_safe_ptr_t* ptr);

/* Panic function for runtime errors */
void curium_panic(const char* format, ...);

#endif
