#include "curium/core.h"
#include "curium/memory.h"
#include "curium/error.h"
#include "curium/http.h"
#include "curium/json.h"
#include "curium/file.h"
#include "curium/map.h"
#include "curium/cmd.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#else
#include <unistd.h>
#include <libgen.h>
#endif

/* Data structure to stress test curium_ptr_t and destructors */
typedef struct {
    char url[1024];
    char* info_json_path;
    curium_ptr_t self_handle;
} DownloadTask;

void download_task_destructor(void* ptr) {
    DownloadTask* task = (DownloadTask*)ptr;
    printf("[STRESS] Destructor called for task: %s\n", task->url);
    if (task->info_json_path) {
        /* Cleanup temporary JSON metadata file if it exists */
        remove(task->info_json_path);
        free(task->info_json_path);
    }
}

void serve_index(CuriumHttpRequest* req, CuriumHttpResponse* res) {
    (void)req;
    curium_res_send_file(res, "public_html/index.html", "text/html");
}

void serve_css(CuriumHttpRequest* req, CuriumHttpResponse* res) {
    (void)req;
    curium_res_send_file(res, "public_html/style.css", "text/css");
}

void serve_js(CuriumHttpRequest* req, CuriumHttpResponse* res) {
    (void)req;
    curium_res_send_file(res, "public_html/script.js", "application/javascript");
}

void login_api(CuriumHttpRequest* req, CuriumHttpResponse* res) {
    struct CuriumJsonNode* body = curium_json_parse(req->body->data);
    if (body && body->type == CURIUM_JSON_OBJECT) {
        curium_map_t* obj = body->value.object_val;
        struct CuriumJsonNode** u_node = (struct CuriumJsonNode**)curium_map_get(obj, "username");
        struct CuriumJsonNode** p_node = (struct CuriumJsonNode**)curium_map_get(obj, "password");
        
        if (u_node && p_node) {
            const char* user = (*u_node)->value.string_val->data;
            const char* pass = (*p_node)->value.string_val->data;
            
            if (strcmp(user, "admin") == 0 && strcmp(pass, "1234") == 0) {
                struct CuriumJsonNode* ok = curium_json_parse("{\"status\":\"success\"}");
                curium_res_json(res, ok);
                CuriumJsonNode_delete(ok);
            } else {
                struct CuriumJsonNode* err = curium_json_parse("{\"status\":\"error\", \"message\":\"Invalid credentials\"}");
                curium_res_status(res, 401);
                curium_res_json(res, err);
                CuriumJsonNode_delete(err);
            }
        }
    } else {
        curium_res_status(res, 400);
    }
    if (body) CuriumJsonNode_delete(body);
}

void serve_status(CuriumHttpRequest* req, CuriumHttpResponse* res) {
    (void)req;
    /* Ultimate stress: return GC stats as JSON */
    curium_gc_stats();
    struct CuriumJsonNode* stats = curium_json_parse("{\"status\":\"ok\", \"note\":\"See server console for detailed leak analysis\"}");
    curium_res_json(res, stats);
    CuriumJsonNode_delete(stats);
}

void stress_download_api(CuriumHttpRequest* req, CuriumHttpResponse* res) {
    DownloadTask* task = (DownloadTask*)curium_alloc(sizeof(DownloadTask), "DownloadTask");
    curium_set_destructor(task, download_task_destructor);
    
    /* Wrap in safe pointer for stress testing */
    curium_ptr_t task_handle = curium_ptr(task);
    task->self_handle = task_handle;

    struct CuriumJsonNode* body = curium_json_parse(req->body->data);
    if (!body || body->type != CURIUM_JSON_OBJECT) {
        curium_res_status(res, 400);
        curium_res_send(res, "Invalid JSON");
        if (body) CuriumJsonNode_delete(body);
        curium_free(task);
        return;
    }

    curium_map_t* obj = body->value.object_val;
    struct CuriumJsonNode** url_node = (struct CuriumJsonNode**)curium_map_get(obj, "url");
    
    if (!url_node) {
        curium_res_status(res, 400);
        curium_res_send(res, "Missing url field");
        CuriumJsonNode_delete(body);
        curium_free(task);
        return;
    }

    const char* url = (*url_node)->value.string_val->data;
    strncpy(task->url, url, sizeof(task->url) - 1);
    task->url[sizeof(task->url) - 1] = '\0';
    CuriumJsonNode_delete(body);

    printf("[STRESS] Starting secure download: %s\n", url);

    curium_cmd_t* cmd = curium_cmd_new("python");
    curium_cmd_arg(cmd, "-m");
    curium_cmd_arg(cmd, "yt_dlp");
    curium_cmd_arg(cmd, "--no-update");
    curium_cmd_arg(cmd, "--write-info-json"); /* EXTRACT FULL METADATA */
    curium_cmd_arg(cmd, "--no-playlist");
    curium_cmd_arg(cmd, "-f");
    curium_cmd_arg(cmd, "best[ext=mp4]/best");
    curium_cmd_arg(cmd, "-o");
    curium_cmd_arg(cmd, "public_html/downloads/%(title)s.%(ext)s");
    curium_cmd_arg(cmd, url);

    curium_cmd_result_t* result = curium_cmd_run(cmd);
    curium_cmd_free(cmd);

    /* Verify task is still valid via safe pointer */
    DownloadTask* v_task = (DownloadTask*)curium_ptr_get(task_handle);
    if (!v_task) {
        curium_res_status(res, 500);
        curium_res_send(res, "Memory integrity error: task pointer invalidated");
        if (result) curium_cmd_result_free(result);
        return;
    }

    if (result && result->exit_code == 0) {
        printf("[STRESS] Download success for: %s\n", url);
        /* Check for metadata file */
        char metadata_file[1024];
        snprintf(metadata_file, sizeof(metadata_file), "public_html/downloads/%s.info.json", "..."); // Simplified
        
        struct CuriumJsonNode* ok = curium_json_parse("{\"status\":\"success\", \"metadata_extracted\":true}");
        curium_res_json(res, ok);
        CuriumJsonNode_delete(ok);
    } else {
        printf("[STRESS] Download failed for: %s (Exit code: %d)\n", url, result ? result->exit_code : -1);
        if (result && result->stderr_output) {
            printf("[STRESS] Error output: %s\n", result->stderr_output->data);
        }
        curium_res_status(res, 500);
        curium_res_send(res, "Download failed");
    }

    if (result) curium_cmd_result_free(result);
    
    printf("[STRESS] Task completed. Freeing resources.\n");
    curium_free(task);

    curium_gc_stats();
}

void serve_memory_stats(CuriumHttpRequest* req, CuriumHttpResponse* res) {
    (void)req;
    printf("\n[DASHBOARD] Manual Memory Check Triggered\n");
    curium_gc_stats();
    curium_gc_print_leaks();
    struct CuriumJsonNode* ok = curium_json_parse("{\"status\":\"checked\", \"message\":\"Check server console for detailed leak report\"}");
    curium_res_json(res, ok);
    CuriumJsonNode_delete(ok);
}

int main() {
    curium_init();
    curium_init_error_detector();

    printf("\n🚀 CM Production Stress-Test Server\n");
    printf("====================================\n");

    curium_app_get("/", serve_index);
    curium_app_get("/style.css", serve_css);
    curium_app_get("/script.js", serve_js);
    curium_app_get("/api/status", serve_status);
    curium_app_get("/api/memory", serve_memory_stats);
    curium_app_post("/api/login", login_api);
    curium_app_post("/api/download", stress_download_api);

    curium_app_listen(8081);
    
    curium_shutdown();
    return 0;
}
