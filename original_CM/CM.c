/*
 * ============================================================================
 * CM.c - C Multitask Intelligent Library
 * Implementation File
 * Author: Adham Hossam
 * Version: 5.0.0
 * ============================================================================
 */


/* ============================================================================
 * INCLUDES
 * ============================================================================ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <limits.h>
#include <float.h>
#include <math.h>
#include <time.h>
#include <ctype.h>
#include <wchar.h>
#include <wctype.h>
#include <locale.h>
#include <errno.h>
#include <assert.h>
#include <signal.h>
#include <setjmp.h>
#include <pthread.h>

#include "CM.h"

#ifdef _WIN32
    #include <windows.h>
    #include <wincrypt.h>
    #pragma comment(lib, "advapi32.lib")
#endif


/* ============================================================================
 * SAFE I/O FUNCTIONS
 * ============================================================================ */
#ifndef CM_SAFE_IO_H
#define CM_SAFE_IO_H


#ifdef __ANDROID__
#include <android/log.h>
#include <unistd.h>
#include <linux/random.h>

/*
 * logic: bridges linux syscalls securing random buffer populates natively
 * calling: internal use exclusively for random sequence generations
 */
static int getrandom(void *buf, size_t buflen, unsigned int flags){
    return syscall(__NR_getrandom, buf, buflen, flags);
}
#define CM_LOG_TAG "CM"

#endif


extern __thread cm_exception_frame_t* cm_current_frame;

/*
 * logic: normal console printing routing buffers intelligently across platforms
 * calling: cm_printf("Loaded %d files", max);
 */
void cm_printf(const char* format, ...) {
    if (!format) return;
    
    va_list args;
    va_start(args, format);
    
    #ifdef __ANDROID__
        // على Android، نستخدم printf العادي
        vprintf(format, args);
        fflush(stdout);
    #else
        if (stdout) {
            vprintf(format, args);
            fflush(stdout);
        }
    #endif
    
    va_end(args);
}

/*
 * logic: routes errors to stderr outputting standard formatting blocks gracefully
 * calling: cm_error("Failure reading pointer %p", ptr);
 */
void cm_error(const char* format, ...) {
    if (!format) return;
    
    va_list args;
    va_start(args, format);
    
    #ifdef __ANDROID__
        __android_log_vprint(ANDROID_LOG_ERROR, CM_LOG_TAG, format, args);
        
        va_end(args);
        va_start(args, format);
        
        printf("❌ ERROR: ");
        vprintf(format, args);
        printf("\n");
        fflush(stdout);
    #else
        if (stderr) {
            vfprintf(stderr, format, args);
            fprintf(stderr, "\n");
        } else if (stdout) {
            printf("ERROR: ");
            vprintf(format, args);
            printf("\n");
        }
    #endif
    
    va_end(args);
}

/*
 * logic: queries inputs bypassing standard buffered traps safely globally
 * calling: cm_gets(buffer, 128);
 */
char* cm_gets(char* buffer, size_t size) {
    if (!buffer || size == 0) return NULL;
    
    #ifdef __ANDROID__
        if (stdin) {
            char* result = fgets(buffer, size, stdin);
            if (result) {
                // إزالة newline من النهاية
                buffer[strcspn(buffer, "\n")] = 0;
                return result;
            }
        }
        (void)buffer;  // لمنع تحذير unused parameter
        (void)size;    // لمنع تحذير unused parameter
        return NULL;   // فشل القراءة
    #else
        if (stdin) {
            char* result = fgets(buffer, size, stdin);
            if (result) {
                buffer[strcspn(buffer, "\n")] = 0;
                return result;
            }
        }
        return NULL;
    #endif
}
#endif

/* ============================================================================
 * INTERNAL STRUCTURES
 * ============================================================================ */
#define CM_GC_HASH_SIZE 4096

/*
 * logic: hashes pointers rapidly resolving 32 bit mapped distributions consistently
 * calling: internal use mapping elements properly within linked structs
 */
static inline uint32_t cm_hash_ptr(void* ptr) {
    uintptr_t val = (uintptr_t)ptr;
    val ^= val >> 13;
    val *= 0x5bd1e995;
    val ^= val >> 15;
    return (uint32_t)val;
}

typedef struct {
    CMObject* head;
    CMObject* tail;
    CMObject* obj_hash[CM_GC_HASH_SIZE];
    size_t total_memory;
    size_t gc_last_collection;
    pthread_mutex_t gc_lock;
    CMArena* current_arena;
    pthread_mutex_t arena_lock;

    size_t peak_memory;
    size_t allocations;
    size_t frees;
    size_t collections;
    double avg_collection_time;
    size_t total_objects;
} CMMemorySystem;

static CMMemorySystem cm_mem = {0};

int cm_last_error = 0;
char cm_error_message[1024] = {0};

/* ============================================================================
 * IMPLEMENTATION
 * ============================================================================ */

/*
 * logic: initializes tracked collection bounds ensuring state readiness globally
 * calling: automatically invoked via OS constructor initialization beforehand
 */
void cm_gc_init(void) {
    memset(&cm_mem, 0, sizeof(CMMemorySystem)); 
    pthread_mutex_init(&cm_mem.gc_lock, NULL);
    pthread_mutex_init(&cm_mem.arena_lock, NULL);
}

/*
 * logic: produces isolated arena spaces enabling ultra fast memory drops immediately
 * calling: arena = cm_arena_create(2048);
 */
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

/*
 * logic: permanently reclaims active arenas removing tracking memory explicitly inherently
 * calling: cm_arena_destroy(arena);
 */
void cm_arena_destroy(CMArena* arena) {
    if (!arena) return;
    if (arena->block) free(arena->block);
    free(arena);
}

/*
 * logic: binds threaded spaces allocating context directly routing pointers immediately internally
 * calling: cm_arena_push(arena);
 */
void cm_arena_push(CMArena* arena) {
    pthread_mutex_lock(&cm_mem.arena_lock);
    cm_mem.current_arena = arena;
    pthread_mutex_unlock(&cm_mem.arena_lock);
}

/*
 * logic: dismisses currently tracking bounds restoring external OS fallbacks implicitly
 * calling: cm_arena_pop();
 */
void cm_arena_pop(void) {
    pthread_mutex_lock(&cm_mem.arena_lock);
    cm_mem.current_arena = NULL;
    pthread_mutex_unlock(&cm_mem.arena_lock);
}

/*
 * logic: dynamically handles standard allocation wrapping memory via managed trackers seamlessly
 * calling: ptr = cm_alloc(1024, "string", __FILE__, __LINE__);
 */
void* cm_alloc(size_t size, const char* type, const char* file, int line) {
    if (size == 0) return NULL;

    pthread_mutex_lock(&cm_mem.arena_lock);
    if (cm_mem.current_arena) {
        /* Align memory to 8 bytes for CPU efficiency and to prevent alignment faults */
        size_t aligned_size = (size + 7) & ~7;

        if (cm_mem.current_arena->offset + aligned_size <= cm_mem.current_arena->block_size) {
            void* ptr = (char*)cm_mem.current_arena->block + cm_mem.current_arena->offset;
            cm_mem.current_arena->offset += aligned_size;

            /* Update Arena usage statistics */
            if (cm_mem.current_arena->offset > cm_mem.current_arena->peak_usage) {
                cm_mem.current_arena->peak_usage = cm_mem.current_arena->offset;
            }
            pthread_mutex_unlock(&cm_mem.arena_lock);
            /* ✅ IMPORTANT: Arena objects are not tracked by GC to eliminate overhead */
            return ptr; /* Immediate return for maximum speed */
        }

        /* Fallback mechanism if the current arena is exhausted */
        cm_error("[ARENA] Warning: Arena '%s' full, falling back to GC", 
         cm_mem.current_arena->name);
    }
    pthread_mutex_unlock(&cm_mem.arena_lock);
    void* ptr = malloc(size);
    if (!ptr) return NULL;

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

    pthread_mutex_lock(&cm_mem.gc_lock);
    
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

    pthread_mutex_unlock(&cm_mem.gc_lock);

    return ptr;
}

