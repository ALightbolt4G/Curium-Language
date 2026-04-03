#include "curium/core.h"
#include "curium/error.h"
#include "curium/memory.h"
#include "curium/string.h"
#include "curium/array.h"
#include "curium/map.h"
#include "curium/json.h"
#include "curium/http.h"
#include "curium/file.h"
#include "curium/thread.h"

static void curium_builtin_print(const char* s) { if (s) printf("%s", s); }
static void curium_builtin_print_str(curium_string_t* s) { if (s && s->data) printf("%s", s->data); }

static void curium_serve_static(CuriumHttpRequest* req, CuriumHttpResponse* res, const char* path, const char* mime) {
    (void)req; curium_res_send_file(res, path, mime);
}
static void curium_serve_index(CuriumHttpRequest* req, CuriumHttpResponse* res) {
    curium_serve_static(req, res, "public_html/index.html", "text/html");
}
static void curium_serve_js(CuriumHttpRequest* req, CuriumHttpResponse* res) {
    curium_serve_static(req, res, "public_html/script.js", "application/javascript");
}
static void curium_serve_css(CuriumHttpRequest* req, CuriumHttpResponse* res) {
    curium_serve_static(req, res, "public_html/style.css", "text/css");
}

int main(void) {
    curium_gc_init();
    curium_init_error_detector();

    curium_string_t* name = curium_input(NULL);
    curium_string_upper(name);
    curium_builtin_print("Hello, ");
    curium_builtin_print_str(name);
    curium_builtin_print("!\n");
    curium_gc_stats();
    curium_gc_shutdown();
    return 0;
}
