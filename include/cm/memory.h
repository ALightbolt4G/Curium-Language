/**
 * @file memory.h
 * @brief Safe memory allocation, ownership rules, and GC tracking.
 */
#ifndef CM_MEMORY_H
#define CM_MEMORY_H

#include "core.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the memory tracking system. Should be called at program start.
 */
void cm_gc_init(void);

/**
 * @brief Arena Allocator structure
 */
typedef struct CMArena CMArena;
struct CMArena {
    void* block;
    size_t block_size;
    size_t offset;
    struct CMArena* next;
    const char* name;
    size_t peak_usage;
};

/**
 * @brief Create a new memory arena
 * @param size Initial size of the arena in bytes
 */
CMArena* cm_arena_create(size_t size);

/**
 * @brief Destroy a memory arena and reclaim its memory
 * @param arena Pointer to the arena to destroy
 */
void cm_arena_destroy(CMArena* arena);

/**
 * @brief Push an arena onto the current thread's context
 * @param arena Pointer to the arena
 */
void cm_arena_push(CMArena* arena);

/**
 * @brief Pop the current arena from the thread's context
 */
void cm_arena_pop(void);

/**
 * @brief Cleanup callback for scoped arenas
 * @param ptr Pointer to the arena pointer
 */
void cm_arena_cleanup(void* ptr);

/**
 * @brief Scoped memory allocation within an arena for automatic cleanup
 */
#if defined(__GNUC__) || defined(__clang__)
#define CM_WITH_ARENA(name_str, size) \
    for (CMArena* _a __attribute__((cleanup(cm_arena_cleanup))) = cm_arena_create(size); \
         _a ? (_a->name = (name_str), cm_arena_push(_a), 1) : 0; \
         cm_arena_pop(), _a = NULL)
#else
/* Fallback if cleanup attribute is unavailable, though scoping won't be as magical */
#define CM_WITH_ARENA(name_str, size) \
    for (CMArena* _a = cm_arena_create(size); \
         _a ? (_a->name = (name_str), cm_arena_push(_a), 1) : 0; \
         cm_arena_pop(), cm_arena_destroy(_a), _a = NULL)
#endif

/**
 * @brief Shut down GC system and report on leaks.
 */
void cm_gc_shutdown(void);

/**
 * @brief Allocate tracked memory securely.
 * @param size Block size in bytes
 * @param type Custom identifying tags
 * @param file Source file tracking (use __FILE__)
 * @param line Source line tracking (use __LINE__)
 */
void* cm_alloc_impl(size_t size, const char* type, const char* file, int line);

/**
 * @brief Macro for convenient allocations tracking source automatically.
 */
#define cm_alloc(size, type) cm_alloc_impl(size, type, __FILE__, __LINE__)

/**
 * @brief Decrease reference count or free memory. Drops pointer tracking implicitly.
 */
void cm_free(void* ptr);

/**
 * @brief Increase reference count, indicating extended ownership.
 */
void cm_retain(void* ptr);

/**
 * @brief Register a cleanup function for a memory-tracked pointer.
 */
void cm_set_destructor(void* ptr, void (*destructor)(void*));

/**
 * @brief Wrap a raw pointer into a safe handle (cm_ptr_t).
 */
cm_ptr_t cm_ptr(void* ptr);

/**
 * @brief Safely resolve a handle back to a raw pointer. Returns NULL if invalid.
 */
void* cm_ptr_get(cm_ptr_t handle);

/**
 * @brief Stop tracking a pointer in the GC system manually cleanly.
 */
void cm_untrack(void* ptr);

/**
 * @brief Perform an immediate garbage collection cycle safely resolving scopes.
 */
void cm_gc_collect(void);

/**
 * @brief Logs memory tracking statistics accurately mapping allocations.
 */
void cm_gc_stats(void);

/**
 * @brief Detailed report of all currently leaked/uncollected objects.
 */
void cm_gc_print_leaks(void);

#ifdef __cplusplus
}
#endif

#endif /* CM_MEMORY_H */