void cm_free(void* ptr) {
    if (!ptr) return;

    pthread_mutex_lock(&cm_mem.gc_lock);

    uint32_t h = cm_hash_ptr(ptr) % CM_GC_HASH_SIZE;
    CMObject* prev_hash = NULL;
    CMObject* obj = cm_mem.obj_hash[h];
    
    while (obj) {
        if (obj->ptr == ptr) {
            obj->ref_count--;

            if (obj->ref_count <= 0) {
                if (obj->destructor) {
                    obj->destructor(ptr);
                }

                // Remove from hash chain
                if (prev_hash) prev_hash->hash_next = obj->hash_next;
                else cm_mem.obj_hash[h] = obj->hash_next;

                free(ptr);
                obj->ptr = NULL;

                if (obj->prev) {
                    obj->prev->next = obj->next;
                } else {
                    cm_mem.head = obj->next;
                }

                if (obj->next) {
                    obj->next->prev = obj->prev;
                } else {
                    cm_mem.tail = obj->prev;
                }

                cm_mem.total_objects--;
                cm_mem.total_memory -= obj->size;
                cm_mem.frees++;

                free(obj);
            }

            pthread_mutex_unlock(&cm_mem.gc_lock);
            return;
        }
        prev_hash = obj;
        obj = obj->hash_next;
    }

    pthread_mutex_unlock(&cm_mem.gc_lock);
}

void cm_gc_collect(void) {
    pthread_mutex_lock(&cm_mem.gc_lock);

    cm_printf("[GC] Starting collection...\n");

    for (CMObject* obj = cm_mem.head; obj; obj = obj->next) {
        // Objects with ref_count == 0 (e.g., after cm_untrack) 
        // will be marked 0 and dynamically swept and freed below.
        obj->marked = (obj->ref_count > 0) ? 1 : 0;
    }

    CMObject* current = cm_mem.head;
    size_t freed_memory = 0;
    int freed_objects = 0;

    while (current) {
        CMObject* next = current->next;

        if (!current->marked) {
            freed_memory += current->size;
            freed_objects++;

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

            if (current->destructor) {
                current->destructor(current->ptr);
            }
            free(current->ptr);

            if (current->prev) {
                current->prev->next = current->next;
            } else {
                cm_mem.head = current->next;
            }

            if (current->next) {
                current->next->prev = current->prev;
            } else {
                cm_mem.tail = current->prev;
            }

            cm_mem.total_objects--;
            cm_mem.total_memory -= current->size;
            cm_mem.frees++;

            free(current);
        }

        current = next;
    }
    cm_mem.gc_last_collection = freed_memory;
    cm_mem.collections++;

cm_printf("[GC] Completed: freed %d objects (%zu bytes)\n", freed_objects, freed_memory);

    pthread_mutex_unlock(&cm_mem.gc_lock);
}

/*
 * logic: prints statistical dumps mapping arena metrics globally accurately tracking lengths inherently
 * calling: cm_gc_stats();
 */
void cm_gc_stats(void) {
    pthread_mutex_lock(&cm_mem.gc_lock);

    cm_printf("\n");
cm_printf("══════════════════════════════════════════════════════════════\n");
cm_printf("              GARBAGE COLLECTOR STATISTICS\n");
cm_printf("──────────────────────────────────────────────────────────────\n");
cm_printf("  Total objects    │ %20zu\n", cm_mem.total_objects);
cm_printf("  Total memory     │ %20zu bytes\n", cm_mem.total_memory);
cm_printf("  Peak memory      │ %20zu bytes\n", cm_mem.peak_memory);
cm_printf("  Allocations      │ %20zu\n", cm_mem.allocations);
cm_printf("  Frees            │ %20zu\n", cm_mem.frees);
cm_printf("  Collections      │ %20zu\n", cm_mem.collections);
cm_printf("──────────────────────────────────────────────────────────────\n");
cm_printf("  Avg collection   │ %19.3f ms\n", cm_mem.avg_collection_time * 1000);
cm_printf("  Last freed       │ %20zu bytes\n", cm_mem.gc_last_collection);
if (cm_mem.current_arena) {
    cm_printf("──────────────────────────────────────────────────────────────\n");
    cm_printf("  ARENA STATISTICS\n");
    cm_printf("  Arena name       │ %20s\n", cm_mem.current_arena->name);
    cm_printf("  Arena size       │ %20zu bytes\n", cm_mem.current_arena->block_size);
    cm_printf("  Arena used       │ %20zu bytes\n", cm_mem.current_arena->offset);
    cm_printf("  Arena peak       │ %20zu bytes\n", cm_mem.current_arena->peak_usage);
}
cm_printf("══════════════════════════════════════════════════════════════\n");
if (cm_mem.total_objects > 0 && CM_LOG_LEVEL >= 3) {
    cm_printf("\nACTIVE OBJECTS:\n");
    cm_printf("──────────────────────────────────────────────────────────────\n");

int i=0;
    for (CMObject* obj = cm_mem.head; obj; obj = obj->next) {
        cm_printf("  [%d] %s (%zu bytes) at %s:%d [refs: %d]\n",
                  ++i, obj->type ? obj->type : "unknown",
                  obj->size, obj->file ? obj->file : "unknown",
                  obj->line, obj->ref_count);
    }
}

    pthread_mutex_unlock(&cm_mem.gc_lock);
}

/*
 * logic: evaluates tracking variables incrementing dependencies seamlessly 
 * calling: cm_retain(ptr);
 */
void cm_retain(void* ptr) {
    if (!ptr) return;

    pthread_mutex_lock(&cm_mem.gc_lock);

    uint32_t h = cm_hash_ptr(ptr) % CM_GC_HASH_SIZE;
    CMObject* obj = cm_mem.obj_hash[h];

    while (obj) {
        if (obj->ptr == ptr) {
            obj->ref_count++;
            break;
        }
        obj = obj->hash_next;
    }

    pthread_mutex_unlock(&cm_mem.gc_lock);
}

// Usage: cm_untrack(p); cm_gc_collect(); -> p gets freed by the GC
// Usage: cm_untrack(p); // p still valid, just untracked
void cm_untrack(void* ptr) {
    if (!ptr) return;

    pthread_mutex_lock(&cm_mem.gc_lock);

    uint32_t h = cm_hash_ptr(ptr) % CM_GC_HASH_SIZE;
    CMObject* prev_hash = NULL;
    CMObject* obj = cm_mem.obj_hash[h];

    while (obj) {
        if (obj->ptr == ptr) {
            
            // Remove from O(1) hash chain
            if (prev_hash) prev_hash->hash_next = obj->hash_next;
            else cm_mem.obj_hash[h] = obj->hash_next;

            // Remove from doubly linked list
            if (obj->prev) {
                obj->prev->next = obj->next;
            } else {
                cm_mem.head = obj->next;
            }

            if (obj->next) {
                obj->next->prev = obj->prev;
            } else {
                cm_mem.tail = obj->prev;
            }

            // Update stats
            cm_mem.total_objects--;
            cm_mem.total_memory -= obj->size;

            free(obj);
            cm_mem.frees++;  // track untrack operations in stats
            
            break;
        }
        prev_hash = obj;
        obj = obj->hash_next;
    }

    pthread_mutex_unlock(&cm_mem.gc_lock);
}

/*
 * logic: isolates contexts systematically freeing tracking trails globally implicitly 
 * calling: automatically triggers terminating bounds natively 
 */
