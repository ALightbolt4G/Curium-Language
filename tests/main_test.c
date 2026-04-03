#include "curium/core.h"
#include "curium/memory.h"
#include "curium/error.h"
#include "curium/string.h"
#include "curium/array.h"
#include "curium/map.h"
#include "curium/json.h"
#include "curium/http.h"
#include "curium/cmd.h"
#include <stdio.h>
#include <assert.h>

void test_arena_module() {
    printf("Testing Arena Module...\n");
    CURIUM_WITH_ARENA("test_arena", 1024) {
        int* arr = (int*)curium_alloc(10 * sizeof(int), "arena_test_array");
        for (int i = 0; i < 10; ++i) {
            arr[i] = i * 2;
        }
        assert(arr[5] == 10);
    }
    printf("Arena Module OK\n");
}

void test_string_module() {
    printf("Testing String Module...\n");
    curium_string_t* str = curium_string_new("Hello");
    assert(curium_string_length(str) == 5);
    curium_string_upper(str);
    assert(strcmp(str->data, "HELLO") == 0);
    curium_string_free(str);
    printf("String Module OK\n");
}

void test_array_module() {
    printf("Testing Array Module...\n");
    curium_array_t* arr = curium_array_new(sizeof(int), 2);
    int a = 10, b = 20, c = 30;
    curium_array_push(arr, &a);
    curium_array_push(arr, &b);
    curium_array_push(arr, &c); // Triggers expansion
    assert(curium_array_length(arr) == 3);
    
    int* val = (int*)curium_array_get(arr, 1);
    (void)val;
    assert(*val == 20);
    
    curium_array_pop(arr);
    assert(curium_array_length(arr) == 2);
    curium_array_free(arr);
    printf("Array Module OK\n");
}

void test_cmd_module(void) {
    printf("Testing CMD SDK Module...\n");

    /* Test 1: run python -c "print('hello from cmd sdk')" and capture output */
    curium_cmd_t* cmd = curium_cmd_new("python");
    curium_cmd_arg(cmd, "-c");
    curium_cmd_arg(cmd, "print('hello from cmd sdk')");

    curium_cmd_result_t* r = curium_cmd_run(cmd);
    curium_cmd_free(cmd);

    assert(r != NULL);
    printf("[test] exit_code=%d stdout=[%s]\n", r->exit_code, r->stdout_output->data);
    assert(r->exit_code == 0);
    assert(strstr(r->stdout_output->data, "hello from cmd sdk") != NULL);
    curium_cmd_result_free(r);

    /* Test 2: injection payload — a shell dangerous string is passed as a
       literal argv element; curium_cmd does NOT invoke a shell, so it is safe.
       We just verify it runs and returns without crashing/injecting. */
    curium_cmd_t* safe_cmd = curium_cmd_new("python");
    curium_cmd_arg(safe_cmd, "-c");
    /* This string would break a naive system() call but is safe via curium_cmd */
    curium_cmd_arg(safe_cmd, "import sys; sys.exit(0)  # & del /f /q C:\\windows");
    curium_cmd_result_t* r2 = curium_cmd_run(safe_cmd);
    curium_cmd_free(safe_cmd);
    assert(r2 != NULL);
    assert(r2->exit_code == 0);  /* python exits 0 — the # comment is ignored */
    curium_cmd_result_free(r2);

    printf("CMD SDK Module OK\n");
}

static void dummy_handler(CuriumHttpRequest* req, CuriumHttpResponse* res) {
    (void)req;
    curium_res_send(res, "OK");
}

void test_http_module(void) {
    printf("Testing HTTP Module (PUT/DELETE)...\n");

    /* Register some routes using the new macros */
    curium_app_put("/api/update", dummy_handler);
    curium_app_delete("/api/remove", dummy_handler);

    /* We don't have an easy way to trigger handle_server_client manually
       without a socket, so we verify they are registered by checking the
       internal state if we could, but for now just compile/registration check. */
    printf("HTTP Module (PUT/DELETE) OK\n");
}

static int destructor_called = 0;
static void my_destructor(void* ptr) {
    (void)ptr;
    destructor_called = 1;
}

void test_memory_safety(void) {
    printf("Testing Memory Safety (Safe Pointers & Destructors)...\n");

    /* 1. Safe Pointer UAF Prevention */
    void* p1 = curium_alloc(100, "safe_ptr_test");
    curium_ptr_t h1 = curium_ptr(p1);
    (void)h1;
    assert(curium_ptr_get(h1) == p1);

    curium_free(p1);
    assert(curium_ptr_get(h1) == NULL); /* Should be NULL after free */

    /* 2. Destructor Integration */
    destructor_called = 0;
    void* p2 = curium_alloc(50, "destructor_test");
    curium_set_destructor(p2, my_destructor);
    curium_free(p2);
    assert(destructor_called == 1);

    /* 3. Address Reuse Safety */
    /* We allocate many times to hopefully reuse an address or at least verify stability */
    void* p3 = curium_alloc(1024, "reuse_test_1");
    curium_ptr_t h3 = curium_ptr(p3);
    (void)h3;
    curium_free(p3);
    
    for (int i = 0; i < 100; i++) {
        void* tmp = curium_alloc(1024, "reuse_test_tmp");
        curium_free(tmp);
    }
    
    /* Even if memory is reused, the old handle h3 must remain NULL because the generation changed */
    assert(curium_ptr_get(h3) == NULL);

    printf("Memory Safety OK\n");
}

int main(void) {
    curium_gc_init();
    curium_init_error_detector();
    
    printf("\n=========================================\n");
    printf("CM Library v%s Modular Test Suite\n", CURIUM_VERSION);
    printf("=========================================\n\n");

    test_arena_module();
    test_string_module();
    test_array_module();
    test_cmd_module();
    test_http_module();
    test_memory_safety();
    
    // Test GC explicitly natively
    curium_gc_collect();
    curium_gc_shutdown();

    printf("\nAll Tests Passed Successfully!\n");
    return 0;
}
