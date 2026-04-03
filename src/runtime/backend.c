#include "curium/backend.h"
#include "curium/http.h"
#include "curium/json.h"
#include "curium/error.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/time.h>
#endif

/* ============================================================================
 * Connection Pool Implementation
 * ========================================================================== */

curium_connection_pool_t* curium_pool_create(size_t initial, size_t max,
                                      int (*create)(void**),
                                      void (*destroy)(void*),
                                      int (*validate)(void*)) {
    curium_connection_pool_t* pool = (curium_connection_pool_t*)curium_alloc(sizeof(curium_connection_pool_t), "conn_pool");
    if (!pool) return NULL;
    
    memset(pool, 0, sizeof(*pool));
    pool->connections = (void**)curium_alloc(sizeof(void*) * max, "conn_pool_array");
    if (!pool->connections) {
        curium_free(pool);
        return NULL;
    }
    
    pool->capacity = max;
    pool->max_size = max;
    pool->create_conn = create;
    pool->destroy_conn = destroy;
    pool->validate_conn = validate;
    pool->lock = curium_mutex_init();
    
    /* Pre-create initial connections */
    for (size_t i = 0; i < initial; i++) {
        void* conn = NULL;
        if (create(&conn) == 0) {
            pool->connections[pool->size++] = conn;
        }
    }
    
    return pool;
}

void curium_pool_destroy(curium_connection_pool_t* pool) {
    if (!pool) return;
    
    curium_mutex_lock(pool->lock);
    for (size_t i = 0; i < pool->size; i++) {
        if (pool->connections[i] && pool->destroy_conn) {
            pool->destroy_conn(pool->connections[i]);
        }
    }
    curium_mutex_unlock(pool->lock);
    
    curium_free(pool->connections);
    curium_mutex_destroy(pool->lock);
    curium_free(pool);
}

void* curium_pool_acquire(curium_connection_pool_t* pool) {
    if (!pool) return NULL;
    
    curium_mutex_lock(pool->lock);
    
    /* Try to get existing connection */
    for (size_t i = 0; i < pool->size; i++) {
        void* conn = pool->connections[i];
        if (conn && pool->validate_conn && pool->validate_conn(conn) == 0) {
            pool->connections[i] = NULL;  /* Mark as in-use */
            curium_mutex_unlock(pool->lock);
            return conn;
        }
    }
    
    /* Create new if under limit */
    if (pool->size < pool->max_size && pool->create_conn) {
        void* conn = NULL;
        if (pool->create_conn(&conn) == 0) {
            pool->size++;
            curium_mutex_unlock(pool->lock);
            return conn;
        }
    }
    
    curium_mutex_unlock(pool->lock);
    return NULL;
}

int curium_pool_release(curium_connection_pool_t* pool, void* conn) {
    if (!pool || !conn) return -1;
    
    curium_mutex_lock(pool->lock);
    
    /* Find empty slot */
    for (size_t i = 0; i < pool->size; i++) {
        if (pool->connections[i] == NULL) {
            pool->connections[i] = conn;
            curium_mutex_unlock(pool->lock);
            return 0;
        }
    }
    
    /* Extend array if needed */
    if (pool->size < pool->capacity) {
        pool->connections[pool->size++] = conn;
        curium_mutex_unlock(pool->lock);
        return 0;
    }
    
    curium_mutex_unlock(pool->lock);
    
    /* No room, destroy */
    if (pool->destroy_conn) {
        pool->destroy_conn(conn);
    }
    return -1;
}

/* ============================================================================
 * Zero-Copy Buffer Implementation
 * ========================================================================== */

curium_zerocopy_buffer_t* curium_buffer_create(size_t capacity) {
    curium_zerocopy_buffer_t* buf = (curium_zerocopy_buffer_t*)curium_alloc(sizeof(curium_zerocopy_buffer_t), "zcbuffer");
    if (!buf) return NULL;
    
    buf->data = (char*)curium_alloc(capacity, "zcbuffer_data");
    if (!buf->data) {
        curium_free(buf);
        return NULL;
    }
    
    buf->capacity = capacity;
    buf->read_pos = 0;
    buf->write_pos = 0;
    buf->ref_count = 1;
    
    return buf;
}

curium_zerocopy_buffer_t* curium_buffer_ref(curium_zerocopy_buffer_t* buf) {
    if (!buf) return NULL;
    /* Note: In real implementation, would use atomic increment */
    buf->ref_count++;
    return buf;
}

void curium_buffer_unref(curium_zerocopy_buffer_t* buf) {
    if (!buf) return;
    /* Note: In real implementation, would use atomic decrement */
    buf->ref_count--;
    if (buf->ref_count <= 0) {
        if (buf->data) curium_free(buf->data);
        curium_free(buf);
    }
}

