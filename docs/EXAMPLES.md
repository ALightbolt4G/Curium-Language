# CM Framework Examples

This document provides practical code snippets for common use cases when building with the CM Framework.

---

## 🚀 Basic JSON REST API

This example demonstrates how to parse a JSON request and return a JSON response.

```c
#include <cm/core.h>
#include <cm/http.h>
#include <cm/json.h>

void user_create(CMHttpRequest* req, CMHttpResponse* res) {
    // 1. Parse incoming JSON body
    struct CMJsonNode* root = cm_json_parse(req->body->data);
    
    // 2. Prepare a response node
    struct CMJsonNode* response = cm_json_parse("{\"ok\": true, \"received\": true}");
    
    // 3. Send response
    cm_res_json(res, response);
    
    // 4. Cleanup
    CMJsonNode_delete(root);
    CMJsonNode_delete(response);
}

int main() {
    cm_gc_init();
    
    cm_app_post("/api/users", user_create);
    
    cm_app_listen(8080);
    cm_gc_shutdown();
    return 0;
}
```

---

## 📄 Static File Server

Serving a complete website with HTML, CSS, and JS assets.

```c
#include <cm/core.h>
#include <cm/http.h>

void serve_index(CMHttpRequest* req, CMHttpResponse* res) {
    cm_res_send_file(res, "public_html/index.html", "text/html");
}

void serve_css(CMHttpRequest* req, CMHttpResponse* res) {
    cm_res_send_file(res, "public_html/style.css", "text/css");
}

int main() {
    cm_gc_init();
    
    cm_app_get("/", serve_index);
    cm_app_get("/style.css", serve_css);
    
    cm_app_listen(8080);
    cm_gc_shutdown();
    return 0;
}
```

---

## 🧵 Background Worker

Using the CM threading API to perform long-running tasks without blocking the main server.

```c
#include <cm/core.h>
#include <cm/thread.h>
#include <stdio.h>

void* background_task(void* arg) {
    const char* task_name = (const char*)arg;
    printf("Starting background task: %s\n", task_name);
    // Simulate work
    for(int i = 0; i < 5; i++) {
        printf("Task %s: %d%%\n", task_name, (i+1)*20);
    }
    return NULL;
}

int main() {
    cm_gc_init();
    
    CMThread t = cm_thread_create(background_task, "DataCrawler");
    cm_thread_join(t);
    
    cm_gc_shutdown();
    return 0;
}
```

---

## 💾 Persisting Configurations

Using hash maps to manage application settings and saving them to disk as JSON.

```c
#include <cm/core.h>
#include <cm/map.h>

void setup_config() {
    cm_map_t* config = cm_map_new();
    
    const char* val = "production";
    cm_map_set(config, "environment", (void*)val, 11);
    
    const char* port = "8080";
    cm_map_set(config, "port", (void*)port, 5);
    
    // Save to disk
    cm_map_save_to_json(config, "config.json");
    
    // Cleanup is handled by GC if you don't call cm_map_free explicitly
    // but explicit free is also fine:
    cm_map_free(config);
}
```