void cm_arena_cleanup(void* ptr) {
    CMArena** arena_ptr = (CMArena**)ptr;
    if (*arena_ptr) {
        pthread_mutex_lock(&cm_mem.arena_lock);
        if (cm_mem.current_arena == *arena_ptr) {
            cm_mem.current_arena = NULL;
        }
        pthread_mutex_unlock(&cm_mem.arena_lock);

        cm_arena_destroy(*arena_ptr);
        *arena_ptr = NULL;
    }
}



/* ============================================================================
 * ERROR HANDLING IMPLEMENTATION
 * ============================================================================ */

/*
 * logic: bridges codes generating logical formats systematically seamlessly
 * calling: printf("%s", cm_error_string(1));
 */
const char* cm_error_string(int error) {
    switch(error) {
        case CM_SUCCESS: return "Success";
        case CM_ERROR_MEMORY: return "Memory allocation failed";
        case CM_ERROR_NULL_POINTER: return "Null pointer dereference";
        case CM_ERROR_OUT_OF_BOUNDS: return "Index out of bounds";
        case CM_ERROR_DIVISION_BY_ZERO: return "Division by zero";
        case CM_ERROR_OVERFLOW: return "Integer overflow";
        case CM_ERROR_UNDERFLOW: return "Integer underflow";
        case CM_ERROR_INVALID_ARGUMENT: return "Invalid argument";
        case CM_ERROR_NOT_FOUND: return "Not found";
        case CM_ERROR_ALREADY_EXISTS: return "Already exists";
        case CM_ERROR_PERMISSION_DENIED: return "Permission denied";
        case CM_ERROR_IO: return "I/O error";
        case CM_ERROR_NETWORK: return "Network error";
        case CM_ERROR_TIMEOUT: return "Operation timed out";
        case CM_ERROR_THREAD: return "Thread error";
        case CM_ERROR_SYNC: return "Synchronization error";
        case CM_ERROR_PARSE: return "Parse error";
        case CM_ERROR_TYPE: return "Type error";
        case CM_ERROR_UNIMPLEMENTED: return "Unimplemented";
        case CM_ERROR_UNKNOWN: return "Unknown error";
        default: return "Unknown error code";
    }
}

/*
 * logic: queries native global states parsing strings inherently securely
 * calling: msg = cm_error_get_message();
 */
const char* cm_error_get_message(void) {
    return cm_error_message;
}

/*
 * logic: fetches numeric constants evaluating bounds globally explicitly safely
 * calling: code = cm_error_get_last();
 */
int cm_error_get_last(void) {
    return cm_last_error;
}

/*
 * logic: purges errors handling system logic dynamically inherently logically
 * calling: cm_error_clear();
 */
void cm_error_clear(void) {
    cm_last_error = 0;
    cm_error_message[0] = '\0';
}

/*
 * logic: manipulates traces triggering exceptions logically accurately manually securely
 * calling: cm_error_set(1, "Out of bounds");
 */
void cm_error_set(int error, const char* message) {
    cm_last_error = error;
    if (message) {
        strncpy(cm_error_message, message, sizeof(cm_error_message) - 1);
        cm_error_message[sizeof(cm_error_message) - 1] = '\0';
    }
    
    if (CM_LOG_LEVEL >= 3 && cm_current_frame) {
        printf("[EXCEPTION] Error %d set in thread %lu at %s:%d\n", 
               error, (unsigned long)pthread_self(),
               cm_current_frame->file, cm_current_frame->line);
    }
}

/* ============================================================================
 * STRING IMPLEMENTATION
 * ============================================================================ */
#define CM_STRING_COPY 0x01
#define CM_STRING_NOCOPY 0x02

/*
 * logic: provisions character wrappers defining native string allocations smoothly
 * calling: str = cm_string_new("example");
 */
cm_string_t* cm_string_new(const char* initial) {
    cm_string_t* s = (cm_string_t*)cm_alloc(sizeof(cm_string_t), "string", __FILE__, __LINE__);
    if (!s) return NULL;

    size_t len = initial ? strlen(initial) : 0;
    s->length = len;
    s->capacity = len + 1;
    s->data = (char*)cm_alloc(s->capacity, "string_data", __FILE__, __LINE__);
    s->ref_count = 1;
    s->hash = 0;
    s->created = time(NULL);
    s->flags = CM_STRING_COPY;

    if (s->data) {
        if (initial && len > 0) {
            memcpy(s->data, initial, len + 1);
        } else {
            s->data[0] = '\0';
        }
    }

    return s;
}

/*
 * logic: handles deletions removing tracks safely naturally explicitly  
 * calling: cm_string_free(str);
 */
void cm_string_free(cm_string_t* s) {
    if (!s) return;

    s->ref_count--;
    if (s->ref_count <= 0) {
        if (s->data && !(s->flags & CM_STRING_NOCOPY)) {
            cm_free(s->data);
        }
        cm_free(s);
    }
}

/*
 * logic: accesses sizes measuring properties clearly internally naturally  
 * calling: max = cm_string_length(str);
 */
size_t cm_string_length(cm_string_t* s) {
    return s ? s->length : 0;
}

/*
 * logic: spans formats injecting attributes accurately comprehensively strictly
 * calling: str = cm_string_format("data: %s", "demo");
 */
cm_string_t* cm_string_format(const char* format, ...) {
    if (!format) return NULL;

    va_list args;
    va_start(args, format);

    int size = vsnprintf(NULL, 0, format, args);
    va_end(args);

    if (size < 0) return NULL;

    char* buffer = (char*)malloc(size + 1);
    if (!buffer) return NULL;

    va_start(args, format);
    vsnprintf(buffer, size + 1, format, args);
    va_end(args);

    cm_string_t* result = cm_string_new(buffer);
    free(buffer);

    return result;
}

/*
 * logic: bridges variable inputs adapting buffers securely naturally smoothly
 * calling: cm_string_set(str, "data");
 */
void cm_string_set(cm_string_t* s, const char* value) {
    if (!s) return;
    if (!value) value = "";

    size_t len = strlen(value);

    if (len + 1 > s->capacity || (s->flags & CM_STRING_NOCOPY)) {
        char* new_data = (char*)cm_alloc(len + 1, "string_data", __FILE__, __LINE__);
        if (!new_data) return;

        if (s->data && !(s->flags & CM_STRING_NOCOPY)) {
            cm_free(s->data);
        }

        s->data = new_data;
        s->capacity = len + 1;
        s->flags &= ~CM_STRING_NOCOPY;
    }

    memcpy(s->data, value, len + 1);
    s->length = len;
    s->hash = 0;
}

/*
 * logic: loops characters modifying attributes rendering arrays uppercase naturally
 * calling: cm_string_upper(str);
 */
void cm_string_upper(cm_string_t* s) {
    if (!s || !s->data) return;

    for (size_t i = 0; i < s->length; i++) {
        s->data[i] = toupper(s->data[i]);
    }
    s->hash = 0;
}

/*
 * logic: iterates pointers morphing attributes securely lowering sets naturally
 * calling: cm_string_lower(str);
 */
void cm_string_lower(cm_string_t* s) {
    if (!s || !s->data) return;

    for (size_t i = 0; i < s->length; i++) {
        s->data[i] = tolower(s->data[i]);
    }
    s->hash = 0;
}

/* ============================================================================
 * ARRAY IMPLEMENTATION
 * ============================================================================ */

/*
 * logic: defines array bounds reserving scopes globally gracefully smoothly
 * calling: arr = cm_array_new(sizeof(int), 10);
 */
