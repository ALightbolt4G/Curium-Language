/**
 * @file http.h
 * @brief HTTP communication handling responses logically neatly.
 */
#ifndef CURIUM_HTTP_H
#define CURIUM_HTTP_H

#include "core.h"
#include "string.h"
#include "map.h"

typedef struct CuriumJsonNode CuriumJsonNode;

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int status_code;
    curium_string_t* body;
    curium_map_t* headers;
} CHttpResponse;

typedef struct {
    curium_string_t* method;
    curium_string_t* path;
    curium_string_t* body;
    curium_map_t* query;
} CuriumHttpRequest;

typedef struct {
    int status_code;
    curium_string_t* body;
    curium_string_t* content_type;
    curium_map_t* custom_headers;
} CuriumHttpResponse;

typedef void (*CMRouteHandler)(CuriumHttpRequest* req, CuriumHttpResponse* res);

// HTTP Client API
/**
 * @brief dispatches synchronously executing bounds securely directly natively.
 */
CHttpResponse* curium_http_get(const char* url);

/**
 * @brief streams properties extracting signals logically organically securely.
 */
CHttpResponse* curium_http_post(const char* url, const char* body, const char* content_type);

/**
 * @brief sends a PUT request.
 */
CHttpResponse* curium_http_put(const char* url, const char* body, const char* content_type);

/**
 * @brief sends a DELETE request.
 */
CHttpResponse* curium_http_delete(const char* url);

/**
 * @brief wipes HTTP contexts inherently deleting metrics accurately systematically.
 */
void CHttpResponse_delete(CHttpResponse* response);

// Express-style Server Routing API
void curium_app_route(const char* method, const char* path, CMRouteHandler handler);
void curium_res_send(CuriumHttpResponse* res, const char* text);
void curium_res_json(CuriumHttpResponse* res, struct CuriumJsonNode* json);
void curium_res_status(CuriumHttpResponse* res, int status);
void curium_res_set_header(CuriumHttpResponse* res, const char* key, const char* value);
void curium_res_send_file(CuriumHttpResponse* res, const char* filepath, const char* content_type);
void curium_app_listen(int port);

#define curium_app_get(path, handler) curium_app_route("GET", path, handler)
#define curium_app_post(path, handler) curium_app_route("POST", path, handler)
#define curium_app_put(path, handler) curium_app_route("PUT", path, handler)
#define curium_app_delete(path, handler) curium_app_route("DELETE", path, handler)

#ifdef __cplusplus
}
#endif

#endif /* CURIUM_HTTP_H */
