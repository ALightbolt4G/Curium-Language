#include "curium/http.h"
#include "curium/memory.h"
#include "curium/error.h"
#include "curium/array.h"
#include "curium/json.h"
#include "curium/file.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <sys/socket.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <unistd.h>
#endif

static void curium_http_init_winsock() {
#ifdef _WIN32
    static int winsock_initialized = 0;
    if (!winsock_initialized) {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            curium_error_set(CURIUM_ERROR_NETWORK, "Failed to initialize Winsock.");
        }
        winsock_initialized = 1;
    }
#endif
}

static int curium_http_connect(const char* hostname, int port) {
    curium_http_init_winsock();
    
    struct hostent* he;
    if ((he = gethostbyname(hostname)) == NULL) {
        curium_error_set(CURIUM_ERROR_NETWORK, "gethostbyname failed");
        return -1;
    }
    
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        curium_error_set(CURIUM_ERROR_NETWORK, "Failed to create socket");
        return -1;
    }
    
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr = *((struct in_addr*)he->h_addr);
    memset(&(server_addr.sin_zero), 0, 8);
    
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(struct sockaddr)) < 0) {
        curium_error_set(CURIUM_ERROR_NETWORK, "Connection failed");
#ifdef _WIN32
        closesocket(sock);
#else
        close(sock);
#endif
        return -1;
    }
    return sock;
}

static CHttpResponse* extract_http_response(const char* response_str) {
    CHttpResponse* res = (CHttpResponse*)curium_alloc(sizeof(CHttpResponse), "CHttpResponse");
    res->status_code = 0;
    res->headers = curium_map_new();
    res->body = curium_string_new("");
    
    const char* header_end = strstr(response_str, "\r\n\r\n");
    if (!header_end) {
        /* Fallback for some proxies or malformed HTTP/2 headers from curl */
        header_end = strstr(response_str, "\n\n");
        if (!header_end) return res;
        
        sscanf(response_str, "%*s %d", &res->status_code);
        curium_string_set(res->body, header_end + 2);
        return res;
    }
    
    sscanf(response_str, "%*s %d", &res->status_code);
    curium_string_set(res->body, header_end + 4);
    
    return res;
}

static CHttpResponse* curium_https_request_curl(const char* method, const char* url, const char* body, const char* content_type) {
    char cmd[8192];
    if (strcmp(method, "GET") == 0) {
        snprintf(cmd, sizeof(cmd), "curl -s -i \"%s\"", url);
    } else if (strcmp(method, "DELETE") == 0) {
        snprintf(cmd, sizeof(cmd), "curl -s -i -X DELETE \"%s\"", url);
    } else {
        snprintf(cmd, sizeof(cmd), "curl -s -i -X %s -H \"Content-Type: %s\" -d \"%s\" \"%s\"", 
                 method, content_type ? content_type : "application/x-www-form-urlencoded", body ? body : "", url);
    }

#ifdef _WIN32
    FILE* fp = _popen(cmd, "r");
#else
    FILE* fp = popen(cmd, "r");
#endif

    if (!fp) {
        return extract_http_response("");
    }

    size_t capacity = 4096;
    size_t length = 0;
    char* full_response = (char*)malloc(capacity);
    full_response[0] = '\0';

    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        size_t blen = strlen(buffer);
        if (length + blen + 1 > capacity) {
            capacity *= 2;
            full_response = (char*)realloc(full_response, capacity);
        }
        memcpy(full_response + length, buffer, blen);
        length += blen;
        full_response[length] = '\0';
    }

#ifdef _WIN32
    _pclose(fp);
#else
    pclose(fp);
#endif

    CHttpResponse* res = extract_http_response(full_response);
    free(full_response);
    return res;
}