cm_array_t* cm_array_new(size_t element_size, size_t initial_capacity) {
    if (element_size == 0) return NULL;

    cm_array_t* arr = (cm_array_t*)cm_alloc(sizeof(cm_array_t), "array", __FILE__, __LINE__);
    if (!arr) return NULL;

    arr->element_size = element_size;
    arr->capacity = initial_capacity > 0 ? initial_capacity : 16;
    arr->length = 0;
    arr->data = cm_alloc(element_size * arr->capacity, "array_data", __FILE__, __LINE__);
    arr->ref_counts = (int*)cm_alloc(sizeof(int) * arr->capacity, "array_refs", __FILE__, __LINE__);
    arr->element_destructor = NULL;
    arr->flags = 0;

    if (!arr->data || !arr->ref_counts) {
        if (arr->data) cm_free(arr->data);
        if (arr->ref_counts) cm_free(arr->ref_counts);
        cm_free(arr);
        return NULL;
    }

    memset(arr->ref_counts, 0, sizeof(int) * arr->capacity);

    return arr;
}

/*
 * logic: removes array instances terminating constraints actively permanently 
 * calling: cm_array_free(arr);
 */
void cm_array_free(cm_array_t* arr) {
    if (!arr) return;

    if (arr->element_destructor) {
        for (size_t i = 0; i < arr->length; i++) {
            void* elem = (char*)arr->data + (i * arr->element_size);
            arr->element_destructor(elem);
        }
    }

    cm_free(arr->data);
    cm_free(arr->ref_counts);
    cm_free(arr);
}

/*
 * logic: isolates targets addressing internal pointers exactly quickly 
 * calling: ptr = cm_array_get(arr, 3);
 */
void* cm_array_get(cm_array_t* arr, size_t index) {
    if (!arr) return NULL;
    if (index >= arr->length) return NULL;

    arr->ref_counts[index]++;
    return (char*)arr->data + (index * arr->element_size);
}

/*
 * logic: adapts capacities seamlessly appending items accurately internally natively
 * calling: cm_array_push(arr, ptr);
 */
void cm_array_push(cm_array_t* arr, const void* value) {
    if (!arr || !value) return;

    if (arr->length >= arr->capacity) {
        size_t new_capacity = arr->capacity * 2;
        void* new_data = cm_alloc(arr->element_size * new_capacity, "array_data", __FILE__, __LINE__);
        int* new_refs = (int*)cm_alloc(sizeof(int) * new_capacity, "array_refs", __FILE__, __LINE__);

        if (!new_data || !new_refs) {
            if (new_data) cm_free(new_data);
            if (new_refs) cm_free(new_refs);
            return;
        }

        memcpy(new_data, arr->data, arr->element_size * arr->length);
        memset(new_refs, 0, sizeof(int) * new_capacity);

        cm_free(arr->data);
        cm_free(arr->ref_counts);

        arr->data = new_data;
        arr->ref_counts = new_refs;
        arr->capacity = new_capacity;
    }

    void* dest = (char*)arr->data + (arr->length * arr->element_size);
    memcpy(dest, value, arr->element_size);
    arr->length++;
}

/*
 * logic: accesses recent elements returning blocks removing constraints trailing explicitly
 * calling: ptr = cm_array_pop(arr);
 */
void* cm_array_pop(cm_array_t* arr) {
    if (!arr || arr->length == 0) return NULL;

    arr->length--;
    return (char*)arr->data + (arr->length * arr->element_size);
}

/*
 * logic: maps count metrics reading boundary instances effectively rapidly  
 * calling: max = cm_array_length(arr);
 */
size_t cm_array_length(cm_array_t* arr) {
    return arr ? arr->length : 0;
}

/* ============================================================================
 * MAP IMPLEMENTATION
 * ============================================================================ */
#define CM_MAP_INITIAL_SIZE 16
#define CM_MAP_LOAD_FACTOR 0.75

/*
 * logic: transforms strings mathematically hashing results deterministically  
 * calling: internal function mapping keys systematically inherently
 */
static uint32_t cm_hash_string(const char* str) {
    uint32_t hash = 5381;
    int c;

    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }

    return hash;
}

/*
 * logic: roots map tracking organizing table constraints natively cleanly  
 * calling: m = cm_map_new();
 */
cm_map_t* cm_map_new(void) {
    cm_map_t* map = (cm_map_t*)cm_alloc(sizeof(cm_map_t), "map", __FILE__, __LINE__);
    if (!map) return NULL;

    map->bucket_count = CM_MAP_INITIAL_SIZE;
    map->size = 0;
    map->load_factor = CM_MAP_LOAD_FACTOR;
    map->growth_factor = 2;
    map->buckets = (cm_map_entry_t**)cm_alloc(sizeof(cm_map_entry_t*) * map->bucket_count, 
                                               "map_buckets", __FILE__, __LINE__);

    if (!map->buckets) {
        cm_free(map);
        return NULL;
    }

    memset(map->buckets, 0, sizeof(cm_map_entry_t*) * map->bucket_count);

    return map;
}

/*
 * logic: routes entries handling larger sizes dynamically maintaining buckets cleanly inherently  
 * calling: internal use expanding boundaries securely inherently
 */
static void cm_map_resize(cm_map_t* map, size_t new_size) {
    if (!map) return;

    if (new_size == 0 || new_size > SIZE_MAX / sizeof(cm_map_entry_t*)) {
        return;
    }

    cm_map_entry_t** new_buckets = (cm_map_entry_t**)cm_alloc(
        sizeof(cm_map_entry_t*) * new_size, "map_buckets", __FILE__, __LINE__);

    if (!new_buckets) return;

    memset(new_buckets, 0, sizeof(cm_map_entry_t*) * new_size);

    for (int i = 0; i < map->bucket_count; i++) {
        cm_map_entry_t* entry = map->buckets[i];
        while (entry) {
            cm_map_entry_t* next = entry->next;
            size_t new_index = entry->hash % new_size;
            entry->next = new_buckets[new_index];
            new_buckets[new_index] = entry;
            entry = next;
        }
    }

    cm_free(map->buckets);
    map->buckets = new_buckets;
    map->bucket_count = (int)new_size;
}



/*
 * logic: integrates keys associating internal structures tracking bytes securely  
 * calling: cm_map_set(m, "id", ptr, 4);
 */
void cm_map_set(cm_map_t* map, const char* key, const void* value, size_t value_size) {
    if (!map || !key || !value) return;

    if (map->bucket_count > 0) {
        double needed_capacity = (double)map->bucket_count * (double)map->load_factor;
        if (needed_capacity > (double)INT_MAX) {
            return;
        }
        
        if ((double)map->size >= needed_capacity) {
            size_t new_size = (size_t)map->bucket_count * (size_t)map->growth_factor;
            
            if (new_size > 0 && new_size <= SIZE_MAX / sizeof(cm_map_entry_t*)) {
                cm_map_resize(map, new_size);
            } else {
                return;
            }
        }
    }

    uint32_t hash = cm_hash_string(key);
    int index = hash % map->bucket_count;

    cm_map_entry_t* entry = map->buckets[index];
    while (entry) {
        if (entry->hash == hash && strcmp(entry->key, key) == 0) {
            cm_free(entry->value);
            
            entry->value = cm_alloc(value_size, "map_value", __FILE__, __LINE__);
            if (!entry->value) {
                cm_last_error = CM_ERROR_MEMORY;
                return;
            }
            
            memcpy(entry->value, value, value_size);
            entry->value_size = value_size;
            return;
        }
        entry = entry->next;
    }

    entry = (cm_map_entry_t*)cm_alloc(sizeof(cm_map_entry_t), "map_entry", __FILE__, __LINE__);
    if (!entry) {
        cm_last_error = CM_ERROR_MEMORY;
        return;
    }
    
    size_t key_len = strlen(key) + 1;
    entry->key = (char*)cm_alloc(key_len, "map_key", __FILE__, __LINE__);
    if (!entry->key) {
        cm_free(entry);
        cm_last_error = CM_ERROR_MEMORY;
        return;
    }
    memcpy(entry->key, key, key_len);

    entry->value = cm_alloc(value_size, "map_value", __FILE__, __LINE__);
    if (!entry->value) {
        cm_free(entry->key);
        cm_free(entry);
        cm_last_error = CM_ERROR_MEMORY;
        return;
    }
    memcpy(entry->value, value, value_size);

    entry->value_size = value_size;
    entry->hash = hash;
    entry->next = map->buckets[index];

    map->buckets[index] = entry;
    map->size++;
}

