#include "cm/memory.h"
#include "cm/error.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdint.h>
#include "cm/thread.h"

#ifdef _WIN32
#include <windows.h>
#endif

#define CM_GC_HASH_SIZE 4096
#define CM_MAX_HANDLES  65536


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
    CMObject* obj_hash[CM_GC_HASH_SIZE];
    
    CMHandle handle_table[CM_MAX_HANDLES];
    uint64_t next_generation;
    uint32_t next_handle_index;

    size_t total_memory;
    size_t gc_last_collection;
    CMMutex gc_lock;
    CMArena* current_arena;
    CMMutex arena_lock;

    size_t peak_memory;
    size_t allocations;
    size_t frees;
    size_t collections;
    double avg_collection_time;
    size_t total_objects;
} CMMemorySystem;

static CMMemorySystem cm_mem = {0};

static void free_object_internal(CMObject* obj);

static inline uint32_t cm_hash_ptr(void* ptr) {
    uintptr_t val = (uintptr_t)ptr;
    val ^= val >> 13;
    val *= 0x5bd1e995;
    val ^= val >> 15;
    return (uint32_t)val;
}

void cm_gc_init(void) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    memset(&cm_mem, 0, sizeof(CMMemorySystem));
    cm_mem.gc_lock = cm_mutex_init();
    cm_mem.arena_lock = cm_mutex_init();
    cm_mem.next_generation = 1;
    cm_mem.next_handle_index = 0;
}

void cm_gc_shutdown(void) {
    cm_gc_collect();
    if (cm_mem.total_objects > 0) {
        printf("\n[CM] WARNING: Memory leaks detected during shutdown!\n");
        cm_gc_print_leaks();

        /* Force-free all remaining leaked objects to prevent OS-level leaks.
         * Drain from head rather than iterating via next, because destructors
         * may cascade cm_free() calls that mutate the list. */
        while (cm_mem.head) {
            CMObject* current = cm_mem.head;
            uint32_t h = cm_hash_ptr(current->ptr) % CM_GC_HASH_SIZE;
            CMObject* ho = cm_mem.obj_hash[h];
            CMObject* prev_ho = NULL;
            while (ho) {
                if (ho == current) {
                    if (prev_ho) prev_ho->hash_next = ho->hash_next;
                    else cm_mem.obj_hash[h] = ho->hash_next;
                    break;
                }
                prev_ho = ho;
                ho = ho->hash_next;
            }
            free_object_internal(current);
        }
    }

    /* Destroy mutexes to avoid OS-level resource leaks */
    if (cm_mem.gc_lock) {
        cm_mutex_destroy(cm_mem.gc_lock);
        cm_mem.gc_lock = NULL;
    }
    if (cm_mem.arena_lock) {
        cm_mutex_destroy(cm_mem.arena_lock);
        cm_mem.arena_lock = NULL;
    }
}

