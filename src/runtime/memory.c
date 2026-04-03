#include "curium/memory.h"
#include "curium/error.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdint.h>
#include "curium/thread.h"

#ifdef _WIN32
#include <windows.h>
#endif

#define CURIUM_GC_HASH_SIZE 4096
#define CURIUM_MAX_HANDLES  65536


struct CMObject {
    void* ptr;
    size_t size;
    const char* type;
    const char* file;
    int line;
    time_t alloc_time;
    int ref_count;
    int marked;
    uint32_t hash;
    struct CMObject* next;
    struct CMObject* prev;
    void (*destructor)(void*);
    void (*mark_cb)(void*);
    struct CMObject* hash_next;
    uint64_t generation;
    uint32_t handle_index;
};
typedef struct CMObject CMObject;

typedef struct {
    CMObject* obj;
    uint64_t generation;
} CMHandle;

typedef struct {
    CMObject* head;
    CMObject* tail;
    CMObject* obj_hash[CURIUM_GC_HASH_SIZE];
    
    CMHandle handle_table[CURIUM_MAX_HANDLES];
    uint32_t free_handles[CURIUM_MAX_HANDLES];
    uint32_t free_handles_count;
    uint64_t next_generation;

    size_t total_memory;
    size_t gc_last_collection;
    CMMutex gc_lock;
    CuriumArena* current_arena;
    CMMutex arena_lock;

    size_t peak_memory;
    size_t allocations;
    size_t frees;
    size_t collections;
    double avg_collection_time;
    size_t total_objects;
} CMMemorySystem;

static CMMemorySystem curium_mem = {0};

static void free_object_internal(CMObject* obj);

static inline uint32_t curium_hash_ptr(void* ptr) {
    uintptr_t val = (uintptr_t)ptr;
    val ^= val >> 13;
    val *= 0x5bd1e995;
    val ^= val >> 15;
    return (uint32_t)val;
}

void curium_gc_init(void) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    memset(&curium_mem, 0, sizeof(CMMemorySystem));
    curium_mem.gc_lock = curium_mutex_init();
    curium_mem.arena_lock = curium_mutex_init();
    curium_mem.next_generation = 1;
    curium_mem.free_handles_count = CURIUM_MAX_HANDLES;
    for (uint32_t i = 0; i < CURIUM_MAX_HANDLES; i++) {
        curium_mem.free_handles[i] = CURIUM_MAX_HANDLES - 1 - i;
    }
}

void curium_gc_shutdown(void) {
    curium_gc_collect();
    if (curium_mem.total_objects > 0) {
        /*
        printf("\n[CM] WARNING: Memory leaks detected during shutdown!\n");
        curium_gc_print_leaks();
        */

        /* Force-free all remaining leaked objects to prevent OS-level leaks.
         * Drain from head rather than iterating via next, because destructors
         * may cascade curium_free() calls that mutate the list. */
        while (curium_mem.head) {
            CMObject* current = curium_mem.head;
            uint32_t h = curium_hash_ptr(current->ptr) % CURIUM_GC_HASH_SIZE;
            CMObject* ho = curium_mem.obj_hash[h];
            CMObject* prev_ho = NULL;
            while (ho) {
                if (ho == current) {
                    if (prev_ho) prev_ho->hash_next = ho->hash_next;
                    else curium_mem.obj_hash[h] = ho->hash_next;
                    break;
                }
                prev_ho = ho;
                ho = ho->hash_next;
            }
            free_object_internal(current);
        }
    }

    /* Destroy mutexes to avoid OS-level resource leaks */
    if (curium_mem.gc_lock) {
        curium_mutex_destroy(curium_mem.gc_lock);
        curium_mem.gc_lock = NULL;
    }
    if (curium_mem.arena_lock) {
        curium_mutex_destroy(curium_mem.arena_lock);
        curium_mem.arena_lock = NULL;
    }
}

CuriumArena* curium_arena_create(size_t size) {
    CuriumArena* arena = (CuriumArena*)malloc(sizeof(CuriumArena));
    if (!arena) return NULL;
    arena->block = malloc(size);
    if (!arena->block) { free(arena); return NULL; }
    arena->block_size = size;
    arena->offset = 0;
    arena->name = "dynamic_arena";
    arena->next = NULL;
    arena->peak_usage = 0;
    return arena;
}

