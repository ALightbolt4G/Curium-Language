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
#include <direct.h>  /* _getcwd, _chdir */
#else
#include <unistd.h>  /* getcwd, chdir */
#include <libgen.h>  /* dirname */
#endif

void serve_html(CMHttpRequest* req, CMHttpResponse* res) {
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
    // Basic Authentication Logic natively in C
    struct CMJsonNode* parsed_body = cm_json_parse(req->body->data);
    
    if (parsed_body && parsed_body->type == CM_JSON_OBJECT) {
        cm_map_t* obj = parsed_body->value.object_val;
        struct CMJsonNode** user_node = (struct CMJsonNode**)cm_map_get(obj, "username");
        struct CMJsonNode** pass_node = (struct CMJsonNode**)cm_map_get(obj, "password");
        
        if (user_node && pass_node) {
            const char* user = (*user_node)->value.string_val->data;
            const char* pass = (*pass_node)->value.string_val->data;
            
            // Hardcoded check for demo
            if (strcmp(user, "admin") == 0 && strcmp(pass, "1234") == 0) {
                struct CMJsonNode* ok = cm_json_parse("{\"status\":\"success\"}");
                cm_res_json(res, ok);
                CMJsonNode_delete(ok);
            } else {
                struct CMJsonNode* err = cm_json_parse("{\"status\":\"error\", \"message\":\"Invalid Username or Password\"}");
                cm_res_status(res, 401);
                cm_res_json(res, err);
                CMJsonNode_delete(err);
            }
        }
    } else {
        struct CMJsonNode* err = cm_json_parse("{\"status\":\"error\", \"message\":\"Malformed Request\"}");
        cm_res_status(res, 400);
        cm_res_json(res, err);
        CMJsonNode_delete(err);
    }
    
    if (parsed_body) CMJsonNode_delete(parsed_body);
}

void download_api(CMHttpRequest* req, CMHttpResponse* res) {
    struct CMJsonNode* parsed_body = cm_json_parse(req->body->data);
    
    if (parsed_body && parsed_body->type == CM_JSON_OBJECT) {
        cm_map_t* obj = parsed_body->value.object_val;
        struct CMJsonNode** url_node = (struct CMJsonNode**)cm_map_get(obj, "url");
        
        if (url_node) {
            const char* url = (*url_node)->value.string_val->data;
            printf("\u2728 SERVER LOG: Downloading via CMD SDK: %s\n", url);

            /* Build command safely — no shell, no injection possible */
            cm_cmd_t* cmd = cm_cmd_new("python");
            cm_cmd_arg(cmd, "-m");
            cm_cmd_arg(cmd, "yt_dlp");
            cm_cmd_arg(cmd, "--no-update");

            /* Conditionally add cookies */
            FILE* cookie_check = fopen("cookies.txt", "r");
            if (cookie_check) {
                fclose(cookie_check);
                cm_cmd_arg(cmd, "--cookies");
                cm_cmd_arg(cmd, "cookies.txt");
            }

            cm_cmd_arg(cmd, "-f");
            cm_cmd_arg(cmd, "best[ext=mp4]/best");
            cm_cmd_arg(cmd, "-o");
            cm_cmd_arg(cmd, "public_html/downloads/%(title)s.%(ext)s");
            cm_cmd_arg(cmd, url);  /* user URL — safe regardless of content */

            cm_cmd_result_t* result = cm_cmd_run(cmd);
            cm_cmd_free(cmd);

            if (result && result->exit_code == 0) {
                printf("[CMD SDK] Download succeeded.\n");
                struct CMJsonNode* ok = cm_json_parse("{\"status\":\"success\"}");
                cm_res_json(res, ok);
                CMJsonNode_delete(ok);
            } else {
                printf("[CMD SDK] Download failed (exit=%d).\n",
                       result ? result->exit_code : -1);
                if (result) {
                    printf("[CMD SDK] stderr: %s\n", result->stderr_output->data);
                }
                struct CMJsonNode* err = cm_json_parse("{\"status\":\"error\", \"message\":\"Download failed\"}");
                cm_res_status(res, 500);
                cm_res_json(res, err);
                CMJsonNode_delete(err);
            }
            cm_cmd_result_free(result);
        } else {
            struct CMJsonNode* err = cm_json_parse("{\"status\":\"error\", \"message\":\"Missing URL field\"}");
            cm_res_status(res, 400);
            cm_res_json(res, err);
            CMJsonNode_delete(err);
        }
    } else {
        struct CMJsonNode* err = cm_json_parse("{\"status\":\"error\", \"message\":\"Malformed Request\"}");
        cm_res_status(res, 400);
        cm_res_json(res, err);
        CMJsonNode_delete(err);
    }
    
    if (parsed_body) CMJsonNode_delete(parsed_body);
}

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    /* Set working directory to the folder that contains this exe.
       This ensures relative paths like "public_html/index.html" will
       always resolve correctly, no matter where the user launched from. */
    char exe_path[MAX_PATH];
    if (GetModuleFileNameA(NULL, exe_path, MAX_PATH)) {
        /* Strip the exe filename to get just the directory */
        char* last_backslash = strrchr(exe_path, '\\');
        if (last_backslash) {
            *last_backslash = '\0';
            _chdir(exe_path);
            printf("[CM] Server root: %s\n", exe_path);
        }
    }
#else
    /* On Linux/macOS, use /proc/self/exe or argv[0] to find the exe dir */
    char exe_path[4096];
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len > 0) {
        exe_path[len] = '\0';
        char* dir = dirname(exe_path);
        chdir(dir);
        printf("[CM] Server root: %s\n", dir);
    }
#endif
    cm_gc_init();
    cm_init_error_detector();

    printf("\n🚀 CM Express v%s is running!\n", CM_VERSION);
    printf("🌐 Serving Login Page on http://localhost:8080\n\n");

    // Static Routes
    cm_app_get("/", serve_html);
    cm_app_get("/style.css", serve_css);
    cm_app_get("/script.js", serve_js);

    // Dynamic API Routes
    cm_app_post("/api/login", login_api);
    cm_app_post("/api/download", download_api);

    // Start Server
    cm_app_listen(8080);
    
    cm_gc_shutdown();
    return 0;
}