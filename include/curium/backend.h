#ifndef CURIUM_BACKEND_H
#define CURIUM_BACKEND_H

#include "curium/core.h"
#include "curium/memory.h"
#include "curium/map.h"
#include "curium/string.h"
#include "curium/thread.h"
#include "curium/http.h"

/* ============================================================================
 * CM Backend Performance Optimizations
 * High-performance server-side optimizations for CM language
 * ========================================================================== */

#define CURIUM_BACKEND_VERSION "1.0.0"

/* ============================================================================
 * Connection Pooling
 * ========================================================================== */

typedef struct {
    void** connections;
    size_t size;
    size_t capacity;
    size_t max_size;
    CMMutex lock;
    int (*create_conn)(void** conn);
    void (*destroy_conn)(void* conn);
    int (*validate_conn)(void* conn);
} curium_connection_pool_t;

curium_connection_pool_t* curium_pool_create(size_t initial, size_t max,
                                      int (*create)(void**),
                                      void (*destroy)(void*),
                                      int (*validate)(void*));
void curium_pool_destroy(curium_connection_pool_t* pool);
void* curium_pool_acquire(curium_connection_pool_t* pool);
int curium_pool_release(curium_connection_pool_t* pool, void* conn);

/* ============================================================================
 * Zero-Copy Buffer Management
 * ========================================================================== */

typedef struct {
    char* data;
    size_t capacity;
    size_t read_pos;
    size_t write_pos;
    int ref_count;
} curium_zerocopy_buffer_t;

curium_zerocopy_buffer_t* curium_buffer_create(size_t capacity);
curium_zerocopy_buffer_t* curium_buffer_ref(curium_zerocopy_buffer_t* buf);
void curium_buffer_unref(curium_zerocopy_buffer_t* buf);
size_t curium_buffer_write(curium_zerocopy_buffer_t* buf, const void* data, size_t len);
size_t curium_buffer_read(curium_zerocopy_buffer_t* buf, void* dest, size_t len);
void curium_buffer_reset(curium_zerocopy_buffer_t* buf);

/* ============================================================================
 * String Interning (for reduced memory)
 * ========================================================================== */

typedef struct {
    curium_map_t* table;  /* hash -> string */
    CMMutex lock;
} curium_string_pool_t;

curium_string_pool_t* curium_string_pool_create(void);
void curium_string_pool_destroy(curium_string_pool_t* pool);
const char* curium_string_pool_intern(curium_string_pool_t* pool, const char* str);
void curium_string_pool_collect_garbage(curium_string_pool_t* pool);

/* ============================================================================
 * Async I/O (io_uring style on Linux, IOCP on Windows)
 * ========================================================================== */

typedef enum {
    CURIUM_AIO_READ,
    CURIUM_AIO_WRITE,
    CURIUM_AIO_ACCEPT,
    CURIUM_AIO_CONNECT,
    CURIUM_AIO_CLOSE
} curium_aio_op_t;

typedef struct {
    curium_aio_op_t op;
    int fd;
    void* buffer;
    size_t len;
    void* user_data;
    int result;
    size_t bytes_transferred;
} curium_aio_request_t;

typedef struct curium_aio_queue curium_aio_queue_t;

curium_aio_queue_t* curium_aio_queue_create(size_t queue_depth);
void curium_aio_queue_destroy(curium_aio_queue_t* q);
int curium_aio_submit(curium_aio_queue_t* q, curium_aio_request_t* reqs, size_t count);
int curium_aio_wait(curium_aio_queue_t* q, curium_aio_request_t** completed, size_t* count, int timeout_ms);

/* ============================================================================
 * SIMD-Accelerated JSON Parsing
 * ========================================================================== */

struct CuriumJsonNode* curium_json_parse_fast(const char* json, size_t len);
curium_string_t* curium_json_stringify_fast(struct CuriumJsonNode* node);

/* Check for SIMD support */
int curium_cpu_has_sse2(void);
int curium_cpu_has_avx2(void);
int curium_cpu_has_neon(void);  /* ARM */

/* ============================================================================
 * Work-Stealing Thread Pool
 * ========================================================================== */