void curium_arena_destroy(CuriumArena* arena) {
    if (!arena) return;
    if (arena->block) free(arena->block);
    free(arena);
}

void curium_arena_push(CuriumArena* arena) {
    curium_mutex_lock(curium_mem.arena_lock);
    curium_mem.current_arena = arena;
    curium_mutex_unlock(curium_mem.arena_lock);
}

void curium_arena_pop(void) {
    curium_mutex_lock(curium_mem.arena_lock);
    curium_mem.current_arena = NULL;
    curium_mutex_unlock(curium_mem.arena_lock);
}

void curium_arena_cleanup(void* ptr) {
    CuriumArena** arena_ptr = (CuriumArena**)ptr;
    if (*arena_ptr) {
        curium_mutex_lock(curium_mem.arena_lock);
        if (curium_mem.current_arena == *arena_ptr) {
            curium_mem.current_arena = NULL;
        }
        curium_mutex_unlock(curium_mem.arena_lock);

        curium_arena_destroy(*arena_ptr);
        *arena_ptr = NULL;
    }
}

void* curium_alloc_impl(size_t size, const char* type, const char* file, int line) {
    if (size == 0) return NULL;

    curium_mutex_lock(curium_mem.arena_lock);
    if (curium_mem.current_arena) {
        size_t aligned_size = (size + 7) & ~7;
        if (curium_mem.current_arena->offset + aligned_size <= curium_mem.current_arena->block_size) {
            void* ptr = (char*)curium_mem.current_arena->block + curium_mem.current_arena->offset;
            curium_mem.current_arena->offset += aligned_size;

            if (curium_mem.current_arena->offset > curium_mem.current_arena->peak_usage) {
                curium_mem.current_arena->peak_usage = curium_mem.current_arena->offset;
            }
            curium_mutex_unlock(curium_mem.arena_lock);
            memset(ptr, 0, size); // Zero-initialize arena memory
            return ptr;
        }
    }
    curium_mutex_unlock(curium_mem.arena_lock);

    void* ptr = malloc(size);
    if (!ptr) return NULL;
    memset(ptr, 0, size); /* Zero-initialize for safety in GC system */

    CMObject* obj = (CMObject*)malloc(sizeof(CMObject));
    if (!obj) {
        free(ptr);
        return NULL;
    }

    obj->ptr = ptr;
    obj->size = size;
    obj->type = type ? type : "unknown";
    obj->file = file ? file : "unknown";
    obj->line = line;
    obj->alloc_time = time(NULL);
    obj->ref_count = 1;
    obj->marked = 0;
    obj->hash = 0;
    obj->next = NULL;
    obj->prev = NULL;
    obj->destructor = NULL;
    obj->mark_cb = NULL;
    obj->hash_next = NULL;

    curium_mutex_lock(curium_mem.gc_lock);
    
    /* Assign handle and generation */
    int found = 0;
    if (curium_mem.free_handles_count > 0) {
        uint32_t idx = curium_mem.free_handles[--curium_mem.free_handles_count];
        obj->handle_index = idx;
        obj->generation = curium_mem.next_generation++;
        curium_mem.handle_table[idx].obj = obj;
        curium_mem.handle_table[idx].generation = obj->generation;
        found = 1;
    }

    if (!found) {
        curium_mutex_unlock(curium_mem.gc_lock);
        free(obj); free(ptr);
        curium_error_set(CURIUM_ERROR_MEMORY, "Handle table overflow");
        return NULL;
    }

    uint32_t h = curium_hash_ptr(ptr) % CURIUM_GC_HASH_SIZE;
    obj->hash_next = curium_mem.obj_hash[h];
    curium_mem.obj_hash[h] = obj;

    if (curium_mem.tail) {
        curium_mem.tail->next = obj;
        obj->prev = curium_mem.tail;
        curium_mem.tail = obj;
    } else {
        curium_mem.head = curium_mem.tail = obj;
    }

    curium_mem.total_objects++;
    curium_mem.total_memory += size;
    curium_mem.allocations++;

    if (curium_mem.total_memory > curium_mem.peak_memory) {
        curium_mem.peak_memory = curium_mem.total_memory;
    }

    curium_mutex_unlock(curium_mem.gc_lock);

    return ptr;
}

