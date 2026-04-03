#include "curium_codegen.h"

#include "curium/error.h"
#include "curium/memory.h"

int curium_opt_show_stat = 0;

#include <stdio.h>
#include <string.h>
#include <ctype.h>


static void curium_emit_prelude(curium_string_t* out) {
    curium_string_append(out, "#include \"curium/core.h\"\n");
    curium_string_append(out, "#include \"curium/error.h\"\n");
    curium_string_append(out, "#include \"curium/memory.h\"\n");
    curium_string_append(out, "#include \"curium/string.h\"\n");
    curium_string_append(out, "#include \"curium/array.h\"\n");
    curium_string_append(out, "#include \"curium/map.h\"\n");
    curium_string_append(out, "#include \"curium/json.h\"\n");
    curium_string_append(out, "#include \"curium/http.h\"\n");
    curium_string_append(out, "#include \"curium/file.h\"\n");
    curium_string_append(out, "#include \"curium/thread.h\"\n\n");

    curium_string_append(out,
        "static void curium_builtin_print(const char* s) { if (s) printf(\"%s\", s); }\n"
        "static void curium_builtin_print_str(curium_string_t* s) { if (s && s->data) printf(\"%s\", s->data); }\n\n");

    curium_string_append(out,
        "static void curium_serve_static(CuriumHttpRequest* req, CuriumHttpResponse* res, const char* path, const char* mime) {\n"
        "    (void)req; curium_res_send_file(res, path, mime);\n"
        "}\n"
        "static void curium_serve_index(CuriumHttpRequest* req, CuriumHttpResponse* res) {\n"
        "    curium_serve_static(req, res, \"public_html/index.html\", \"text/html\");\n"
        "}\n"
        "static void curium_serve_js(CuriumHttpRequest* req, CuriumHttpResponse* res) {\n"
        "    curium_serve_static(req, res, \"public_html/script.js\", \"application/javascript\");\n"
        "}\n"
        "static void curium_serve_css(CuriumHttpRequest* req, CuriumHttpResponse* res) {\n"
        "    curium_serve_static(req, res, \"public_html/style.css\", \"text/css\");\n"
        "}\n\n");
}

static void curium_emit_postlude(curium_string_t* out) {
    if (curium_opt_show_stat) {
        curium_string_append(out, "    curium_gc_stats();\n");
    }
    curium_string_append(out, "    curium_gc_shutdown();\n");
    curium_string_append(out, "    return 0;\n");
    curium_string_append(out, "}\n");
}

static void curium_emit_var_decl(curium_string_t* out, const curium_ast_node_t* n) {
    if (!out || !n) return;
    const char* type = n->as.var_decl.type_name->data;
    /* Trim any leading whitespace from type name */
    while (*type == ' ' || *type == '\t') type++;
    
    if (strcmp(type, "string") == 0 || strcmp(type, "str") == 0) {
        curium_string_append(out, "    curium_string_t* ");
        curium_string_append(out, n->as.var_decl.var_name->data);
        curium_string_append(out, " = ");
        if (strstr(n->as.var_decl.init_expr->data, "input") != NULL) {
            curium_string_append(out, "curium_input(NULL);\n");
        } else if (strstr(n->as.var_decl.init_expr->data, "malloc") != NULL) {
            curium_string_append(out, "curium_alloc(");
            /* Extract size from malloc(size) */
            const char* open = strchr(n->as.var_decl.init_expr->data, '(');
            const char* close = strrchr(n->as.var_decl.init_expr->data, ')');
            if (open && close) {
                curium_string_append(out, open + 1);
            }
            curium_string_append(out, ");\n");
        } else {
            curium_string_append(out, "curium_string_new(");
            curium_string_append(out, n->as.var_decl.init_expr->data);
            curium_string_append(out, ");\n");
        }
    } else if (strcmp(n->as.var_decl.type_name->data, "ptr") == 0) {
        /* ptr type - maps to void* */
        curium_string_append(out, "    void* ");
        curium_string_append(out, n->as.var_decl.var_name->data);
        curium_string_append(out, " = ");
        if (strstr(n->as.var_decl.init_expr->data, "malloc") != NULL) {
            curium_string_append(out, "curium_alloc(");
            /* Extract size from malloc(size) */
            const char* open = strchr(n->as.var_decl.init_expr->data, '(');
            const char* close = strrchr(n->as.var_decl.init_expr->data, ')');
            if (open && close && close > open) {
                /* Extract just the size argument between parens */
                size_t arg_len = (size_t)(close - (open + 1));
                char* arg = (char*)malloc(arg_len + 1);
                if (arg) {
                    memcpy(arg, open + 1, arg_len);
                    arg[arg_len] = '\0';
                    curium_string_append(out, arg);
                    free(arg);
                }
                curium_string_append(out, ", \"ptr\"");
            }
            curium_string_append(out, ");\n");
        } else {
            curium_string_append(out, n->as.var_decl.init_expr->data);
            curium_string_append(out, ";\n");
        }
    } else if (strcmp(n->as.var_decl.type_name->data, "list") == 0) {
        curium_string_append(out, "    curium_list_t* ");
        curium_string_append(out, n->as.var_decl.var_name->data);
        curium_string_append(out, " = curium_list_new();\n");
    } else if (strcmp(n->as.var_decl.type_name->data, "array") == 0) {
        curium_string_append(out, "    curium_dynarray_t* ");
        curium_string_append(out, n->as.var_decl.var_name->data);
        curium_string_append(out, " = curium_dynarray_new(0);\n");
    } else if (strcmp(n->as.var_decl.type_name->data, "map") == 0) {
        curium_string_append(out, "    curium_map_t* ");
        curium_string_append(out, n->as.var_decl.var_name->data);
        curium_string_append(out, " = curium_map_new();\n");
    } else {
        curium_string_append(out, "    ");
        curium_string_append(out, n->as.var_decl.type_name->data);
        curium_string_append(out, " ");
        curium_string_append(out, n->as.var_decl.var_name->data);
        curium_string_append(out, " = ");
        curium_string_append(out, n->as.var_decl.init_expr->data);
        curium_string_append(out, ";\n");
    }
}