size_t curium_buffer_write(curium_zerocopy_buffer_t* buf, const void* data, size_t len) {
    if (!buf || !data || len == 0) return 0;
    
    size_t available = buf->capacity - buf->write_pos;
    size_t to_write = (len < available) ? len : available;
    
    memcpy(buf->data + buf->write_pos, data, to_write);
    buf->write_pos += to_write;
    
    return to_write;
}

size_t curium_buffer_read(curium_zerocopy_buffer_t* buf, void* dest, size_t len) {
    if (!buf || !dest || len == 0) return 0;
    
    size_t available = buf->write_pos - buf->read_pos;
    size_t to_read = (len < available) ? len : available;
    
    memcpy(dest, buf->data + buf->read_pos, to_read);
    buf->read_pos += to_read;
    
    return to_read;
}

void curium_buffer_reset(curium_zerocopy_buffer_t* buf) {
    if (!buf) return;
    buf->read_pos = 0;
    buf->write_pos = 0;
}

/* ============================================================================
 * String Interning Pool
 * ========================================================================== */

curium_string_pool_t* curium_string_pool_create(void) {
    curium_string_pool_t* pool = (curium_string_pool_t*)curium_alloc(sizeof(curium_string_pool_t), "string_pool");
    if (!pool) return NULL;
    
    pool->table = curium_map_new();
    pool->lock = curium_mutex_init();
    
    return pool;
}

void curium_string_pool_destroy(curium_string_pool_t* pool) {
    if (!pool) return;
    
    curium_mutex_lock(pool->lock);
    /* Free all interned strings */
    curium_map_free(pool->table);
    curium_mutex_unlock(pool->lock);
    
    curium_mutex_destroy(pool->lock);
    curium_free(pool);
}

const char* curium_string_pool_intern(curium_string_pool_t* pool, const char* str) {
    if (!pool || !str) return NULL;
    
    curium_mutex_lock(pool->lock);
    
    /* Check if already interned */
    curium_string_t** existing = (curium_string_t**)curium_map_get(pool->table, str);
    if (existing) {
        curium_mutex_unlock(pool->lock);
        return (*existing)->data;
    }
    
    /* Create new interned string */
    curium_string_t* interned = curium_string_new(str);
    curium_map_set(pool->table, str, &interned, sizeof(curium_string_t*));
    
    curium_mutex_unlock(pool->lock);
    return interned->data;
}

/* ============================================================================
 * Worker Thread Pool (Basic Implementation)
 * ========================================================================== */

struct curium_worker_pool {
    CMThread* workers;
    size_t num_workers;
    CMMutex queue_lock;
    /* Simple task queue - in production, use lock-free queue */
    void** tasks;
    size_t task_count;
    size_t task_capacity;
    int shutdown;
};

static void* curium_worker_thread(void* arg) {
    curium_worker_pool_t* pool = (curium_worker_pool_t*)arg;
    
    while (!pool->shutdown) {
        curium_mutex_lock(pool->queue_lock);
        
        if (pool->task_count > 0) {
            /* Get task */
            void* task = pool->tasks[0];
            /* Shift queue */
            for (size_t i = 0; i < pool->task_count - 1; i++) {
                pool->tasks[i] = pool->tasks[i + 1];
            }
            pool->task_count--;
            curium_mutex_unlock(pool->queue_lock);
            
            /* Execute task */
            curium_worker_task_t* task_fn = (curium_worker_task_t*)task;
            if (task_fn) {
                (*task_fn)(NULL);  /* Task arg would come from task struct */
            }
        } else {
            curium_mutex_unlock(pool->queue_lock);
            /* Small sleep to prevent busy-waiting */
#ifdef _WIN32
            Sleep(1);
#else
            usleep(1000);
#endif
        }
    }
    
    return NULL;
}

curium_worker_pool_t* curium_worker_pool_create(size_t num_workers) {
    curium_worker_pool_t* pool = (curium_worker_pool_t*)curium_alloc(sizeof(curium_worker_pool_t), "worker_pool");
    if (!pool) return NULL;
    
    memset(pool, 0, sizeof(*pool));
    pool->num_workers = num_workers;
    pool->workers = (CMThread*)curium_alloc(sizeof(CMThread) * num_workers, "worker_threads");
    pool->tasks = (void**)curium_alloc(sizeof(void*) * 1024, "task_queue");
    pool->task_capacity = 1024;
    pool->queue_lock = curium_mutex_init();
    
    /* Start worker threads */
    for (size_t i = 0; i < num_workers; i++) {
        pool->workers[i] = curium_thread_create(curium_worker_thread, pool);
    }
    
    return pool;
}

void curium_worker_pool_destroy(curium_worker_pool_t* pool) {
    if (!pool) return;
    
    /* Signal shutdown */
    pool->shutdown = 1;
    
    /* Wait for workers */
    for (size_t i = 0; i < pool->num_workers; i++) {
        curium_thread_join(pool->workers[i]);
    }
    
    curium_free(pool->workers);
    curium_free(pool->tasks);
    curium_mutex_destroy(pool->queue_lock);
    curium_free(pool);
}