void curium_set_destructor(void* ptr, void (*destructor)(void*)) {
    if (!ptr) return;
    curium_mutex_lock(curium_mem.gc_lock);
    uint32_t h = curium_hash_ptr(ptr) % CURIUM_GC_HASH_SIZE;
    CMObject* obj = curium_mem.obj_hash[h];
    while (obj) {
        if (obj->ptr == ptr) {
            obj->destructor = destructor;
            break;
        }
        obj = obj->hash_next;
    }
    curium_mutex_unlock(curium_mem.gc_lock);
}

curium_ptr_t curium_ptr(void* ptr) {
    curium_ptr_t p = {0, 0};
    if (!ptr) return p;
    curium_mutex_lock(curium_mem.gc_lock);
    uint32_t h = curium_hash_ptr(ptr) % CURIUM_GC_HASH_SIZE;
    CMObject* obj = curium_mem.obj_hash[h];
    while (obj) {
        if (obj->ptr == ptr) {
            p.index = obj->handle_index;
            p.generation = obj->generation;
            break;
        }
        obj = obj->hash_next;
    }
    curium_mutex_unlock(curium_mem.gc_lock);
    return p;
}

void* curium_ptr_get(curium_ptr_t handle) {
    if (handle.generation == 0 || handle.index >= CURIUM_MAX_HANDLES) return NULL;
    void* result = NULL;
    curium_mutex_lock(curium_mem.gc_lock);
    if (curium_mem.handle_table[handle.index].generation == handle.generation) {
        CMObject* obj = curium_mem.handle_table[handle.index].obj;
        if (obj) result = obj->ptr;
    }
    curium_mutex_unlock(curium_mem.gc_lock);
    return result;
}

static void free_object_internal(CMObject* obj) {
    if (!obj) return;
    if (obj->destructor && obj->ptr) obj->destructor(obj->ptr);
    if (obj->ptr) {
        free(obj->ptr);
        obj->ptr = NULL;
    }
    /* Invalidate handle table entry but keep generation for safety */
    curium_mem.handle_table[obj->handle_index].obj = NULL;
    curium_mem.free_handles[curium_mem.free_handles_count++] = obj->handle_index;

    curium_mem.total_objects--;
    curium_mem.total_memory -= obj->size;
    curium_mem.frees++;
    
    /* Unlink from list */
    if (obj->prev) obj->prev->next = obj->next;
    else curium_mem.head = obj->next;
    if (obj->next) obj->next->prev = obj->prev;
    else curium_mem.tail = obj->prev;

    free(obj);
}

void curium_free(void* ptr) {
    if (!ptr) return;

    curium_mutex_lock(curium_mem.gc_lock);

    uint32_t h = curium_hash_ptr(ptr) % CURIUM_GC_HASH_SIZE;
    CMObject* prev_hash = NULL;
    CMObject* obj = curium_mem.obj_hash[h];
    
    while (obj) {
        if (obj->ptr == ptr) {
            obj->ref_count--;

            if (obj->ref_count <= 0) {
                if (prev_hash) prev_hash->hash_next = obj->hash_next;
                else curium_mem.obj_hash[h] = obj->hash_next;
                free_object_internal(obj);
            }
            curium_mutex_unlock(curium_mem.gc_lock);
            return;
        }
        prev_hash = obj;
        obj = obj->hash_next;
    }

    curium_mutex_unlock(curium_mem.gc_lock);
}

void curium_gc_collect(void) {
    curium_mutex_lock(curium_mem.gc_lock);

    for (CMObject* obj = curium_mem.head; obj; obj = obj->next) {
        obj->marked = (obj->ref_count > 0) ? 1 : 0;
    }

    CMObject* current = curium_mem.head;
    // size_t freed_memory = 0; // Removed
    // int freed_objects = 0; // Removed

    while (current) {
        CMObject* next = current->next;

        if (!current->marked) {
            // freed_memory += current->size; // Removed
            // freed_objects++; // Removed

            uint32_t h = curium_hash_ptr(current->ptr) % CURIUM_GC_HASH_SIZE;
            CMObject* ho = curium_mem.obj_hash[h];
            CMObject* prev_ho = NULL;
            while (ho) {
                if (ho == current) {
                    if (prev_ho) prev_ho->hash_next = ho->hash_next;
                    else curium_mem.obj_hash[h] = ho->hash_next;
                    break;
                }
                prev_ho = ho;
                ho = ho->hash_next;
            }

            free_object_internal(current);
        }

        current = next;
    }
    // curium_mem.gc_last_collection = freed_memory; // Removed
    curium_mem.collections++;

    curium_mutex_unlock(curium_mem.gc_lock);
}

