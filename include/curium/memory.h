/**
 * @file memory.h
 * @brief Safe memory allocation, ownership rules, and GC tracking.
 */
#ifndef CURIUM_MEMORY_H
#define CURIUM_MEMORY_H

#include "core.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the memory tracking system. Should be called at program start.
 */
void curium_gc_init(void);

/**
 * @brief Arena Allocator structure
 */
typedef struct CuriumArena CuriumArena;
struct CuriumArena {
    void* block;
    size_t block_size;
    size_t offset;
    struct CuriumArena* next;
    const char* name;
    size_t peak_usage;
};

/**
 * @brief Create a new memory arena
 * @param size Initial size of the arena in bytes
 */
CuriumArena* curium_arena_create(size_t size);

/**
 * @brief Destroy a memory arena and reclaim its memory
 * @param arena Pointer to the arena to destroy
 */
void curium_arena_destroy(CuriumArena* arena);

/**
 * @brief Push an arena onto the current thread's context
 * @param arena Pointer to the arena
 */
void curium_arena_push(CuriumArena* arena);

/**
 * @brief Pop the current arena from the thread's context
 */
void curium_arena_pop(void);

/**
 * @brief Cleanup callback for scoped arenas
 * @param ptr Pointer to the arena pointer
 */
void curium_arena_cleanup(void* ptr);

/**
 * @brief Scoped memory allocation within an arena for automatic cleanup
 */
#if defined(__GNUC__) || defined(__clang__)
#define CURIUM_WITH_ARENA(name_str, size) \
    for (CuriumArena* _a __attribute__((cleanup(curium_arena_cleanup))) = curium_arena_create(size); \
         _a ? (_a->name = (name_str), curium_arena_push(_a), 1) : 0; \
         curium_arena_pop(), _a = NULL)
#else
/* Fallback if cleanup attribute is unavailable, though scoping won't be as magical */
#define CURIUM_WITH_ARENA(name_str, size) \
    for (CuriumArena* _a = curium_arena_create(size); \
         _a ? (_a->name = (name_str), curium_arena_push(_a), 1) : 0; \
         curium_arena_pop(), curium_arena_destroy(_a), _a = NULL)
#endif

/**
 * @brief Shut down GC system and report on leaks.
 */
void curium_gc_shutdown(void);

/**
 * @brief Allocate tracked memory securely.
 * @param size Block size in bytes
 * @param type Custom identifying tags
 * @param file Source file tracking (use __FILE__)
 * @param line Source line tracking (use __LINE__)
 */
void* curium_alloc_impl(size_t size, const char* type, const char* file, int line);

/**
 * @brief Macro for convenient allocations tracking source automatically.
 */
#define curium_alloc(size, type) curium_alloc_impl(size, type, __FILE__, __LINE__)

/**
 * @brief Decrease reference count or free memory. Drops pointer tracking implicitly.
 */
void curium_free(void* ptr);

/**
 * @brief Increase reference count, indicating extended ownership.
 */
void curium_retain(void* ptr);

/**
 * @brief Register a cleanup function for a memory-tracked pointer.
 */
void curium_set_destructor(void* ptr, void (*destructor)(void*));

/**
 * @brief Wrap a raw pointer into a safe handle (curium_ptr_t).
 */
curium_ptr_t curium_ptr(void* ptr);

/**
 * @brief Safely resolve a handle back to a raw pointer. Returns NULL if invalid.
 */
void* curium_ptr_get(curium_ptr_t handle);

/**
 * @brief Stop tracking a pointer in the GC system manually cleanly.
 */
void curium_untrack(void* ptr);

/**
 * @brief Perform an immediate garbage collection cycle safely resolving scopes.
 */
void curium_gc_collect(void);

/**
 * @brief Logs memory tracking statistics accurately mapping allocations.
 */
void curium_gc_stats(void);

/**
 * @brief Detailed report of all currently leaked/uncollected objects.
 */
void curium_gc_print_leaks(void);

#ifdef __cplusplus
}
#endif

#endif /* CURIUM_MEMORY_H */