CHttpResponse* curium_http_get(const char* url) {
    if (strncmp(url, "https://", 8) == 0) {
        return curium_https_request_curl("GET", url, NULL, NULL);
    }

    char hostname[256] = {0};
    char path[1024] = {0};
    
    if (strncmp(url, "http://", 7) == 0) {
        sscanf(url + 7, "%255[^/]/%1023[^\n]", hostname, path);
    } else {
        sscanf(url, "%255[^/]/%1023[^\n]", hostname, path);
    }
    
    if (strlen(path) == 0) strcpy(path, "");
    
    int sock = curium_http_connect(hostname, 80);
    if (sock < 0) return extract_http_response(""); 
    
    char request[2048];
    snprintf(request, sizeof(request), "GET /%s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", path, hostname);
    
    send(sock, request, strlen(request), 0);
    
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

CHttpResponse* curium_http_post(const char* url, const char* body, const char* content_type) {
    if (strncmp(url, "https://", 8) == 0) {
        return curium_https_request_curl("POST", url, body, content_type);
    }

    char hostname[256] = {0};
    char path[1024] = {0};
    
    if (strncmp(url, "http://", 7) == 0) {
        sscanf(url + 7, "%255[^/]/%1023[^\n]", hostname, path);
    } else {
        sscanf(url, "%255[^/]/%1023[^\n]", hostname, path);
    }
    if (strlen(path) == 0) strcpy(path, "");
    
    int sock = curium_http_connect(hostname, 80);
    if (sock < 0) return extract_http_response("");
    
    char request[4096];
    snprintf(request, sizeof(request), "POST /%s HTTP/1.1\r\nHost: %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n%s", 
             path, hostname, content_type ? content_type : "application/x-www-form-urlencoded", strlen(body), body);
             
    send(sock, request, strlen(request), 0);
    
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

CHttpResponse* curium_http_put(const char* url, const char* body, const char* content_type) {
    if (strncmp(url, "https://", 8) == 0) {
        return curium_https_request_curl("PUT", url, body, content_type);
    }

    char hostname[256] = {0};
    char path[1024] = {0};
    
    if (strncmp(url, "http://", 7) == 0) {
        sscanf(url + 7, "%255[^/]/%1023[^\n]", hostname, path);
    } else {
        sscanf(url, "%255[^/]/%1023[^\n]", hostname, path);
    }
    if (strlen(path) == 0) strcpy(path, "");
    
    int sock = curium_http_connect(hostname, 80);
    if (sock < 0) return extract_http_response("");
    
    char request[4096];
    snprintf(request, sizeof(request), "PUT /%s HTTP/1.1\r\nHost: %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n%s", 
             path, hostname, content_type ? content_type : "application/x-www-form-urlencoded", strlen(body), body);
             
    send(sock, request, strlen(request), 0);
    
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

CHttpResponse* curium_http_delete(const char* url) {
    if (strncmp(url, "https://", 8) == 0) {
        return curium_https_request_curl("DELETE", url, NULL, NULL);
    }

    char hostname[256] = {0};
    char path[1024] = {0};
    
    if (strncmp(url, "http://", 7) == 0) {
        sscanf(url + 7, "%255[^/]/%1023[^\n]", hostname, path);
    } else {
        sscanf(url, "%255[^/]/%1023[^\n]", hostname, path);
    }
    if (strlen(path) == 0) strcpy(path, "");
    
    int sock = curium_http_connect(hostname, 80);
    if (sock < 0) return extract_http_response("");
    
    char request[2048];
    snprintf(request, sizeof(request), "DELETE /%s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", path, hostname);
    
    send(sock, request, strlen(request), 0);
    
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

void CHttpResponse_delete(CHttpResponse* response) {
    if (!response) return;
    if (response->body) curium_string_free(response->body);
    if (response->headers) curium_map_free(response->headers);
    curium_free(response);
}

// ============================================
// Express-like Server Framework
// ============================================

typedef struct {
    curium_string_t* method;
    curium_string_t* path;
    CMRouteHandler handler;
} CMRouteNode;

static void curium_route_destructor(void* elem) {
    CMRouteNode* r = (CMRouteNode*)elem;
    if (r->method) curium_string_free(r->method);
    if (r->path) curium_string_free(r->path);
}

static curium_array_t* server_routes = NULL;

void curium_app_route(const char* method, const char* path, CMRouteHandler handler) {
    if (!server_routes) {
        server_routes = curium_array_new(sizeof(CMRouteNode), 10);
        server_routes->element_destructor = curium_route_destructor;
    }
    CMRouteNode r;
    r.method = curium_string_new(method);
    r.path = curium_string_new(path);
    r.handler = handler;
    curium_array_push(server_routes, &r);
}

void curium_res_send(CuriumHttpResponse* res, const char* text) {
    if (!res) return;
    curium_string_set(res->body, text);
    curium_string_set(res->content_type, "text/plain; charset=utf-8");
}

void curium_res_json(CuriumHttpResponse* res, struct CuriumJsonNode* json) {
    if (!res) return;
    curium_string_t* str = curium_json_stringify(json);
    curium_string_set(res->body, str->data);
    curium_string_set(res->content_type, "application/json; charset=utf-8");
    curium_string_free(str);
}

void curium_res_status(CuriumHttpResponse* res, int status) {
    if (res) res->status_code = status;
}

void curium_res_set_header(CuriumHttpResponse* res, const char* key, const char* value) {
    if (!res || !res->custom_headers || !key || !value) return;
    curium_map_set(res->custom_headers, key, value, strlen(value) + 1);
}

void curium_res_send_file(CuriumHttpResponse* res, const char* filepath, const char* content_type) {
    if (!res || !filepath) return;
    curium_string_t* file_content = curium_file_read(filepath);
    if (!file_content) {
        curium_res_status(res, 404);
        curium_res_send(res, "404 File Not Found");
        return;
    }
    curium_string_set(res->body, file_content->data);
    
    char full_type[256];
    if (content_type && (strstr(content_type, "text/") || strstr(content_type, "json") || strstr(content_type, "javascript"))) {
        snprintf(full_type, sizeof(full_type), "%s; charset=utf-8", content_type);
        curium_string_set(res->content_type, full_type);
    } else {
        curium_string_set(res->content_type, content_type ? content_type : "application/octet-stream");
    }
    curium_string_free(file_content);
}

static CuriumHttpRequest* parse_request(const char* raw_data) {
    CuriumHttpRequest* req = (CuriumHttpRequest*)curium_alloc(sizeof(CuriumHttpRequest), "CuriumHttpRequest");
    req->method = curium_string_new("");
    req->path = curium_string_new("");
    req->body = curium_string_new("");
    req->query = curium_map_new();
    
    char method[16] = {0}, raw_path[1024] = {0};
    sscanf(raw_data, "%15s %1023s", method, raw_path);
    curium_string_set(req->method, method);
    
    char* qmark = strchr(raw_path, '?');
    if (qmark) {
        *qmark = '\0';
        curium_string_set(req->path, raw_path);
        char* qstring = qmark + 1;
        
        char* token = strtok(qstring, "&");
        while (token) {
            char* eq = strchr(token, '=');
            if (eq) {
                *eq = '\0';
                curium_map_set(req->query, token, eq + 1, strlen(eq + 1) + 1);
            } else {
                curium_map_set(req->query, token, "", 1);
            }
            token = strtok(NULL, "&");
        }
    } else {
        curium_string_set(req->path, raw_path);
    }
    
    const char* body_start = strstr(raw_data, "\r\n\r\n");
    if (body_start) {
        curium_string_set(req->body, body_start + 4);
    }
    return req;
}

static void free_request(CuriumHttpRequest* req) {
    if (!req) return;
    curium_string_free(req->method);
    curium_string_free(req->path);
    curium_string_free(req->body);
    curium_map_free(req->query);
    curium_free(req);
}

static void handle_server_client(int client_socket) {
    char* buffer = (char*)malloc(8192);
    int bytes_read = recv(client_socket, buffer, 8192 - 1, 0);
    if (bytes_read <= 0) {
        free(buffer);
#ifdef _WIN32
        closesocket(client_socket);
#else
        close(client_socket);
#endif
        return;
    }
    buffer[bytes_read] = '\0';

    /* In a real server we'd check Content-Length and loop recv until full,
       but for this stress test we'll at least capture up to 8KB which covers our JSON. */
    
    CuriumHttpRequest* req = parse_request(buffer);
    free(buffer);
    CuriumHttpResponse* res = (CuriumHttpResponse*)curium_alloc(sizeof(CuriumHttpResponse), "CuriumHttpResponse");
    res->status_code = 200;
    res->body = curium_string_new("");
    res->content_type = curium_string_new("text/plain");
    res->custom_headers = curium_map_new();
    
    printf("\n[CM Express] %s %s\n", req->method->data, req->path->data);

    int route_found = 0;
    if (server_routes) {
        for (size_t i = 0; i < curium_array_length(server_routes); i++) {
            CMRouteNode* r = (CMRouteNode*)curium_array_get(server_routes, i);
            if (strcmp(r->method->data, req->method->data) == 0 &&
                strcmp(r->path->data, req->path->data) == 0) {
                r->handler(req, res);
                route_found = 1;
                break;
            }
        }
    }
    
    if (!route_found) {
        curium_res_status(res, 404);
        curium_res_send(res, "404 Not Found");
    }
    
    const char* status_msg = "OK";
    if (res->status_code == 404) status_msg = "Not Found";
    if (res->status_code == 500) status_msg = "Internal Server Error";
    if (res->status_code == 400) status_msg = "Bad Request";

    curium_string_t* headers_str = curium_string_new("");
    if (res->custom_headers) {
        for (int i = 0; i < res->custom_headers->bucket_count; i++) {
            curium_map_entry_t* entry = res->custom_headers->buckets[i];
            while (entry) {
                // Only add headers if they are not Content-Type or Content-Length to avoid duplicates
                if (strcmp(entry->key, "Content-Type") != 0 && strcmp(entry->key, "Content-Length") != 0) {
                    curium_string_t* append_hdr = curium_string_format("%s: %s\r\n", entry->key, (const char*)entry->value);
                    curium_string_append(headers_str, append_hdr->data);
                    curium_string_free(append_hdr);
                } else if (strcmp(entry->key, "Content-Type") == 0) {
                    curium_string_set(res->content_type, (const char*)entry->value);
                }
                entry = entry->next;
            }
        }
    }

    curium_string_t* http_response = curium_string_format(
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Connection: close\r\n"
        "Content-Length: %zu\r\n"
        "%s"
        "\r\n"
        "%s",
        res->status_code,
        status_msg,
        res->content_type->data,
        curium_string_length(res->body),
        headers_str->data,
        res->body->data
    );
    
    printf("[SERVER] Sending %zu bytes to client\n", curium_string_length(http_response));
    send(client_socket, http_response->data, curium_string_length(http_response), 0);
    
    free_request(req);
    curium_string_free(res->body);
    curium_string_free(res->content_type);
    curium_map_free(res->custom_headers);
    curium_free(res);
    curium_string_free(headers_str);
    curium_string_free(http_response);
    curium_gc_collect();

#ifdef _WIN32
    closesocket(client_socket);
#else
    close(client_socket);
#endif
}

void curium_app_listen(int port) {
    curium_http_init_winsock();

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;

#ifndef _WIN32
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));
#else
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
#endif

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }
    
    listen(server_fd, 10);

    printf("===================================================\n");
    printf(" CM Express Server Listening on http://localhost:%d\n", port);
    printf("===================================================\n\n");

    while (1) {
        int addrlen = sizeof(address);
#ifdef _WIN32
        int client_socket = accept(server_fd, (struct sockaddr*)&address, &addrlen);
#else
        int client_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
#endif
        if (client_socket >= 0) {
            handle_server_client(client_socket);
        }
    }
}
