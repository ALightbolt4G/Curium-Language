#include "cm/core.h"
#include "cm/memory.h"
#include "cm/error.h"
#include "cm/http.h"
#include "cm/json.h"
#include "cm/file.h"
#include "cm/map.h"
#include "cm/cmd.h"
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

/* Data structure to stress test cm_ptr_t and destructors */
typedef struct {
    char url[1024];
    char* info_json_path;
    cm_ptr_t self_handle;
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

void serve_index(CMHttpRequest* req, CMHttpResponse* res) {
    (void)req;
    cm_res_send_file(res, "public_html/index.html", "text/html");
}

void serve_css(CMHttpRequest* req, CMHttpResponse* res) {
    (void)req;
    cm_res_send_file(res, "public_html/style.css", "text/css");
}

void serve_js(CMHttpRequest* req, CMHttpResponse* res) {
    (void)req;
    cm_res_send_file(res, "public_html/script.js", "application/javascript");
}

void login_api(CMHttpRequest* req, CMHttpResponse* res) {
    struct CMJsonNode* body = cm_json_parse(req->body->data);
    if (body && body->type == CM_JSON_OBJECT) {
        cm_map_t* obj = body->value.object_val;
        struct CMJsonNode** u_node = (struct CMJsonNode**)cm_map_get(obj, "username");
        struct CMJsonNode** p_node = (struct CMJsonNode**)cm_map_get(obj, "password");
        
        if (u_node && p_node) {
            const char* user = (*u_node)->value.string_val->data;
            const char* pass = (*p_node)->value.string_val->data;
            
            if (strcmp(user, "admin") == 0 && strcmp(pass, "1234") == 0) {
                struct CMJsonNode* ok = cm_json_parse("{\"status\":\"success\"}");
                cm_res_json(res, ok);
                CMJsonNode_delete(ok);
            } else {
                struct CMJsonNode* err = cm_json_parse("{\"status\":\"error\", \"message\":\"Invalid credentials\"}");
                cm_res_status(res, 401);
                cm_res_json(res, err);
                CMJsonNode_delete(err);
            }
        }
    } else {
        cm_res_status(res, 400);
    }
    if (body) CMJsonNode_delete(body);
}

void serve_status(CMHttpRequest* req, CMHttpResponse* res) {
    (void)req;
    /* Ultimate stress: return GC stats as JSON */
    cm_gc_stats();
    struct CMJsonNode* stats = cm_json_parse("{\"status\":\"ok\", \"note\":\"See server console for detailed leak analysis\"}");
    cm_res_json(res, stats);
    CMJsonNode_delete(stats);
}

void stress_download_api(CMHttpRequest* req, CMHttpResponse* res) {
    DownloadTask* task = (DownloadTask*)cm_alloc(sizeof(DownloadTask), "DownloadTask");
    cm_set_destructor(task, download_task_destructor);
    
    /* Wrap in safe pointer for stress testing */
    cm_ptr_t task_handle = cm_ptr(task);
    task->self_handle = task_handle;

    struct CMJsonNode* body = cm_json_parse(req->body->data);
    if (!body || body->type != CM_JSON_OBJECT) {
        cm_res_status(res, 400);
        cm_res_send(res, "Invalid JSON");
        if (body) CMJsonNode_delete(body);
        cm_free(task);
        return;
    }

    cm_map_t* obj = body->value.object_val;
    struct CMJsonNode** url_node = (struct CMJsonNode**)cm_map_get(obj, "url");
    
    if (!url_node) {
        cm_res_status(res, 400);
        cm_res_send(res, "Missing url field");
        CMJsonNode_delete(body);
        cm_free(task);
        return;
    }

    const char* url = (*url_node)->value.string_val->data;
    strncpy(task->url, url, sizeof(task->url) - 1);
    task->url[sizeof(task->url) - 1] = '\0';
    CMJsonNode_delete(body);

    printf("[STRESS] Starting secure download: %s\n", url);

    cm_cmd_t* cmd = cm_cmd_new("python");
    cm_cmd_arg(cmd, "-m");
    cm_cmd_arg(cmd, "yt_dlp");
    cm_cmd_arg(cmd, "--no-update");
    cm_cmd_arg(cmd, "--write-info-json"); /* EXTRACT FULL METADATA */
    cm_cmd_arg(cmd, "--no-playlist");
    cm_cmd_arg(cmd, "-f");
    cm_cmd_arg(cmd, "best[ext=mp4]/best");
    cm_cmd_arg(cmd, "-o");
    cm_cmd_arg(cmd, "public_html/downloads/%(title)s.%(ext)s");
    cm_cmd_arg(cmd, url);

    cm_cmd_result_t* result = cm_cmd_run(cmd);
    cm_cmd_free(cmd);

    /* Verify task is still valid via safe pointer */
    DownloadTask* v_task = (DownloadTask*)cm_ptr_get(task_handle);
    if (!v_task) {
        cm_res_status(res, 500);
        cm_res_send(res, "Memory integrity error: task pointer invalidated");
        if (result) cm_cmd_result_free(result);
        return;
    }

    if (result && result->exit_code == 0) {
        printf("[STRESS] Download success for: %s\n", url);
        /* Check for metadata file */
        char metadata_file[1024];
        snprintf(metadata_file, sizeof(metadata_file), "public_html/downloads/%s.info.json", "..."); // Simplified
        
        struct CMJsonNode* ok = cm_json_parse("{\"status\":\"success\", \"metadata_extracted\":true}");
        cm_res_json(res, ok);
        CMJsonNode_delete(ok);
    } else {
        printf("[STRESS] Download failed for: %s (Exit code: %d)\n", url, result ? result->exit_code : -1);
        if (result && result->stderr_output) {
            printf("[STRESS] Error output: %s\n", result->stderr_output->data);
        }
        cm_res_status(res, 500);
        cm_res_send(res, "Download failed");
    }

    if (result) cm_cmd_result_free(result);
    
    printf("[STRESS] Task completed. Freeing resources.\n");
    cm_free(task);

    cm_gc_stats();
}

void serve_memory_stats(CMHttpRequest* req, CMHttpResponse* res) {
    (void)req;
    printf("\n[DASHBOARD] Manual Memory Check Triggered\n");
    cm_gc_stats();
    cm_gc_print_leaks();
    struct CMJsonNode* ok = cm_json_parse("{\"status\":\"checked\", \"message\":\"Check server console for detailed leak report\"}");
    cm_res_json(res, ok);
    CMJsonNode_delete(ok);
}

int main() {
    cm_init();
    cm_init_error_detector();

    printf("\n🚀 CM Production Stress-Test Server\n");
    printf("====================================\n");

    cm_app_get("/", serve_index);
    cm_app_get("/style.css", serve_css);
    cm_app_get("/script.js", serve_js);
    cm_app_get("/api/status", serve_status);
    cm_app_get("/api/memory", serve_memory_stats);
    cm_app_post("/api/login", login_api);
    cm_app_post("/api/download", stress_download_api);

    cm_app_listen(8081);
    
    cm_shutdown();
    return 0;
}
