#include "curium/curium_lang.h"
#include "curium/memory.h"
#include "curium/error.h"
#include "curium/cmd.h"
#include "curium/file.h"
#include "curium_codegen.h"
#include "curium/compiler/ast_v2.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>

#ifndef _WIN32
#include <dirent.h>
#include <sys/stat.h>
#else
#include <direct.h>
#include <io.h>
#include <sys/stat.h>
#endif

/* ============================================================================
 * Parser
 * ==========================================================================*/



/* Legacy parser has been replaced by v2. */

typedef struct curium_module_list {
    curium_string_t** items;
    size_t        count;
    size_t        capacity;
} curium_module_list_t;

static void curium_module_list_init(curium_module_list_t* list) {
    memset(list, 0, sizeof(*list));
}

static void curium_module_list_append(curium_module_list_t* list, const char* path) {
    if (!list || !path) return;
    if (list->count == list->capacity) {
        size_t new_cap = list->capacity ? list->capacity * 2 : 8;
        curium_string_t** tmp = (curium_string_t**)curium_alloc(new_cap * sizeof(curium_string_t*), "curium_module_list");
        if (!tmp) return;
        if (list->items && list->count) {
            memcpy(tmp, list->items, list->count * sizeof(curium_string_t*));
        }
        list->items = tmp;
        list->capacity = new_cap;
    }
    list->items[list->count++] = curium_string_new(path);
}

static void curium_module_list_free(curium_module_list_t* list) {
    if (!list) return;
    for (size_t i = 0; i < list->count; ++i) {
        if (list->items[i]) curium_string_free(list->items[i]);
    }
    if (list->items) curium_free(list->items);
    list->items = NULL;
    list->count = list->capacity = 0;
}

/* ============================================================================
 * Transpiler / Codegen: CM AST -> Hardened C
 * ==========================================================================*/

/* Legacy blacklist checks removed */

/* ============================================================================
 * High-level compile pipeline
 * ==========================================================================*/