static void curium_emit_expr_stmt(curium_string_t* out, const curium_ast_node_t* n) {
    if (!out || !n) return;
    const char* expr = n->as.expr_stmt.expr_text->data ? n->as.expr_stmt.expr_text->data : "";

    const char* trimmed = expr;
    while (*trimmed == ' ' || *trimmed == '\t') trimmed++;

    if (strncmp(trimmed, "gc.", 3) == 0) {
        if (strcmp(trimmed + 3, "stats()") == 0) { curium_string_append(out, "    curium_gc_stats();\n"); return; }
        if (strcmp(trimmed + 3, "collect()") == 0) { curium_string_append(out, "    curium_gc_collect();\n"); return; }
        if (strcmp(trimmed + 3, "leaks()") == 0) { curium_string_append(out, "    curium_gc_print_leaks();\n"); return; }
    }

    /* Native API: gc_collect() */
    if (strcmp(trimmed, "gc_collect()") == 0) {
        curium_string_append(out, "    curium_gc_collect();\n");
        return;
    }

    /* Native API: malloc(x) -> curium_alloc(x, \"ptr\") */
    if (strncmp(trimmed, "malloc(", 7) == 0) {
        const char* open = strchr(trimmed, '(');
        const char* close = strrchr(trimmed, ')');
        if (open && close && close > open) {
            curium_string_append(out, "    curium_alloc(");
            curium_string_append(out, open + 1);
            curium_string_append(out, ");\n");
            return;
        }
    }

    /* Native API: free(x) -> curium_free(x) */
    if (strncmp(trimmed, "free(", 5) == 0) {
        const char* open = strchr(trimmed, '(');
        const char* close = strrchr(trimmed, ')');
        if (open && close && close > open) {
            curium_string_append(out, "    curium_free(");
            /* Extract just the argument between parens */
            size_t arg_len = (size_t)(close - (open + 1));
            char* arg = (char*)malloc(arg_len + 1);
            if (arg) {
                memcpy(arg, open + 1, arg_len);
                arg[arg_len] = '\0';
                curium_string_append(out, arg);
                free(arg);
            }
            curium_string_append(out, ");\n");
            return;
        }
    }

    if (strncmp(trimmed, "print", 5) == 0) {
        const char* open = strchr(trimmed, '(');
        const char* close = open ? strrchr(trimmed, ')') : NULL;
        if (open && close && close > open + 1) {
            size_t arg_len = (size_t)(close - (open + 1));
            curium_string_t* arg = curium_string_new("");
            if (arg_len > 0) {
                char* tmp = (char*)malloc(arg_len + 1);
                if (tmp) {
                    memcpy(tmp, open + 1, arg_len);
                    tmp[arg_len] = '\0';
                    curium_string_set(arg, tmp);
                    free(tmp);
                }
            }

            const char* a = arg->data;
            while (*a == ' ' || *a == '\t') a++;
            if (*a == '"') {
                curium_string_append(out, "    curium_builtin_print(");
                curium_string_append(out, a);
                curium_string_append(out, ");\n");
            } else {
                /* For string literals passed from CM (which lost their quotes during lexing),
                 * we need to wrap them in quotes for C */
                
                /* Check if this is a simple identifier (single word, alphanumeric + underscore) */
                const char* p = a;
                int has_space = 0;
                int has_special = 0;
                size_t token_len = 0;
                
                while (*p && *p != ')' && *p != '\n' && *p != '\r') {
                    if (*p == ' ' || *p == '\t') has_space = 1;
                    if (!isalnum((unsigned char)*p) && *p != '_') has_special = 1;
                    token_len++;
                    p++;
                }
                
                int is_simple_ident = !has_space && !has_special && token_len > 0 &&
                                      (isalpha((unsigned char)a[0]) || a[0] == '_');

                if (is_simple_ident) {
                    curium_string_append(out, "    curium_builtin_print_str(");
                    curium_string_append(out, a);
                    curium_string_append(out, ");\n");
                } else {
                    /* String literal content without quotes - wrap in quotes */
                    curium_string_append(out, "    curium_builtin_print(\"");
                    curium_string_append(out, a);
                    curium_string_append(out, "\");\n");
                }
            }
            curium_string_free(arg);
            return;
        }
    }

    /* Basic string method lowering: name.upper(), name.lower(), name.append(x) */
    const char* dot = strchr(trimmed, '.');
    const char* open = dot ? strchr(dot, '(') : NULL;
    const char* close = open ? strrchr(open, ')') : NULL;
    if (dot && open && close && close >= open) {
        size_t recv_len = (size_t)(dot - trimmed);
        while (recv_len > 0 && isspace((unsigned char)trimmed[recv_len - 1])) recv_len--;

        size_t method_len = (size_t)(open - (dot + 1));
        while (method_len > 0 && isspace((unsigned char)dot[1])) { dot++; method_len--; }
        while (method_len > 0 && isspace((unsigned char)(dot[1 + method_len - 1]))) method_len--;

        char recv[256] = {0};
        char method[64] = {0};
        if (recv_len < sizeof(recv) && method_len < sizeof(method)) {
            memcpy(recv, trimmed, recv_len); recv[recv_len] = '\0';
            memcpy(method, dot + 1, method_len); method[method_len] = '\0';
            if (strcmp(method, "upper") == 0) {
                curium_string_append(out, "    curium_string_upper("); curium_string_append(out, recv); curium_string_append(out, ");\n"); return;
            }
            if (strcmp(method, "lower") == 0) {
                curium_string_append(out, "    curium_string_lower("); curium_string_append(out, recv); curium_string_append(out, ");\n"); return;
            }
            if (strcmp(method, "append") == 0) {
                size_t arg_len = (size_t)(close - open - 1);
                char argbuf[512] = {0};
                if (arg_len < sizeof(argbuf)) {
                    memcpy(argbuf, open + 1, arg_len); argbuf[arg_len] = '\0';
                    curium_string_append(out, "    curium_string_append(");
                    curium_string_append(out, recv);
                    curium_string_append(out, ", ");
                    curium_string_append(out, argbuf);
                    curium_string_append(out, ");\n");
                    return;
                }
            }
        }
    }

    curium_string_append(out, "    ");
    curium_string_append(out, expr);
    curium_string_append(out, ";\n");
}