/*
 * logic: finds values searching bucket chains securely efficiently 
 * calling: ptr = cm_map_get(m, "id");
 */
void* cm_map_get(cm_map_t* map, const char* key) {
    if (!map || !key) return NULL;

    uint32_t hash = cm_hash_string(key);
    int index = hash % map->bucket_count;

    cm_map_entry_t* entry = map->buckets[index];
    while (entry) {
        if (entry->hash == hash && strcmp(entry->key, key) == 0) {
            return entry->value;
        }
        entry = entry->next;
    }

    return NULL;
}

/*
 * logic: checks inputs evaluating properties accurately smoothly boolean
 * calling: if (cm_map_has(m, "id")) check();
 */
int cm_map_has(cm_map_t* map, const char* key) {
    return cm_map_get(map, key) != NULL;
}

/*
 * logic: erases table structs iterating references natively actively smoothly
 * calling: cm_map_free(m);
 */
void cm_map_free(cm_map_t* map) {
    if (!map) return;
    
    for (int i = 0; i < map->bucket_count; i++) {
        cm_map_entry_t* entry = map->buckets[i];
        while (entry) {
            cm_map_entry_t* next = entry->next;
            
            if (entry->key) {
                cm_free(entry->key);
            }
            if (entry->value) {
                cm_free(entry->value);
            }
            cm_free(entry);
            
            entry = next;
        }
    }
    
    if (map->buckets) {
        cm_free(map->buckets);
    }
    cm_free(map);
}

/*
 * logic: queries active dimensions returning total sizes safely natively 
 * calling: max = cm_map_size(m);
 */
size_t cm_map_size(cm_map_t* map) {
    return map ? (size_t)map->size : 0;
}

/* ============================================================================
 * UTILITY IMPLEMENTATION
 * ============================================================================ */
/*
 * logic: grounds global sequence variables adapting deterministic parameters 
 * calling: cm_random_seed(1024);
 */
void cm_random_seed(unsigned int seed) {
    srand(seed);
}

#ifdef __linux__
#include <sys/random.h>
#endif

/*
 * logic: draws letters filling character buffers generating string sequences smoothly  
 * calling: cm_random_string(buffer, 32);
 */
void cm_random_string(char* buffer, size_t length) {
    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    size_t charset_size = sizeof(charset) - 1;

    if (!buffer || length == 0) return;

    for (size_t i = 0; i < length - 1; i++) {
        unsigned int random_value;
        
        #ifdef _WIN32
        HCRYPTPROV hCryptProv;
        if (CryptAcquireContext(&hCryptProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
            CryptGenRandom(hCryptProv, sizeof(random_value), (BYTE*)&random_value);
            CryptReleaseContext(hCryptProv, 0);
        } else {
            random_value = (unsigned int)rand();
        }
        #else
        random_value = (unsigned int)rand();
        #endif

        if (charset_size > 0) {
            buffer[i] = charset[random_value % charset_size];
        } else {
            buffer[i] = 'A';
        }
    }
    buffer[length - 1] = '\0';
}

/* ============================================================================
 * CLASS IMPLEMENTATION - OOP
 * ============================================================================ */

/*
 * logic: appends new characters securely extending native strings dynamically
 * calling: obj->concat(obj, "new_part");
 */
String* string_concat(String* self, const char* other) {
    printf("DEBUG: string_concat called: self='%s', other='%s'\n", 
           self && self->data ? self->data : "NULL", 
           other ? other : "NULL");
    
    if (!self) {
        printf("DEBUG: self is NULL\n");
        return NULL;
    }
    
    if (!other) {
        printf("DEBUG: other is NULL\n");
        return self;
    }
    
    if (!self->data) {
        printf("DEBUG: self->data is NULL\n");
        return self;
    }
    
    int new_len = self->length + (int)strlen(other);
    printf("DEBUG: new_len = %d\n", new_len);
    
    char* new_data = (char*)cm_alloc(new_len + 1, "string_data", __FILE__, __LINE__);
    
    if (!new_data) {
        printf("DEBUG: Failed to allocate new_data\n");
        cm_last_error = CM_ERROR_MEMORY;
        return self;
    }
    
    strcpy(new_data, self->data);
    strcat(new_data, other);
    printf("DEBUG: new_data = '%s'\n", new_data);
    
    printf("DEBUG: Freeing old data at %p\n", (void*)self->data);
    cm_free(self->data);
    
    self->data = new_data;
    self->length = new_len;
    
    printf("DEBUG: Concat successful, result = '%s'\n", self->data);
    return self;
}

/*
 * logic: shifts object properties rendering inputs universally capital iteratively 
 * calling: obj->upper(obj);
 */
String* string_upper(String* self) {
    if (!self || !self->data) return self;
    for (int i = 0; i < self->length; i++) {
        self->data[i] = toupper(self->data[i]);
    }
    return self;
}

/*
 * logic: adapts string instances resolving character states explicitly lowercase 
 * calling: obj->lower(obj);
 */
String* string_lower(String* self) {
    if (!self || !self->data) return self;
    for (int i = 0; i < self->length; i++) {
        self->data[i] = tolower(self->data[i]);
    }
    return self;
}

/*
 * logic: formats string properties outputting variables transparently directly natively 
 * calling: obj->print(obj);
 */
void string_print(String* self) {
    if (self && self->data) {
        printf("%s", self->data);
        fflush(stdout);
    }
}

/*
 * logic: fetches current constraints determining variables physically  
 * calling: num = obj->length_func(obj);
 */
int string_length(String* self) {
    return self ? self->length : 0;
}

/*
 * logic: accesses isolated targets routing specific inputs efficiently actively  
 * calling: ch = obj->charAt(obj, 2);
 */
char string_charAt(String* self, int index) {
    if (!self || !self->data || index < 0 || index >= self->length) return '\0';
    return self->data[index];
}

/*
 * logic: initiates tracking wrapping string bytes inside object layouts dynamically natively 
 * calling: obj = String_new("init");
 */
String* String_new(const char* initial) {
    printf("DEBUG: String_new called with initial='%s'\n", initial ? initial : "NULL");
    
    String* self = (String*)cm_alloc(sizeof(String), "String", __FILE__, __LINE__);
    if (!self) {
        printf("DEBUG: Failed to allocate String struct\n");
        cm_last_error = CM_ERROR_MEMORY;
        return NULL;
    }
    
    printf("DEBUG: String struct allocated at %p\n", (void*)self);
    
    int len = initial ? (int)strlen(initial) : 0;
    self->length = len;
    self->capacity = len + 1;
    
    printf("DEBUG: Allocating data of size %d\n", self->capacity);
    self->data = (char*)cm_alloc((size_t)self->capacity, "string_data", __FILE__, __LINE__);
    
    if (!self->data) {
        printf("DEBUG: Failed to allocate string data\n");
        cm_free(self);
        cm_last_error = CM_ERROR_MEMORY;
        return NULL;
    }
    
    if (initial && len > 0) {
        strcpy(self->data, initial);
        printf("DEBUG: Copied initial string: '%s'\n", self->data);
    } else {
        self->data[0] = '\0';
        printf("DEBUG: Empty string created\n");
    }
    
    self->concat = string_concat;
    self->upper = string_upper;
    self->lower = string_lower;
    self->print = string_print;
    self->length_func = string_length;
    self->charAt = string_charAt;
    
    printf("DEBUG: String created successfully at %p\n", (void*)self);
    return self;
}

/*
 * logic: destroys object boundaries reverting fields cleanly aggressively natively  
 * calling: String_delete(obj);
 */
void String_delete(String* self) {
    if (!self) return;
    if (self->data) cm_free(self->data);
    cm_free(self);
}

/*
 * logic: links inputs securely extending internal blocks systematically clearly  
 * calling: obj->push(obj, value);
 */
Array* array_push(Array* self, void* value) {
    if (!self || !value) return self;

    if (self->length >= self->capacity) {
        size_t new_cap = (size_t)self->capacity * 2;

        if (new_cap > (size_t)(INT_MAX / self->element_size)) {
            cm_last_error = CM_ERROR_OVERFLOW;
            return self;
        }

        void* new_data = cm_alloc(self->element_size * new_cap, "array_data", __FILE__, __LINE__);
        if (!new_data) {
            cm_last_error = CM_ERROR_MEMORY;
            return self;
        }

        memcpy(new_data, self->data, self->element_size * self->length);
        cm_free(self->data);
        self->data = new_data;
        self->capacity = (int)new_cap;
    }

    void* dest = (char*)self->data + (self->length * self->element_size);
    memcpy(dest, value, self->element_size);
    self->length++;
    return self;
}

/*
 * logic: removes inputs returning elements parsing lengths reliably globally  
 * calling: ptr = obj->pop(obj);
 */
void* array_pop(Array* self) {
    if (!self || self->length == 0) return NULL;
    self->length--;
    return (char*)self->data + (self->length * self->element_size);
}

/*
 * logic: directs addresses tracking metrics smoothly explicitly actively 
 * calling: ptr = obj->get(obj, 2);
 */
void* array_get(Array* self, int index) {
    if (!self || index < 0 || index >= self->length) return NULL;
    return (char*)self->data + (index * self->element_size);
}

/*
 * logic: sizes fields calculating counts actively robustly efficiently  
 * calling: max = obj->size(obj);
 */
int array_size(Array* self) {
    return self ? self->length : 0;
}

/*
 * logic: instantiates list classes tracing metrics internally securely  
 * calling: arr = Array_new(4, 10);
 */
Array* Array_new(int element_size, int capacity) {
    Array* self = (Array*)cm_alloc(sizeof(Array), "Array", __FILE__, __LINE__);

    self->element_size = element_size;
    self->capacity = capacity > 0 ? capacity : 16;
    self->length = 0;
    self->data = cm_alloc(element_size * self->capacity, "array_data", __FILE__, __LINE__);

    self->push = array_push;
    self->pop = array_pop;
    self->get = array_get;
    self->size = array_size;

    return self;
}

/*
 * logic: erases structural elements terminating layouts directly  
 * calling: Array_delete(obj);
 */
void Array_delete(Array* self) {
    if (!self) return;
    if (self->data) cm_free(self->data);
    cm_free(self);
}

/*
 * logic: maps dictionary keys adjusting states natively seamlessly  
 * calling: obj->set(obj, "id", ptr);
 */
Map* map_set(Map* self, const char* key, void* value) {
    if (!self || !key || !value) return self;
    cm_map_set((cm_map_t*)self->map_data, key, value, sizeof(void*));
    self->size = cm_map_size((cm_map_t*)self->map_data);
    return self;
}

/*
 * logic: searches references fetching parameters naturally cleanly  
 * calling: ptr = obj->get(obj, "id");
 */
void* map_get(Map* self, const char* key) {
    if (!self || !key) return NULL;
    return cm_map_get((cm_map_t*)self->map_data, key);
}

/*
 * logic: queries tables matching mappings verifying states safely 
 * calling: if (obj->has(obj, "id")) resolve();
 */
int map_has(Map* self, const char* key) {
    if (!self || !key) return 0;
    return cm_map_has((cm_map_t*)self->map_data, key);
}

/*
 * logic: evaluates limits determining elements implicitly dynamically  
 * calling: num = obj->size_func(obj);
 */
int map_size_func(Map* self) {
    return self ? self->size : 0;
}

/*
 * logic: resolves maps defining fields explicitly naturally cleanly  
 * calling: m = Map_new();
 */
Map* Map_new(void) {
    Map* self = (Map*)cm_alloc(sizeof(Map), "Map", __FILE__, __LINE__);

    self->map_data = cm_map_new();
    self->size = 0;

    self->set = map_set;
    self->get = map_get;
    self->has = map_has;
    self->size_func = map_size_func;

    return self;
}

/*
 * logic: destroys mapping bounds closing classes systematically directly  
 * calling: Map_delete(obj);
 */
void Map_delete(Map* self) {
    if (!self) return;
    if (self->map_data) cm_map_free((cm_map_t*)self->map_data);
    cm_free(self);
}

/* ============================================================================
 * CHTTP IMPLEMENTATION
 * ============================================================================ */

/*
 * logic: boots socket infrastructures checking platform variables reliably securely  
 * calling: internal initialization exclusively invoked silently
 */
static void cm_http_init_winsock() {
#ifdef _WIN32
    static int winsock_initialized = 0;
    if (!winsock_initialized) {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            cm_error("Failed to initialize Winsock.");
        }
        winsock_initialized = 1;
    }
#endif
}