int curium_worker_pool_submit(curium_worker_pool_t* pool, curium_worker_task_t task, void* arg) {
    if (!pool || !task) return -1;
    (void)arg; /* Not used in simplified implementation */
    
    curium_mutex_lock(pool->queue_lock);
    
    if (pool->task_count >= pool->task_capacity) {
        curium_mutex_unlock(pool->queue_lock);
        return -1;  /* Queue full */
    }
    
    /* Store task using union to avoid strict aliasing issues */
    union { curium_worker_task_t fn; void* ptr; } converter;
    converter.fn = task;
    pool->tasks[pool->task_count++] = converter.ptr;
    
    curium_mutex_unlock(pool->queue_lock);
    return 0;
}

void curium_worker_pool_wait_all(curium_worker_pool_t* pool) {
    if (!pool) return;
    
    /* Wait for queue to empty */
    while (1) {
        curium_mutex_lock(pool->queue_lock);
        int empty = (pool->task_count == 0);
        curium_mutex_unlock(pool->queue_lock);
        
        if (empty) break;
        
#ifdef _WIN32
        Sleep(1);
#else
        usleep(1000);
#endif
    }
}

/* ============================================================================
 * Metrics Implementation
 * ========================================================================== */

static curium_backend_metrics_t g_metrics = {0};
static CMMutex g_metrics_lock;
static int g_metrics_initialized = 0;

static void curium_metrics_init(void) {
    if (!g_metrics_initialized) {
        g_metrics_lock = curium_mutex_init();
        g_metrics_initialized = 1;
    }
}

void curium_backend_get_metrics(curium_backend_metrics_t* metrics) {
    if (!metrics) return;
    curium_metrics_init();
    
    curium_mutex_lock(g_metrics_lock);
    memcpy(metrics, &g_metrics, sizeof(curium_backend_metrics_t));
    curium_mutex_unlock(g_metrics_lock);
}

void curium_backend_reset_metrics(void) {
    curium_metrics_init();
    
    curium_mutex_lock(g_metrics_lock);
    memset(&g_metrics, 0, sizeof(g_metrics));
    curium_mutex_unlock(g_metrics_lock);
}

void curium_backend_record_request(double response_time_ms) {
    curium_metrics_init();
    
    curium_mutex_lock(g_metrics_lock);
    g_metrics.requests_total++;
    
    /* Update average */
    double old_avg = g_metrics.avg_response_time_ms;
    g_metrics.avg_response_time_ms = old_avg + (response_time_ms - old_avg) / g_metrics.requests_total;
    
    /* Track P99 (simplified - real impl would use histogram) */
    if (response_time_ms > g_metrics.p99_response_time_ms * 0.99) {
        g_metrics.p99_response_time_ms = response_time_ms;
    }
    
    curium_mutex_unlock(g_metrics_lock);
}

curium_string_t* curium_backend_metrics_json(void) {
    curium_backend_metrics_t m;
    curium_backend_get_metrics(&m);
    
    return curium_string_format(
        "{"
        "\"requests_total\": %llu,"
        "\"requests_active\": %llu,"
        "\"bytes_in\": %llu,"
        "\"bytes_out\": %llu,"
        "\"avg_response_time_ms\": %.2f,"
        "\"p99_response_time_ms\": %.2f,"
        "\"gc_collections\": %zu,"
        "\"memory_used_bytes\": %zu,"
        "\"memory_peak_bytes\": %zu"
        "}",
        m.requests_total,
        m.requests_active,
        m.bytes_in,
        m.bytes_out,
        m.avg_response_time_ms,
        m.p99_response_time_ms,
        m.gc_collections,
        m.memory_used_bytes,
        m.memory_peak_bytes
    );
}

/* ============================================================================
 * SIMD Detection
 * ========================================================================== */

int curium_cpu_has_sse2(void) {
#if defined(__x86_64__) || defined(_M_X64)
    /* x86-64 always has SSE2 */
    return 1;
#else
    /* Would need CPUID check for x86 */
    return 0;
#endif
}

int curium_cpu_has_avx2(void) {
    /* Would need CPUID check */
    return 0;
}

int curium_cpu_has_neon(void) {
#if defined(__ARM_NEON) || defined(__aarch64__)
    return 1;
#else
    return 0;
#endif
}

/* ============================================================================
 * Generational GC (stubs - would integrate with existing GC)
 * ========================================================================== */

void curium_gc_enable_generational(void) {
    /* Would configure GC for generational mode */
}

void curium_gc_collect_minor(void) {
    /* Collect young generation only */
    curium_gc_collect();
}

void curium_gc_collect_major(void) {
    /* Full GC */
    curium_gc_collect();
}

void curium_gc_set_max_pause_ms(int ms) {
    /* Configure max pause time target */
    (void)ms;
}