static void curium_emit_poly_block(curium_string_t* out, const curium_ast_node_t* n) {
    if (!out || !n) return;
    curium_string_append(out, "\n");
    curium_string_append(out, n->as.poly_block.code->data);
    curium_string_append(out, "\n");
}

static void curium_string_replace(curium_string_t* str, const char* from, const char* to) {
    if (!str || !str->data || !from || !to) return;
    size_t from_len = strlen(from);
    size_t to_len = strlen(to);
    char* pos = strstr(str->data, from);
    while (pos) {
        size_t offset = (size_t)(pos - str->data);
        size_t tail_len = str->length - offset - from_len;
        size_t new_len = str->length - from_len + to_len;
        char* new_data = (char*)curium_alloc(new_len + 1, "curium_string_data");
        memcpy(new_data, str->data, offset);
        memcpy(new_data + offset, to, to_len);
        memcpy(new_data + offset + to_len, pos + from_len, tail_len);
        new_data[new_len] = '\0';
        curium_free(str->data);
        str->data = new_data;
        str->length = new_len;
        str->capacity = new_len + 1;
        pos = strstr(str->data + offset + to_len, from);
    }
}

static int curium_is_global_expr(const char* expr) {
    while (*expr == ' ' || *expr == '\n' || *expr == '\r' || *expr == '\t') expr++;
    if (strncmp(expr, "void ", 5) == 0) return 1;
    if (strncmp(expr, "int ", 4) == 0) return 1;
    if (strncmp(expr, "class ", 6) == 0) return 1;
    if (strncmp(expr, "struct ", 7) == 0) return 1;
    return 0;
}

