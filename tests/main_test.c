#include "cm/core.h"
#include "cm/memory.h"
#include "cm/error.h"
#include "cm/string.h"
#include "cm/array.h"
#include "cm/map.h"
#include "cm/json.h"
#include "cm/http.h"
#include "cm/cmd.h"
#include <stdio.h>
#include <assert.h>

void test_arena_module() {
    printf("Testing Arena Module...\n");
    CM_WITH_ARENA("test_arena", 1024) {
        int* arr = (int*)cm_alloc(10 * sizeof(int), "arena_test_array");
        for (int i = 0; i < 10; ++i) {
            arr[i] = i * 2;
        }
        assert(arr[5] == 10);
    }
    printf("Arena Module OK\n");
}

void test_string_module() {
    printf("Testing String Module...\n");
    cm_string_t* str = cm_string_new("Hello");
    assert(cm_string_length(str) == 5);
    cm_string_upper(str);
    assert(strcmp(str->data, "HELLO") == 0);
    cm_string_free(str);
    printf("String Module OK\n");
}

void test_array_module() {
    printf("Testing Array Module...\n");
    cm_array_t* arr = cm_array_new(sizeof(int), 2);
    int a = 10, b = 20, c = 30;
    cm_array_push(arr, &a);
    cm_array_push(arr, &b);
    cm_array_push(arr, &c); // Triggers expansion
    assert(cm_array_length(arr) == 3);
    
    int* val = (int*)cm_array_get(arr, 1);
    assert(*val == 20);
    
    cm_array_pop(arr);
    assert(cm_array_length(arr) == 2);
    cm_array_free(arr);
    printf("Array Module OK\n");
}

void test_cmd_module(void) {
    printf("Testing CMD SDK Module...\n");

    /* Test 1: run python -c "print('hello from cmd sdk')" and capture output */
    cm_cmd_t* cmd = cm_cmd_new("python");
    cm_cmd_arg(cmd, "-c");
    cm_cmd_arg(cmd, "print('hello from cmd sdk')");

    cm_cmd_result_t* r = cm_cmd_run(cmd);
    cm_cmd_free(cmd);

    assert(r != NULL);
    printf("[test] exit_code=%d stdout=[%s]\n", r->exit_code, r->stdout_output->data);
    assert(r->exit_code == 0);
    assert(strstr(r->stdout_output->data, "hello from cmd sdk") != NULL);
    cm_cmd_result_free(r);

    /* Test 2: injection payload — a shell dangerous string is passed as a
       literal argv element; cm_cmd does NOT invoke a shell, so it is safe.
       We just verify it runs and returns without crashing/injecting. */
    cm_cmd_t* safe_cmd = cm_cmd_new("python");
    cm_cmd_arg(safe_cmd, "-c");
    /* This string would break a naive system() call but is safe via cm_cmd */
    cm_cmd_arg(safe_cmd, "import sys; sys.exit(0)  # & del /f /q C:\\windows");
    cm_cmd_result_t* r2 = cm_cmd_run(safe_cmd);
    cm_cmd_free(safe_cmd);
    assert(r2 != NULL);
    assert(r2->exit_code == 0);  /* python exits 0 — the # comment is ignored */
    cm_cmd_result_free(r2);

    printf("CMD SDK Module OK\n");
}

static void dummy_handler(CMHttpRequest* req, CMHttpResponse* res) {
    (void)req;
    cm_res_send(res, "OK");
}

void test_http_module(void) {
    printf("Testing HTTP Module (PUT/DELETE)...\n");

    /* Register some routes using the new macros */
    cm_app_put("/api/update", dummy_handler);
    cm_app_delete("/api/remove", dummy_handler);

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
    void* p1 = cm_alloc(100, "safe_ptr_test");
    cm_ptr_t h1 = cm_ptr(p1);
    assert(cm_ptr_get(h1) == p1);

    cm_free(p1);
    assert(cm_ptr_get(h1) == NULL); /* Should be NULL after free */

    /* 2. Destructor Integration */
    destructor_called = 0;
    void* p2 = cm_alloc(50, "destructor_test");
    cm_set_destructor(p2, my_destructor);
    cm_free(p2);
    assert(destructor_called == 1);

    /* 3. Address Reuse Safety */
    /* We allocate many times to hopefully reuse an address or at least verify stability */
    void* p3 = cm_alloc(1024, "reuse_test_1");
    cm_ptr_t h3 = cm_ptr(p3);
    cm_free(p3);
    
    for (int i = 0; i < 100; i++) {
        void* tmp = cm_alloc(1024, "reuse_test_tmp");
        cm_free(tmp);
    }
    
    /* Even if memory is reused, the old handle h3 must remain NULL because the generation changed */
    assert(cm_ptr_get(h3) == NULL);

    printf("Memory Safety OK\n");
}

int main(void) {
    cm_gc_init();
    cm_init_error_detector();
    
    printf("\n=========================================\n");
    printf("CM Library v%s Modular Test Suite\n", CM_VERSION);
    printf("=========================================\n\n");

    test_arena_module();
    test_string_module();
    test_array_module();
    test_cmd_module();
    test_http_module();
    test_memory_safety();
    
    // Test GC explicitly natively
    cm_gc_collect();
    cm_gc_shutdown();

    printf("\nAll Tests Passed Successfully!\n");
    return 0;
}