CMArena* cm_arena_create(size_t size) {
    CMArena* arena = (CMArena*)malloc(sizeof(CMArena));
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

void cm_arena_destroy(CMArena* arena) {
    if (!arena) return;
    if (arena->block) free(arena->block);
    free(arena);
}

void cm_arena_push(CMArena* arena) {
    cm_mutex_lock(cm_mem.arena_lock);
    cm_mem.current_arena = arena;
    cm_mutex_unlock(cm_mem.arena_lock);
}

void cm_arena_pop(void) {
    cm_mutex_lock(cm_mem.arena_lock);
    cm_mem.current_arena = NULL;
    cm_mutex_unlock(cm_mem.arena_lock);
}

void cm_arena_cleanup(void* ptr) {
    CMArena** arena_ptr = (CMArena**)ptr;
    if (*arena_ptr) {
        cm_mutex_lock(cm_mem.arena_lock);
        if (cm_mem.current_arena == *arena_ptr) {
            cm_mem.current_arena = NULL;
        }
        cm_mutex_unlock(cm_mem.arena_lock);

        cm_arena_destroy(*arena_ptr);
        *arena_ptr = NULL;
    }
}

void* cm_alloc_impl(size_t size, const char* type, const char* file, int line) {
    if (size == 0) return NULL;

    cm_mutex_lock(cm_mem.arena_lock);
    if (cm_mem.current_arena) {
        size_t aligned_size = (size + 7) & ~7;
        if (cm_mem.current_arena->offset + aligned_size <= cm_mem.current_arena->block_size) {
            void* ptr = (char*)cm_mem.current_arena->block + cm_mem.current_arena->offset;
            cm_mem.current_arena->offset += aligned_size;

            if (cm_mem.current_arena->offset > cm_mem.current_arena->peak_usage) {
                cm_mem.current_arena->peak_usage = cm_mem.current_arena->offset;
            }
            cm_mutex_unlock(cm_mem.arena_lock);
            memset(ptr, 0, size); // Zero-initialize arena memory
            return ptr;
        }
    }
    cm_mutex_unlock(cm_mem.arena_lock);

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

    cm_mutex_lock(cm_mem.gc_lock);
    
    /* Assign handle and generation */
    uint32_t start_idx = cm_mem.next_handle_index;
    int found = 0;
    for (uint32_t i = 0; i < CM_MAX_HANDLES; i++) {
        uint32_t idx = (start_idx + i) % CM_MAX_HANDLES;
        if (cm_mem.handle_table[idx].obj == NULL) {
            obj->handle_index = idx;
            obj->generation = cm_mem.next_generation++;
            cm_mem.handle_table[idx].obj = obj;
            cm_mem.handle_table[idx].generation = obj->generation;
            cm_mem.next_handle_index = (idx + 1) % CM_MAX_HANDLES;
            found = 1;
            break;
        }
    }

    if (!found) {
        cm_mutex_unlock(cm_mem.gc_lock);
        free(obj); free(ptr);
        cm_error_set(CM_ERROR_MEMORY, "Handle table overflow");
        return NULL;
    }

    uint32_t h = cm_hash_ptr(ptr) % CM_GC_HASH_SIZE;
    obj->hash_next = cm_mem.obj_hash[h];
    cm_mem.obj_hash[h] = obj;

    if (cm_mem.tail) {
        cm_mem.tail->next = obj;
        obj->prev = cm_mem.tail;
        cm_mem.tail = obj;
    } else {
        cm_mem.head = cm_mem.tail = obj;
    }

    cm_mem.total_objects++;
    cm_mem.total_memory += size;
    cm_mem.allocations++;

    if (cm_mem.total_memory > cm_mem.peak_memory) {
        cm_mem.peak_memory = cm_mem.total_memory;
    }

    cm_mutex_unlock(cm_mem.gc_lock);

    return ptr;
}

void cm_set_destructor(void* ptr, void (*destructor)(void*)) {
    if (!ptr) return;
    cm_mutex_lock(cm_mem.gc_lock);
    uint32_t h = cm_hash_ptr(ptr) % CM_GC_HASH_SIZE;
    CMObject* obj = cm_mem.obj_hash[h];
    while (obj) {
        if (obj->ptr == ptr) {
            obj->destructor = destructor;
            break;
        }
        obj = obj->hash_next;
    }
    cm_mutex_unlock(cm_mem.gc_lock);
}

cm_ptr_t cm_ptr(void* ptr) {
    cm_ptr_t p = {0, 0};
    if (!ptr) return p;
    cm_mutex_lock(cm_mem.gc_lock);
    uint32_t h = cm_hash_ptr(ptr) % CM_GC_HASH_SIZE;
    CMObject* obj = cm_mem.obj_hash[h];
    while (obj) {
        if (obj->ptr == ptr) {
            p.index = obj->handle_index;
            p.generation = obj->generation;
            break;
        }
        obj = obj->hash_next;
    }
    cm_mutex_unlock(cm_mem.gc_lock);
    return p;
}

void* cm_ptr_get(cm_ptr_t handle) {
    if (handle.generation == 0 || handle.index >= CM_MAX_HANDLES) return NULL;
    void* result = NULL;
    cm_mutex_lock(cm_mem.gc_lock);
    if (cm_mem.handle_table[handle.index].generation == handle.generation) {
        CMObject* obj = cm_mem.handle_table[handle.index].obj;
        if (obj) result = obj->ptr;
    }
    cm_mutex_unlock(cm_mem.gc_lock);
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
    cm_mem.handle_table[obj->handle_index].obj = NULL;

    cm_mem.total_objects--;
    cm_mem.total_memory -= obj->size;
    cm_mem.frees++;
    
    /* Unlink from list */
    if (obj->prev) obj->prev->next = obj->next;
    else cm_mem.head = obj->next;
    if (obj->next) obj->next->prev = obj->prev;
    else cm_mem.tail = obj->prev;

    free(obj);
}

void cm_free(void* ptr) {
    if (!ptr) return;

    cm_mutex_lock(cm_mem.gc_lock);

    uint32_t h = cm_hash_ptr(ptr) % CM_GC_HASH_SIZE;
    CMObject* prev_hash = NULL;
    CMObject* obj = cm_mem.obj_hash[h];
    
    while (obj) {
        if (obj->ptr == ptr) {
            obj->ref_count--;

            if (obj->ref_count <= 0) {
                if (prev_hash) prev_hash->hash_next = obj->hash_next;
                else cm_mem.obj_hash[h] = obj->hash_next;
                free_object_internal(obj);
            }
            cm_mutex_unlock(cm_mem.gc_lock);
            return;
        }
        prev_hash = obj;
        obj = obj->hash_next;
    }

    cm_mutex_unlock(cm_mem.gc_lock);
}

void cm_gc_collect(void) {
    cm_mutex_lock(cm_mem.gc_lock);

    for (CMObject* obj = cm_mem.head; obj; obj = obj->next) {
        obj->marked = (obj->ref_count > 0) ? 1 : 0;
    }

    CMObject* current = cm_mem.head;
    // size_t freed_memory = 0; // Removed
    // int freed_objects = 0; // Removed

    while (current) {
        CMObject* next = current->next;

        if (!current->marked) {
            // freed_memory += current->size; // Removed
            // freed_objects++; // Removed

            uint32_t h = cm_hash_ptr(current->ptr) % CM_GC_HASH_SIZE;
            CMObject* ho = cm_mem.obj_hash[h];
            CMObject* prev_ho = NULL;
            while (ho) {
                if (ho == current) {
                    if (prev_ho) prev_ho->hash_next = ho->hash_next;
                    else cm_mem.obj_hash[h] = ho->hash_next;
                    break;
                }
                prev_ho = ho;
                ho = ho->hash_next;
            }

            free_object_internal(current);
        }

        current = next;
    }
    // cm_mem.gc_last_collection = freed_memory; // Removed
    cm_mem.collections++;

    cm_mutex_unlock(cm_mem.gc_lock);
}

void cm_gc_print_leaks(void) {
    cm_mutex_lock(cm_mem.gc_lock);
    printf("\n══════════════════════════════════════════════════════════════\n");
    printf("                  MEMORY LEAK ANALYSIS REPORT\n");
    printf("──────────────────────────────────────────────────────────────\n");
    CMObject* curr = cm_mem.head;
    while (curr) {
        printf("  LEAK: %-12s │ %6zu bytes │ %s:%d\n", 
               curr->type, curr->size, curr->file, curr->line);
        curr = curr->next;
    }
    printf("══════════════════════════════════════════════════════════════\n");
    cm_mutex_unlock(cm_mem.gc_lock);
}

void cm_gc_stats(void) {
    cm_mutex_lock(cm_mem.gc_lock);
    printf("\n══════════════════════════════════════════════════════════════\n");
    printf("              GARBAGE COLLECTOR STATISTICS\n");
    printf("──────────────────────────────────────────────────────────────\n");
    printf("  Total objects    │ %20zu\n", cm_mem.total_objects);
    printf("  Total memory     │ %20zu bytes\n", cm_mem.total_memory);
    printf("  Peak memory      │ %20zu bytes\n", cm_mem.peak_memory);
    printf("  Allocations      │ %20zu\n", cm_mem.allocations);
    printf("  Frees            │ %20zu\n", cm_mem.frees);
    printf("  Collections      │ %20zu\n", cm_mem.collections);
    if (cm_mem.current_arena) {
        printf("──────────────────────────────────────────────────────────────\n");
        printf("  ARENA STATISTICS\n");
        printf("  Arena name       │ %20s\n", cm_mem.current_arena->name);
        printf("  Arena size       │ %20zu bytes\n", cm_mem.current_arena->block_size);
        printf("  Arena used       │ %20zu bytes\n", cm_mem.current_arena->offset);
        printf("  Arena peak       │ %20zu bytes\n", cm_mem.current_arena->peak_usage);
    }
    printf("══════════════════════════════════════════════════════════════\n");
    cm_mutex_unlock(cm_mem.gc_lock);
}

void cm_retain(void* ptr) {
    if (!ptr) return;

    cm_mutex_lock(cm_mem.gc_lock);

    uint32_t h = cm_hash_ptr(ptr) % CM_GC_HASH_SIZE;
    CMObject* obj = cm_mem.obj_hash[h];

    while (obj) {
        if (obj->ptr == ptr) {
            obj->ref_count++;
            break;
        }
        obj = obj->hash_next;
    }
    cm_mutex_unlock(cm_mem.gc_lock);
}

void cm_untrack(void* ptr) {
    if (!ptr) return;

    cm_mutex_lock(cm_mem.gc_lock);

    uint32_t h = cm_hash_ptr(ptr) % CM_GC_HASH_SIZE;
    CMObject* prev_hash = NULL;
    CMObject* obj = cm_mem.obj_hash[h];

    while (obj) {
        if (obj->ptr == ptr) {
            if (prev_hash) prev_hash->hash_next = obj->hash_next;
            else cm_mem.obj_hash[h] = obj->hash_next;

            cm_mem.handle_table[obj->handle_index].obj = NULL; // Invalidate handle

            if (obj->prev) obj->prev->next = obj->next;
            else cm_mem.head = obj->next;

            if (obj->next) obj->next->prev = obj->prev;
            else cm_mem.tail = obj->prev;

            cm_mem.total_objects--;
            cm_mem.total_memory -= obj->size;

            free(obj);
            cm_mem.frees++;
            break;
        }
        prev_hash = obj;
        obj = obj->hash_next;
    }
    cm_mutex_unlock(cm_mem.gc_lock);
}
