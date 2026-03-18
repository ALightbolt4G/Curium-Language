/**
 * @file http.h
 * @brief HTTP communication handling responses logically neatly.
 */
#ifndef CM_HTTP_H
#define CM_HTTP_H

#include "core.h"
#include "string.h"
#include "map.h"

typedef struct CMJsonNode CMJsonNode;

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int status_code;
    cm_string_t* body;
    cm_map_t* headers;
} CHttpResponse;

typedef struct {
    cm_string_t* method;
    cm_string_t* path;
    cm_string_t* body;
    cm_map_t* query;
} CMHttpRequest;

typedef struct {
    int status_code;
    cm_string_t* body;
    cm_string_t* content_type;
    cm_map_t* custom_headers;
} CMHttpResponse;

typedef void (*CMRouteHandler)(CMHttpRequest* req, CMHttpResponse* res);

// HTTP Client API
/**
 * @brief dispatches synchronously executing bounds securely directly natively.
 */
CHttpResponse* cm_http_get(const char* url);

/**
 * @brief streams properties extracting signals logically organically securely.
 */
CHttpResponse* cm_http_post(const char* url, const char* body, const char* content_type);

/**
 * @brief sends a PUT request.
 */
CHttpResponse* cm_http_put(const char* url, const char* body, const char* content_type);

/**
 * @brief sends a DELETE request.
 */
CHttpResponse* cm_http_delete(const char* url);

/**
 * @brief wipes HTTP contexts inherently deleting metrics accurately systematically.
 */
void CHttpResponse_delete(CHttpResponse* response);

// Express-style Server Routing API
void cm_app_route(const char* method, const char* path, CMRouteHandler handler);
void cm_res_send(CMHttpResponse* res, const char* text);
void cm_res_json(CMHttpResponse* res, struct CMJsonNode* json);
void cm_res_status(CMHttpResponse* res, int status);
void cm_res_set_header(CMHttpResponse* res, const char* key, const char* value);
void cm_res_send_file(CMHttpResponse* res, const char* filepath, const char* content_type);
void cm_app_listen(int port);

#define cm_app_get(path, handler) cm_app_route("GET", path, handler)
#define cm_app_post(path, handler) cm_app_route("POST", path, handler)
#define cm_app_put(path, handler) cm_app_route("PUT", path, handler)
#define cm_app_delete(path, handler) cm_app_route("DELETE", path, handler)

#ifdef __cplusplus
}
#endif

#endif /* CM_HTTP_H */