/*
 * logic: translates hostnames establishing networks inherently flawlessly securely  
 * calling: internal function connecting protocols natively smoothly
 */
static int cm_http_connect(const char* hostname, int port) {
    cm_http_init_winsock();
    
    struct hostent* he;
    if ((he = gethostbyname(hostname)) == NULL) {
        cm_error("gethostbyname failed for %s", hostname);
        return -1;
    }
    
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        cm_error("Failed to create socket");
        return -1;
    }
    
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr = *((struct in_addr*)he->h_addr);
    memset(&(server_addr.sin_zero), 0, 8);
    
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(struct sockaddr)) < 0) {
        cm_error("Connection failed to %s:%d", hostname, port);
#ifdef _WIN32
        closesocket(sock);
#else
        close(sock);
#endif
        return -1;
    }
    return sock;
}

/*
 * logic: traverses HTTP signals framing properties strictly linearly dynamically 
 * calling: res = extract_http_response(raw_string);
 */
static CHttpResponse* extract_http_response(const char* response_str) {
    CHttpResponse* res = (CHttpResponse*)cm_alloc(sizeof(CHttpResponse), "CHttpResponse", __FILE__, __LINE__);
    res->status_code = 0;
    res->headers = Map_new();
    res->body = String_new("");
    
    const char* header_end = strstr(response_str, "\r\n\r\n");
    if (!header_end) return res;
    
    // Parse Status line (e.g. HTTP/1.1 200 OK)
    sscanf(response_str, "%*s %d", &res->status_code);
    
    // The rest is body
    res->body->concat(res->body, header_end + 4);
    
    return res;
}

/*
 * logic: queries HTTP structures dispatching GET routing intrinsically inherently  
 * calling: res = cm_http_get("example.com");
 */