static char* curium_read_file_all(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    long sz = ftell(f);
    if (sz < 0) {
        fclose(f);
        return NULL;
    }
    rewind(f);
    char* buf = (char*)malloc((size_t)sz + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    size_t read = fread(buf, 1, (size_t)sz, f);
    if (read != (size_t)sz) {
        free(buf);
        fclose(f);
        return NULL;
    }
    buf[read] = '\0';
    fclose(f);
    return buf;
}

static int curium_write_text_file(const char* path, const char* text) {
    FILE* f = fopen(path, "wb");
    if (!f) return -1;
    if (text) {
        fwrite(text, 1, strlen(text), f);
    }
    fclose(f);
    return 0;
}

static void curium_try_remove_output(const char* output_exe) {
    if (!output_exe || !output_exe[0]) return;
    remove(output_exe);
#ifdef _WIN32
    /* MinGW produces .exe when -o is basename */
    char buf[512];
    snprintf(buf, sizeof(buf), "%s.exe", output_exe);
    remove(buf);
#endif
}

/* Removed legacy curium_collect_require_modules */

static int curium_invoke_system_compiler(const char* c_path, const char* output_exe) {
    if (!c_path || !output_exe) return -1;

    /* Enterprise default: compile generated C together with the CM runtime sources,
       so `cm main.cm app` works even without a pre-built libcm present. */
    const char* candidates[] = { "gcc", "clang", "cc", NULL };
    const char* cc = NULL;

    for (int i = 0; candidates[i]; ++i) {
        curium_cmd_t* probe = curium_cmd_new(candidates[i]);
        if (!probe) continue;
        curium_cmd_arg(probe, "--version");
        curium_cmd_result_t* r = curium_cmd_run(probe);
        curium_cmd_free(probe);
        if (r && r->exit_code == 0) {
            cc = candidates[i];
            curium_cmd_result_free(r);
            break;
        }
        if (r) curium_cmd_result_free(r);
    }

    if (!cc) {
        curium_error_set(CURIUM_ERROR_IO, "no C compiler found (expected gcc/clang/cc in PATH)");
        return -1;
    }

    curium_cmd_t* cmd = curium_cmd_new(cc);
    if (!cmd) return -1;

    curium_cmd_arg(cmd, c_path);
    curium_cmd_arg(cmd, "src/runtime/memory.c");
    curium_cmd_arg(cmd, "src/runtime/error.c");
    curium_cmd_arg(cmd, "src/runtime/error_detail.c");
    curium_cmd_arg(cmd, "src/runtime/string.c");
    curium_cmd_arg(cmd, "src/runtime/array.c");
    curium_cmd_arg(cmd, "src/runtime/map.c");
    curium_cmd_arg(cmd, "src/runtime/json.c");
    curium_cmd_arg(cmd, "src/runtime/http.c");
    curium_cmd_arg(cmd, "src/runtime/file.c");
    curium_cmd_arg(cmd, "src/runtime/thread.c");
    curium_cmd_arg(cmd, "src/runtime/result.c");
    curium_cmd_arg(cmd, "src/runtime/option.c");
    curium_cmd_arg(cmd, "src/runtime/core.c");

    curium_cmd_arg(cmd, "-Iinclude");
    curium_cmd_arg(cmd, "-o");
    curium_cmd_arg(cmd, output_exe);
    curium_cmd_arg(cmd, "-Wall");
    curium_cmd_arg(cmd, "-Wextra");

#ifdef _WIN32
    curium_cmd_arg(cmd, "-lws2_32");
#endif

    curium_cmd_result_t* res = curium_cmd_run(cmd);
    int exit_code = res ? res->exit_code : -1;
    if (exit_code != 0) {
        const char* msg = res && res->stderr_output
            ? res->stderr_output->data
            : "system compiler error";
        curium_error_set(CURIUM_ERROR_IO, msg);
    }
    if (res) curium_cmd_result_free(res);
    curium_cmd_free(cmd);
    return exit_code;
}

extern curium_string_t* curium_codegen_v2_to_c(const curium_ast_v2_list_t* ast);

static void curium_check_blacklist_ast_v2(curium_ast_v2_list_t* ast) {
    (void)ast;
    /* TODO: Implement comprehensive blacklist checks for v2 AST */
}

static void curium_resolve_imports_recursive_v2(curium_ast_v2_list_t* main_ast, curium_module_list_t* loaded_paths, curium_module_list_t* source_buffers) {
    if (!main_ast || !loaded_paths || !source_buffers) return;
    
    curium_ast_v2_node_t* current = main_ast->head;
    while (current) {
        if (current->kind == CURIUM_AST_V2_IMPORT) {
            const char* path = NULL;
            if (current->as.import_decl.path) {
                path = current->as.import_decl.path->data;
            } else if (current->as.let_decl.init && current->as.let_decl.init->kind == CURIUM_AST_V2_STRING_LITERAL) {
                path = current->as.let_decl.init->as.string_literal.value->data;
            }

            if (path) {
                /* Form complete path. Simplified approach: just use path as given or relative to src/ or std/ etc */
                char full_path[1024];
                snprintf(full_path, sizeof(full_path), "%s", path);
                
                /* Check if already loaded */
                int already_loaded = 0;
                for (size_t i = 0; i < loaded_paths->count; i++) {
                    if (strcmp(loaded_paths->items[i]->data, full_path) == 0) {
                        already_loaded = 1;
                        break;
                    }
                }
                
                if (!already_loaded) {
                    char* src = curium_read_file_all(full_path);
                    if (!src) {
                        snprintf(full_path, sizeof(full_path), "src/%s", path);
                        src = curium_read_file_all(full_path);
                    }
                    if (!src) {
                        snprintf(full_path, sizeof(full_path), "std/%s", path);
                        src = curium_read_file_all(full_path);
                    }
                    if (src) {
                        curium_module_list_append(loaded_paths, full_path);
                        /* Re-use curium_module_list to hold source buffer pointers */
                        /* Hack: cast char* to curium_string_t* just to hold pointer, we'll free manually */
                        curium_module_list_append(source_buffers, src); 
                        
                        curium_ast_v2_list_t sub_ast = curium_parse_v2(src);
                        if (curium_error_get_last() != CURIUM_ERROR_PARSE) {
                            /* Resolve imports inside the imported file */
                            curium_resolve_imports_recursive_v2(&sub_ast, loaded_paths, source_buffers);
                            
                            /* Append the sub AST to main AST */
                            if (sub_ast.head) {
                                if (!main_ast->head) {
                                    main_ast->head = sub_ast.head;
                                    main_ast->tail = sub_ast.tail;
                                } else {
                                    main_ast->tail->next = sub_ast.head;
                                    main_ast->tail = sub_ast.tail;
                                }
                            }
                        }
                    } else {
                        printf("Warning: could not resolve import '%s'\n", path);
                    }
                }
            }
        }
        current = current->next;
    }
}

int curium_compile_file(const char* entry_path, const char* output_exe) {
    if (!entry_path || !output_exe) return -1;

    char* src = curium_read_file_all(entry_path);
    if (!src) {
        curium_error_set(CURIUM_ERROR_IO, "failed to read entry .cm file");
        return -1;
    }

    curium_ast_v2_list_t ast = {0};
    curium_module_list_t loaded_paths;
    curium_module_list_init(&loaded_paths);
    curium_module_list_t source_buffers;
    curium_module_list_init(&source_buffers);
    
    curium_module_list_append(&loaded_paths, entry_path);

    CURIUM_TRY() {
        ast = curium_parse_v2(src);
        if (curium_error_get_last() == CURIUM_ERROR_PARSE) {
            CURIUM_THROW(CURIUM_ERROR_PARSE, curium_error_get_message());
        }
        
        curium_resolve_imports_recursive_v2(&ast, &loaded_paths, &source_buffers);
        curium_check_blacklist_ast_v2(&ast);
    } CURIUM_CATCH() {
        free(src);
        for (size_t i = 0; i < source_buffers.count; i++) free(source_buffers.items[i]);
        curium_module_list_free(&loaded_paths);
        curium_module_list_free(&source_buffers);
        curium_ast_v2_free_list(&ast);
        return -1;
    }

    curium_string_t* c_code = curium_codegen_v2_to_c(&ast);

    const char* c_path = "curium_out.c";
    if (curium_write_text_file(c_path, c_code->data) != 0) {
        curium_string_free(c_code);
        curium_ast_v2_free_list(&ast);
        free(src);
        for (size_t i = 0; i < source_buffers.count; i++) free(source_buffers.items[i]);
        curium_module_list_free(&loaded_paths);
        curium_module_list_free(&source_buffers);
        curium_error_set(CURIUM_ERROR_IO, "failed to write intermediate C file");
        return -1;
    }

    curium_try_remove_output(output_exe);
    int rc = curium_invoke_system_compiler(c_path, output_exe);

    curium_string_free(c_code);
    curium_ast_v2_free_list(&ast);
    free(src);
    
    for (size_t i = 0; i < source_buffers.count; i++) {
        /* Hack: we stored bare char* in curium_string_t* array to avoid importing new headers */
        free(source_buffers.items[i]);
    }
    
    curium_module_list_free(&loaded_paths);
    free(source_buffers.items); /* Free array but we manually freed the char* items above */

    return rc;
}

int curium_emit_c_file(const char* entry_path, const char* output_c_path) {
    if (!entry_path || !output_c_path) return -1;

    char* src = curium_read_file_all(entry_path);
    if (!src) {
        curium_error_set(CURIUM_ERROR_IO, "failed to read entry .cm file");
        return -1;
    }

    curium_ast_v2_list_t ast = {0};
    curium_module_list_t loaded_paths;
    curium_module_list_init(&loaded_paths);
    curium_module_list_t source_buffers;
    curium_module_list_init(&source_buffers);

    curium_module_list_append(&loaded_paths, entry_path);

    CURIUM_TRY() {
        ast = curium_parse_v2(src);
        if (curium_error_get_last() == CURIUM_ERROR_PARSE) {
            CURIUM_THROW(CURIUM_ERROR_PARSE, curium_error_get_message());
        }
        curium_resolve_imports_recursive_v2(&ast, &loaded_paths, &source_buffers);
        curium_check_blacklist_ast_v2(&ast);
    } CURIUM_CATCH() {
        free(src);
        for (size_t i = 0; i < source_buffers.count; i++) free(source_buffers.items[i]);
        curium_module_list_free(&loaded_paths);
        curium_module_list_free(&source_buffers);
        curium_ast_v2_free_list(&ast);
        return -1;
    }

    curium_string_t* c_code = curium_codegen_v2_to_c(&ast);
    if (curium_write_text_file(output_c_path, c_code->data) != 0) {
        curium_string_free(c_code);
        curium_ast_v2_free_list(&ast);
        free(src);
        curium_error_set(CURIUM_ERROR_IO, "failed to write generated C file");
        return -1;
    }

    curium_string_free(c_code);
    curium_ast_v2_free_list(&ast);
    free(src);
    return 0;
}