void curium_gc_print_leaks(void) {
    curium_mutex_lock(curium_mem.gc_lock);
    printf("\n══════════════════════════════════════════════════════════════\n");
    printf("                  MEMORY LEAK ANALYSIS REPORT\n");
    printf("──────────────────────────────────────────────────────────────\n");
    CMObject* curr = curium_mem.head;
    while (curr) {
        printf("  LEAK: %-12s │ %6zu bytes │ %s:%d\n", 
               curr->type, curr->size, curr->file, curr->line);
        curr = curr->next;
    }
    printf("══════════════════════════════════════════════════════════════\n");
    curium_mutex_unlock(curium_mem.gc_lock);
}

void curium_gc_stats(void) {
    curium_mutex_lock(curium_mem.gc_lock);
    printf("\n══════════════════════════════════════════════════════════════\n");
    printf("              GARBAGE COLLECTOR STATISTICS\n");
    printf("──────────────────────────────────────────────────────────────\n");
    printf("  Total objects    │ %20zu\n", curium_mem.total_objects);
    printf("  Total memory     │ %20zu bytes\n", curium_mem.total_memory);
    printf("  Peak memory      │ %20zu bytes\n", curium_mem.peak_memory);
    printf("  Allocations      │ %20zu\n", curium_mem.allocations);
    printf("  Frees            │ %20zu\n", curium_mem.frees);
    printf("  Collections      │ %20zu\n", curium_mem.collections);
    if (curium_mem.current_arena) {
        printf("──────────────────────────────────────────────────────────────\n");
        printf("  ARENA STATISTICS\n");
        printf("  Arena name       │ %20s\n", curium_mem.current_arena->name);
        printf("  Arena size       │ %20zu bytes\n", curium_mem.current_arena->block_size);
        printf("  Arena used       │ %20zu bytes\n", curium_mem.current_arena->offset);
        printf("  Arena peak       │ %20zu bytes\n", curium_mem.current_arena->peak_usage);
    }
    printf("══════════════════════════════════════════════════════════════\n");
    curium_mutex_unlock(curium_mem.gc_lock);
}

void curium_retain(void* ptr) {
    if (!ptr) return;

    curium_mutex_lock(curium_mem.gc_lock);

    uint32_t h = curium_hash_ptr(ptr) % CURIUM_GC_HASH_SIZE;
    CMObject* obj = curium_mem.obj_hash[h];

    while (obj) {
        if (obj->ptr == ptr) {
            obj->ref_count++;
            break;
        }
        obj = obj->hash_next;
    }
    curium_mutex_unlock(curium_mem.gc_lock);
}

void curium_untrack(void* ptr) {
    if (!ptr) return;

    curium_mutex_lock(curium_mem.gc_lock);

    uint32_t h = curium_hash_ptr(ptr) % CURIUM_GC_HASH_SIZE;
    CMObject* prev_hash = NULL;
    CMObject* obj = curium_mem.obj_hash[h];

    while (obj) {
        if (obj->ptr == ptr) {
            if (prev_hash) prev_hash->hash_next = obj->hash_next;
            else curium_mem.obj_hash[h] = obj->hash_next;

            curium_mem.handle_table[obj->handle_index].obj = NULL; // Invalidate handle
            curium_mem.free_handles[curium_mem.free_handles_count++] = obj->handle_index;

            if (obj->prev) obj->prev->next = obj->next;
            else curium_mem.head = obj->next;

            if (obj->next) obj->next->prev = obj->prev;
            else curium_mem.tail = obj->prev;

            curium_mem.total_objects--;
            curium_mem.total_memory -= obj->size;

            free(obj);
            curium_mem.frees++;
            break;
        }
        prev_hash = obj;
        obj = obj->hash_next;
    }
    curium_mutex_unlock(curium_mem.gc_lock);
}