CHttpResponse* cm_http_get(const char* url) {
    char hostname[256] = {0};
    char path[1024] = {0};
    
    if (strncmp(url, "http://", 7) == 0) {
        sscanf(url + 7, "%255[^/]/%1023[^\n]", hostname, path);
    } else {
        sscanf(url, "%255[^/]/%1023[^\n]", hostname, path);
    }
    
    if (strlen(path) == 0) strcpy(path, "");
    
    int sock = cm_http_connect(hostname, 80);
    if (sock < 0) return extract_http_response(""); 
    
    char request[2048];
    snprintf(request, sizeof(request), "GET /%s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", path, hostname);
    
    (send)(sock, request, strlen(request), 0);
    
    char buffer[4096];
    char* full_response = (char*)malloc(1);
    full_response[0] = '\0';
    size_t total_len = 0;
    int bytes_received;
    
    while ((bytes_received = recv(sock, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytes_received] = '\0';
        full_response = (char*)realloc(full_response, total_len + bytes_received + 1);
        memcpy(full_response + total_len, buffer, bytes_received + 1);
        total_len += bytes_received;
    }
    
#ifdef _WIN32
    closesocket(sock);
#else
    close(sock);
#endif
    
    CHttpResponse* res = extract_http_response(full_response);
    free(full_response);
    return res;
}

/*
 * logic: connects HTTP routes triggering POST bodies directly systematically internally  
 * calling: res = cm_http_post("api.com/update", "data=1", "application/json");
 */
CHttpResponse* cm_http_post(const char* url, const char* body, const char* content_type) {
    char hostname[256] = {0};
    char path[1024] = {0};
    
    if (strncmp(url, "http://", 7) == 0) {
        sscanf(url + 7, "%255[^/]/%1023[^\n]", hostname, path);
    } else {
        sscanf(url, "%255[^/]/%1023[^\n]", hostname, path);
    }
    if (strlen(path) == 0) strcpy(path, "");
    
    int sock = cm_http_connect(hostname, 80);
    if (sock < 0) return extract_http_response("");
    
    char request[4096];
    snprintf(request, sizeof(request), "POST /%s HTTP/1.1\r\nHost: %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n%s", 
             path, hostname, content_type ? content_type : "application/x-www-form-urlencoded", strlen(body), body);
             
    (send)(sock, request, strlen(request), 0);
    
    char buffer[4096];
    char* full_response = (char*)malloc(1);
    full_response[0] = '\0';
    size_t total_len = 0;
    int bytes_received;
    
    while ((bytes_received = recv(sock, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytes_received] = '\0';
        full_response = (char*)realloc(full_response, total_len + bytes_received + 1);
        memcpy(full_response + total_len, buffer, bytes_received + 1);
        total_len += bytes_received;
    }
    
#ifdef _WIN32
    closesocket(sock);
#else
    close(sock);
#endif
    
    CHttpResponse* res = extract_http_response(full_response);
    free(full_response);
    return res;
}

/*
 * logic: clears response packets completely eradicating payload memory seamlessly 
 * calling: CHttpResponse_delete(response);
 */
void CHttpResponse_delete(CHttpResponse* response) {
    if (!response) return;
    if (response->body) String_delete(response->body);
    if (response->headers) Map_delete(response->headers);
    cm_free(response);
}


/* ============================================================================
 * JSON IMPLEMENTATION
 * ============================================================================ */
static CMJsonNode* parse_json_value(const char** ptr);

static CMJsonNode* parse_json_value(const char** ptr);

/*
 * logic: advances array bounds ignoring invisible byte structures universally internally
 * calling: skip_whitespace(ptr);
 */
static void skip_whitespace(const char** ptr) {
    while (isspace(**ptr)) {
        (*ptr)++;
    }
}

/*
 * logic: generates parsing tokens allocating json nodes directly systematically
 * calling: node = create_json_node(CM_JSON_STRING);
 */
static CMJsonNode* create_json_node(CMJsonType type) {
    CMJsonNode* node = (CMJsonNode*)cm_alloc(sizeof(CMJsonNode), "CMJsonNode", __FILE__, __LINE__);
    if (node) node->type = type;
    return node;
}

/*
 * logic: resolves inline strings extracting quote layouts structurally smoothly 
 * calling: internal function invoked recursively handling JSON bounds
 */
static CMJsonNode* parse_json_string(const char** ptr) {
    (*ptr)++;
    const char* start = *ptr;
    while (**ptr && **ptr != '"') {
        if (**ptr == '\\') (*ptr)++;
        (*ptr)++;
    }
    
    int len = *ptr - start;
    char* buf = (char*)malloc(len + 1);
    strncpy(buf, start, len);
    buf[len] = '\0';
    
    (*ptr)++;
    
    CMJsonNode* node = create_json_node(CM_JSON_STRING);
    node->value.string_val = String_new(buf);
    free(buf);
    return node;
}

/*
 * logic: translates character chunks calculating deterministic numerics securely intuitively
 * calling: internal parsing helper invoked selectively during node extraction
 */
static CMJsonNode* parse_json_number(const char** ptr) {
    char* end;
    double val = strtod(*ptr, &end);
    *ptr = end;
    CMJsonNode* node = create_json_node(CM_JSON_NUMBER);
    node->value.number_val = val;
    return node;
}

/*
 * logic: analyzes keywords translating explicit boundaries returning properties cleanly
 * calling: internal helper determining boolean metrics accurately statically   
 */
static CMJsonNode* parse_json_boolean(const char** ptr) {
    CMJsonNode* node = create_json_node(CM_JSON_BOOLEAN);
    if (strncmp(*ptr, "true", 4) == 0) {
        node->value.boolean_val = 1;
        *ptr += 4;
    } else {
        node->value.boolean_val = 0;
        *ptr += 5;
    }
    return node;
}

/*
 * logic: parses emptiness returning token definitions securely neutrally predictably  
 * calling: helper routing null nodes explicitly statically    
 */
static CMJsonNode* parse_json_null(const char** ptr) {
    CMJsonNode* node = create_json_node(CM_JSON_NULL);
    *ptr += 4;
    return node;
}

/*
 * logic: groups element bindings translating JSON sequences routing nodes cleanly predictably  
 * calling: internal builder extracting continuous array tokens logically 
 */
static CMJsonNode* parse_json_array(const char** ptr) {
    CMJsonNode* node = create_json_node(CM_JSON_ARRAY);
    node->value.array_val = Array_new(sizeof(CMJsonNode*), 4);
    
    (*ptr)++;
    skip_whitespace(ptr);
    
    while (**ptr && **ptr != ']') {
        CMJsonNode* elem = parse_json_value(ptr);
        if (elem) {
            node->value.array_val->push(node->value.array_val, &elem);
        }
        skip_whitespace(ptr);
        if (**ptr == ',') {
            (*ptr)++;
            skip_whitespace(ptr);
        }
    }
    if (**ptr == ']') (*ptr)++;
    return node;
}

/*
 * logic: bounds dictionary formats recursively linking mapped nodes naturally reliably 
 * calling: internal builder iterating map schemas reliably effectively 
 */
static CMJsonNode* parse_json_object(const char** ptr) {
    CMJsonNode* node = create_json_node(CM_JSON_OBJECT);
    node->value.object_val = Map_new();
    
    (*ptr)++;
    skip_whitespace(ptr);
    
    while (**ptr && **ptr != '}') {
        CMJsonNode* key_node = parse_json_string(ptr);
        skip_whitespace(ptr);
        if (**ptr == ':') {
            (*ptr)++;
            skip_whitespace(ptr);
            CMJsonNode* val_node = parse_json_value(ptr);
            if (val_node) {
                node->value.object_val->set(node->value.object_val, key_node->value.string_val->data, &val_node);
            }
        }
        CMJsonNode_delete(key_node);
        
        skip_whitespace(ptr);
        if (**ptr == ',') {
            (*ptr)++;
            skip_whitespace(ptr);
        }
    }
    if (**ptr == '}') (*ptr)++;
    return node;
}

/*
 * logic: branches dynamic extraction logic mapping sequences correctly natively universally   
 * calling: internal hub routing values distinctly transparently securely  
 */
static CMJsonNode* parse_json_value(const char** ptr) {
    skip_whitespace(ptr);
    char c = **ptr;
    if (c == '{') return parse_json_object(ptr);
    if (c == '[') return parse_json_array(ptr);
    if (c == '"') return parse_json_string(ptr);
    if (isdigit(c) || c == '-') return parse_json_number(ptr);
    if (strncmp(*ptr, "true", 4) == 0 || strncmp(*ptr, "false", 5) == 0) return parse_json_boolean(ptr);
    if (strncmp(*ptr, "null", 4) == 0) return parse_json_null(ptr);
    return NULL;
}

/*
 * logic: drives string iterations transforming syntax payloads thoroughly organically safely   
 * calling: node = cm_json_parse("{\"flag\": 1}");
 */
CMJsonNode* cm_json_parse(const char* json_str) {
    if (!json_str) return NULL;
    const char* ptr = json_str;
    return parse_json_value(&ptr);
}

/*
 * logic: drops deep recursive nodes obliterating payload constraints effectively strictly   
 * calling: CMJsonNode_delete(node);
 */
void CMJsonNode_delete(CMJsonNode* node) {
    if (!node) return;
    switch (node->type) {
        case CM_JSON_STRING:
            if (node->value.string_val) String_delete(node->value.string_val);
            break;
        case CM_JSON_ARRAY:
            if (node->value.array_val) {
                for (int i = 0; i < node->value.array_val->size(node->value.array_val); i++) {
                    CMJsonNode* elem = *(CMJsonNode**)node->value.array_val->get(node->value.array_val, i);
                    CMJsonNode_delete(elem);
                }
                Array_delete(node->value.array_val);
            }
            break;
        case CM_JSON_OBJECT:
            if (node->value.object_val) {
                cm_map_t* internal_map = (cm_map_t*)node->value.object_val->map_data;
                for (int i = 0; i < internal_map->bucket_count; i++) {
                    cm_map_entry_t* entry = internal_map->buckets[i];
                    while (entry) {
                        CMJsonNode* val = *(CMJsonNode**)entry->value;
                        CMJsonNode_delete(val);
                        entry = entry->next;
                    }
                }
                Map_delete(node->value.object_val);
            }
            break;
        default: break;
    }
    cm_free(node);
}

/*
 * logic: wraps string outputs transforming internal payloads properly physically safely
 * calling: internal format resolver
 */
static void stringify_json_node(CMJsonNode* node, String* out) {
    if (!node) {
        out->concat(out, "null");
        return;
    }
    char buf[128];
    switch (node->type) {
        case CM_JSON_NULL:
            out->concat(out, "null");
            break;
        case CM_JSON_BOOLEAN:
            out->concat(out, node->value.boolean_val ? "true" : "false");
            break;
        case CM_JSON_NUMBER:
            snprintf(buf, sizeof(buf), "%g", node->value.number_val);
            out->concat(out, buf);
            break;
        case CM_JSON_STRING:
            out->concat(out, "\"");
            out->concat(out, node->value.string_val->data);
            out->concat(out, "\"");
            break;
        case CM_JSON_ARRAY:
            out->concat(out, "[");
            for (int i = 0; i < node->value.array_val->size(node->value.array_val); i++) {
                CMJsonNode* elem = *(CMJsonNode**)node->value.array_val->get(node->value.array_val, i);
                stringify_json_node(elem, out);
                if (i < node->value.array_val->size(node->value.array_val) - 1) {
                    out->concat(out, ",");
                }
            }
            out->concat(out, "]");
            break;
        case CM_JSON_OBJECT: {
            out->concat(out, "{");
            cm_map_t* internal_map = (cm_map_t*)node->value.object_val->map_data;
            int first = 1;
            for (int i = 0; i < internal_map->bucket_count; i++) {
                cm_map_entry_t* entry = internal_map->buckets[i];
                while (entry) {
                    if (!first) out->concat(out, ",");
                    out->concat(out, "\"");
                    out->concat(out, entry->key);
                    out->concat(out, "\":");
                    stringify_json_node(*(CMJsonNode**)entry->value, out);
                    first = 0;
                    entry = entry->next;
                }
            }
            out->concat(out, "}");
            break;
        }
    }
}

/*
 * logic: generates string output wrapping node states naturally visually natively  
 * calling: str = cm_json_stringify(node);
 */
String* cm_json_stringify(CMJsonNode* node) {
    String* out = String_new("");
    stringify_json_node(node, out);
    return out;
}

/* ============================================================================
 * INPUT IMPLEMENTATION
 * ============================================================================ */
/*
 * logic: captures buffered states bypassing blocks handling console queries completely  
 * calling: obj = cm_input("Value: ");
 */
String* cm_input(const char* prompt) {
    if (prompt) {
        cm_printf("%s", prompt);
        fflush(stdout);
    }

    size_t capacity = 128;
    size_t length = 0;
    char* buffer = (char*)malloc(capacity);
    if (!buffer) return String_new("");

    int ch;
    while ((ch = getchar()) != EOF && ch != '\n') {
        if (length + 1 >= capacity) {
            capacity *= 2;
            char* new_buffer = (char*)realloc(buffer, capacity);
            if (!new_buffer) {
                free(buffer);
                return String_new("");
            }
            buffer = new_buffer;
        }
        buffer[length++] = (char)ch;
    }
    buffer[length] = '\0';

    String* result = String_new(buffer);
    free(buffer);
    return result;
}

/* ============================================================================
 * REALTIME ERROR DETECTOR
 * ============================================================================ */

/*
 * logic: catches terminal violations routing output warnings systematically reliably globally  
 * calling: automatically engaged natively through the OS handling interrupts universally 
 */
static void cm_signal_handler(int sig) {
    const char* sig_name = "Unknown Signal";
    switch(sig) {
        case SIGSEGV: sig_name = "SIGSEGV (Segmentation Fault - Null Dereference / Bad Pointer)"; break;
        case SIGABRT: sig_name = "SIGABRT (Abort)"; break;
        case SIGFPE:  sig_name = "SIGFPE (Floating Point Exception - Division by zero)"; break;
        case SIGILL:  sig_name = "SIGILL (Illegal Instruction)"; break;
    }
    
    printf("\n\n\033[1;31m\xE2\x9D\x97 FATAL ERROR INTERCEPTED: %s\033[0m\n", sig_name);
    printf("\033[1;33mGenerating Realtime GC Dump before terminating...\033[0m\n");
    
    // Print memory stats immediately
    cm_gc_stats();
    
    printf("\n\033[1;31mProcess terminated cleanly by CM Realtime Error Detector.\033[0m\n\n");
    exit(1);
}

/*
 * logic: attaches system signals establishing proactive reporting safely definitively  
 * calling: cm_init_error_detector();
 */
void cm_init_error_detector(void) {
    signal(SIGSEGV, cm_signal_handler);
    signal(SIGABRT, cm_signal_handler);
    signal(SIGFPE,  cm_signal_handler);
    signal(SIGILL,  cm_signal_handler);
}

/* ============================================================================
 * INITIALIZATION
 * ============================================================================ */

/*
 * logic: boots overarching structs ensuring system logic initializes systematically purely  
 * calling: constructor invokes logic natively
 */
__attribute__((constructor)) void cm_init_all(void) {
    cm_gc_init();
    cm_init_error_detector();
    cm_random_seed((unsigned int)time(NULL));
    cm_printf("\n🔷 [CM] Library v%s initialized by %s\n", CM_VERSION, CM_AUTHOR);
}

/*
 * logic: purges properties reclaiming structures automatically implicitly uniformly  
 * calling: destructor invokes logic natively smoothly eliminating constraints automatically globally
 */
__attribute__((destructor)) void cm_cleanup_all(void) {
    pthread_mutex_lock(&cm_mem.gc_lock);
    for (CMObject* obj = cm_mem.head; obj; obj = obj->next) {
        obj->ref_count = 0; 
    }
    pthread_mutex_unlock(&cm_mem.gc_lock);

    cm_gc_collect();

    if (cm_mem.total_objects > 0) {
        cm_printf("\n⚠️ [CM] Warning: %zu objects still alive\n", cm_mem.total_objects);
        cm_gc_stats();
    } else {
        cm_printf("\n✅ [CM] Clean shutdown - all memory recovered!\n");
    }

    pthread_mutex_destroy(&cm_mem.gc_lock);
    pthread_mutex_destroy(&cm_mem.arena_lock);
}