curium_string_t* curium_codegen_to_c(const curium_ast_list_t* ast) {
    curium_string_t* out = curium_string_new("");
    curium_emit_prelude(out);

    /* Transpiler syntactic sugar replacements at AST level */
    int has_main = 0;
    for (const curium_ast_node_t* n = ast ? ast->head : NULL; n; n = n->next) {
        if (n->kind == CURIUM_AST_EXPR_STMT && n->as.expr_stmt.expr_text) {
            /* Allow v2-ish entrypoint syntax in the legacy pipeline. */
            curium_string_replace(n->as.expr_stmt.expr_text, "fn main()", "void main()");
            /* Map println("...") to the existing builtin print. */
            curium_string_replace(n->as.expr_stmt.expr_text, "println(\"", "curium_builtin_print(\"");

            curium_string_replace(n->as.expr_stmt.expr_text, "void main()", "void __curium_main()");
            curium_string_replace(n->as.expr_stmt.expr_text, "app.get", "curium_app_get");
            curium_string_replace(n->as.expr_stmt.expr_text, "app.post", "curium_app_post");
            curium_string_replace(n->as.expr_stmt.expr_text, "app.listen", "curium_app_listen");
            curium_string_replace(n->as.expr_stmt.expr_text, "(req,res)=>{", "({ void __fn(CuriumHttpRequest* req, CuriumHttpResponse* res) {");
            curium_string_replace(n->as.expr_stmt.expr_text, "});", "} __fn; });");
            curium_string_replace(n->as.expr_stmt.expr_text, "html.serve(\"index.html\")", "curium_res_send_file(res, \"public_html/index.html\", \"text/html\")");
            curium_string_replace(n->as.expr_stmt.expr_text, "html.serve(", "curium_res_send_file(res, ");

            if (strstr(n->as.expr_stmt.expr_text->data, "void __curium_main()")) has_main = 1;
        }
    }

    /* Pass 1: Global functions and classes */
    for (const curium_ast_node_t* n = ast ? ast->head : NULL; n; n = n->next) {
        if (n->kind == CURIUM_AST_EXPR_STMT && curium_is_global_expr(n->as.expr_stmt.expr_text->data)) {
            curium_emit_expr_stmt(out, n);
            curium_string_append(out, "\n");
        }
        else if (n->kind == CURIUM_AST_POLYGLOT) {
            curium_emit_poly_block(out, n);
        }
    }

    /* Pass 2: Main entry point */
    curium_string_append(out, "int main(void) {\n");
    curium_string_append(out, "    curium_gc_init();\n");
    curium_string_append(out, "    curium_init_error_detector();\n\n");

    if (has_main) {
        curium_string_append(out, "    __curium_main();\n");
    }

    for (const curium_ast_node_t* n = ast ? ast->head : NULL; n; n = n->next) {
        if (n->kind == CURIUM_AST_EXPR_STMT && curium_is_global_expr(n->as.expr_stmt.expr_text->data)) continue;
        if (n->kind == CURIUM_AST_POLYGLOT) continue;

        switch (n->kind) {
            case CURIUM_AST_VAR_DECL: curium_emit_var_decl(out, n); break;
            case CURIUM_AST_EXPR_STMT: curium_emit_expr_stmt(out, n); break;
            case CURIUM_AST_REQUIRE: break;
            default: break;
        }
    }

    curium_emit_postlude(out);
    return out;
}