typedef struct curium_worker_pool curium_worker_pool_t;

typedef void (*curium_worker_task_t)(void* arg);

curium_worker_pool_t* curium_worker_pool_create(size_t num_workers);
void curium_worker_pool_destroy(curium_worker_pool_t* pool);
int curium_worker_pool_submit(curium_worker_pool_t* pool, curium_worker_task_t task, void* arg);
void curium_worker_pool_wait_all(curium_worker_pool_t* pool);

/* ============================================================================
 * Generational Garbage Collector (for low latency)
 * ========================================================================== */

void curium_gc_enable_generational(void);
void curium_gc_collect_minor(void);  /* Young generation only */
void curium_gc_collect_major(void);  /* Full GC */
void curium_gc_set_max_pause_ms(int ms);

/* ============================================================================
 * HTTP Server Optimizations
 * ========================================================================== */

typedef struct {
    int use_keepalive;
    int keepalive_timeout_sec;
    int max_requests_per_conn;
    int use_compression;  /* gzip/deflate */
    int compression_threshold;  /* min bytes to compress */
} curium_http_server_config_t;

typedef struct curium_http_server curium_http_server_t;

curium_http_server_t* curium_http_server_create(curium_http_server_config_t* config);
void curium_http_server_destroy(curium_http_server_t* server);
int curium_http_server_listen(curium_http_server_t* server, int port);
void curium_http_server_run(curium_http_server_t* server);  /* Event loop */

/* Route with caching */
void curium_http_route_cached(curium_http_server_t* server, const char* path,
                          int cache_seconds,
                          void (*handler)(CuriumHttpRequest*, CuriumHttpResponse*));

/* ============================================================================
 * Database Connection (with pooling)
 * ========================================================================== */

typedef struct curium_db_conn curium_db_conn_t;
typedef struct curium_db_result curium_db_result_t;

curium_db_conn_t* curium_db_connect(const char* connection_string);
void curium_db_close(curium_db_conn_t* conn);
curium_db_result_t* curium_db_query(curium_db_conn_t* conn, const char* sql);
curium_db_result_t* curium_db_query_async(curium_db_conn_t* conn, const char* sql);
curium_db_result_t* curium_db_query_params(curium_db_conn_t* conn, const char* sql, 
                                   const char** params, size_t param_count);

int curium_db_result_fetch_row(curium_db_result_t* result, curium_map_t** row);
void curium_db_result_free(curium_db_result_t* result);

/* ============================================================================
 * Caching Layer
 * ========================================================================== */

typedef struct curium_cache curium_cache_t;

curium_cache_t* curium_cache_create(size_t max_entries, size_t max_memory_bytes);
void curium_cache_destroy(curium_cache_t* cache);
void curium_cache_set(curium_cache_t* cache, const char* key, const void* data, size_t len, int ttl_seconds);
void* curium_cache_get(curium_cache_t* cache, const char* key, size_t* len);
void curium_cache_invalidate(curium_cache_t* cache, const char* pattern);  /* glob pattern */

/* ============================================================================
 * Metrics & Monitoring
 * ========================================================================== */

typedef struct {
    uint64_t requests_total;
    uint64_t requests_active;
    uint64_t bytes_in;
    uint64_t bytes_out;
    double avg_response_time_ms;
    double p99_response_time_ms;
    size_t gc_collections;
    size_t memory_used_bytes;
    size_t memory_peak_bytes;
} curium_backend_metrics_t;

void curium_backend_get_metrics(curium_backend_metrics_t* metrics);
void curium_backend_reset_metrics(void);
curium_string_t* curium_backend_metrics_json(void);

/* ============================================================================
 * Rate Limiting
 * ========================================================================== */

typedef struct curium_rate_limiter curium_rate_limiter_t;

curium_rate_limiter_t* curium_rate_limiter_create(int requests_per_second, int burst_size);
void curium_rate_limiter_destroy(curium_rate_limiter_t* limiter);
int curium_rate_limiter_check(curium_rate_limiter_t* limiter, const char* key);
void curium_rate_limiter_reset(curium_rate_limiter_t* limiter, const char* key);

#endif /* CURIUM_BACKEND_H */
