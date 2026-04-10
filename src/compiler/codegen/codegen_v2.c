#include "curium/compiler/ast_v2.h"
#include "curium/memory.h"
#include "curium/error.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
/* ctype.h removed — no longer needed */

/* ============================================================================
 * CM v2 Codegen - Generate hardened C99 code
 *
 * Language design goals implemented here:
 *   - Native C speed:     emit straight C99 with zero overhead abstractions
 *   - Go ease:            simple, readable generated code
 *   - Rust safety:        bounds-checked indexing, safe-deref, Result/Option types
 *   - Modern C clarity:   use const-correct types, no 'auto' (C99 compat)
 * ==========================================================================*/

extern int curium_opt_show_stat;

typedef struct {
    char* method_name;  /* FIX #003: owned copy (strdup), freed in destroy */
    char* type_name;    /* FIX #003: owned copy (strdup), freed in destroy */
} curium_method_info_t;

typedef struct {
    curium_string_t* includes;
    curium_string_t* forward_decls;
    curium_string_t* closures;
    curium_string_t* output;
    int          indent_level;
    curium_string_t* current_function;
    int          temp_counter;
    curium_method_info_t methods[512];
    int          method_count;
    int          in_dyn_body;  /* set to 1 inside dyn arm/fallback body */
} curium_codegen_v2_t;

static void curium_codegen_v2_init(curium_codegen_v2_t* cg) {
    memset(cg, 0, sizeof(*cg));
    cg->includes         = curium_string_new("");
    cg->forward_decls    = curium_string_new("");
    cg->closures         = curium_string_new("\n/* Closure Functions */\n");
    cg->output           = curium_string_new("");
    cg->indent_level     = 0;
    cg->current_function = curium_string_new("");
    cg->temp_counter     = 0;
}

static void curium_codegen_v2_destroy(curium_codegen_v2_t* cg) {
    /* FIX #003: free the owned method name/type copies before freeing strings */
    {
        int i;
        for (i = 0; i < cg->method_count; i++) {
            free(cg->methods[i].method_name);
            free(cg->methods[i].type_name);
            cg->methods[i].method_name = NULL;
            cg->methods[i].type_name   = NULL;
        }
    }
    if (cg->includes)         curium_string_free(cg->includes);
    if (cg->forward_decls)    curium_string_free(cg->forward_decls);
    if (cg->closures)         curium_string_free(cg->closures);
    if (cg->output)           curium_string_free(cg->output);
    if (cg->current_function) curium_string_free(cg->current_function);
}

static void curium_codegen_v2_indent(curium_codegen_v2_t* cg) {
    int i;
    for (i = 0; i < cg->indent_level; i++) {
        curium_string_append(cg->output, "    ");
    }
}

static void curium_codegen_v2_newline(curium_codegen_v2_t* cg) {
    curium_string_append(cg->output, "\n");
}

/* Name mangling: prefix all user identifiers with curium_ to avoid C keyword conflicts.
 * BUG FIX: was using a static buffer — unsafe for nested calls.
 * Now returns a freshly allocated curium_string_t; caller must free.
 * FIX #018: guard against double-mangling — if name already starts with "curium_"
 * (7 chars) we skip the prefix.  The old code used strncmp(..., 3) which matched
 * only "cur", accepting false positives like "cured" or "curry".              */
static curium_string_t* curium_codegen_v2_mangle(const char* name) {
    if (!name) name = "unknown";
    /* FIX #018: exact 7-char prefix check so we don't emit curium_curium_xxx */
    if (strncmp(name, "curium_", 7) == 0) {
        return curium_string_new(name); /* already mangled, return as-is */
    }
    return curium_string_format("curium_%s", name);
}

/* Map CM type node to a C99 type string (static, short-lived). */
static const char* curium_codegen_v2_type_to_c(curium_ast_v2_node_t* type) {
    if (!type) return "void";

    switch (type->kind) {
        case CURIUM_AST_V2_TYPE_NAMED:
            if (!type->as.type_named.name) return "void";
            if (strcmp(type->as.type_named.name->data, "int")    == 0) return "int";
            if (strcmp(type->as.type_named.name->data, "float")  == 0) return "double";
            if (strcmp(type->as.type_named.name->data, "string") == 0) return "curium_string_t* restrict";
            if (strcmp(type->as.type_named.name->data, "bool")   == 0) return "int";
            if (strcmp(type->as.type_named.name->data, "void")   == 0) return "void";
            /* v4.0: Sized numeric types */
            if (strcmp(type->as.type_named.name->data, "i8")     == 0) return "int8_t";
            if (strcmp(type->as.type_named.name->data, "i16")    == 0) return "int16_t";
            if (strcmp(type->as.type_named.name->data, "i32")    == 0) return "int";
            if (strcmp(type->as.type_named.name->data, "i64")    == 0) return "int64_t";
            if (strcmp(type->as.type_named.name->data, "u8")     == 0) return "uint8_t";
            if (strcmp(type->as.type_named.name->data, "u16")    == 0) return "uint16_t";
            if (strcmp(type->as.type_named.name->data, "u32")    == 0) return "uint32_t";
            if (strcmp(type->as.type_named.name->data, "u64")    == 0) return "uint64_t";
            if (strcmp(type->as.type_named.name->data, "f32")    == 0) return "float";
            if (strcmp(type->as.type_named.name->data, "f64")    == 0) return "double";
            if (strcmp(type->as.type_named.name->data, "usize")  == 0) return "size_t";
            return type->as.type_named.name->data; /* user-defined struct name */

        case CURIUM_AST_V2_TYPE_DYN:    return "curium_string_t*";
        case CURIUM_AST_V2_TYPE_PTR: {
            static char ptr_buf[16][4096];
            static int ptr_buf_idx = 0;
            const char* base = curium_codegen_v2_type_to_c(type->as.type_ptr.base);
            ptr_buf_idx = (ptr_buf_idx + 1) % 16;
            snprintf(ptr_buf[ptr_buf_idx], 4096, "%s*", base);
            return ptr_buf[ptr_buf_idx];
        }
        case CURIUM_AST_V2_TYPE_OPTION: return "curium_option_t";
        case CURIUM_AST_V2_TYPE_RESULT: return "curium_result_t";
        case CURIUM_AST_V2_TYPE_ARRAY:  return "curium_array_t* restrict";
        case CURIUM_AST_V2_TYPE_SLICE:  return "curium_slice_t";
        case CURIUM_AST_V2_TYPE_MAP:    return "curium_map_t* restrict";
        case CURIUM_AST_V2_TYPE_FN:     return "void* /* closure/fn */";
        default:                    return "void";
    }
}

/* Forward declaration */
static void curium_codegen_v2_generate_expr(curium_codegen_v2_t* cg, curium_ast_v2_node_t* expr);
static void curium_codegen_v2_generate_stmt(curium_codegen_v2_t* cg, curium_ast_v2_node_t* stmt);

/* ============================================================================
 * Expression generators
 * ==========================================================================*/

static void curium_codegen_v2_generate_binary_op(curium_codegen_v2_t* cg, curium_ast_v2_node_t* binary) {
    curium_string_append(cg->output, "(");
    curium_codegen_v2_generate_expr(cg, binary->as.binary_expr.left);
    curium_string_append(cg->output, " ");
    curium_string_append(cg->output, binary->as.binary_expr.op->data);
    curium_string_append(cg->output, " ");
    curium_codegen_v2_generate_expr(cg, binary->as.binary_expr.right);
    curium_string_append(cg->output, ")");
}

static void curium_codegen_v2_generate_unary_op(curium_codegen_v2_t* cg, curium_ast_v2_node_t* unary) {
    const char* op = unary->as.unary_expr.op->data;

    if (strcmp(op, "!") == 0) {
        /* Error propagation operator:
         * CM:  result!
         * C99: curium_result_unwrap_or_return(result)
         * This is safe-like-Rust: propagates error up the call stack. */
        curium_string_append(cg->output, "curium_result_unwrap_or_return(");
        curium_codegen_v2_generate_expr(cg, unary->as.unary_expr.expr);
        curium_string_append(cg->output, ")");
    } else if (strcmp(op, "^") == 0) {
        /* In CM, ^x is address-of, which maps to C99 &x */
        curium_string_append(cg->output, "(&");
        curium_codegen_v2_generate_expr(cg, unary->as.unary_expr.expr);
        curium_string_append(cg->output, ")");
    } else {
        curium_string_append(cg->output, "(");
        curium_string_append(cg->output, op);
        curium_codegen_v2_generate_expr(cg, unary->as.unary_expr.expr);
        curium_string_append(cg->output, ")");
    }
}

static void curium_codegen_v2_generate_call(curium_codegen_v2_t* cg, curium_ast_v2_node_t* call) {
    if (call->as.call_expr.callee && call->as.call_expr.callee->kind == CURIUM_AST_V2_FIELD_ACCESS) {
        /* Method call: obj.method(...) -> curium_OOP_method((obj), ...) */
        curium_ast_v2_node_t* obj = call->as.call_expr.callee->as.field_access.object;
        curium_string_t* method_name = call->as.call_expr.callee->as.field_access.field;
        
        /* Look up if method is an impl method */
        const char* target_type = NULL;
        int match_count = 0;
        for (int i = 0; i < cg->method_count; i++) {
            if (strcmp(cg->methods[i].method_name, method_name->data) == 0) {
                target_type = cg->methods[i].type_name;
                match_count++;
            }
        }
        
        if (match_count == 1) {
            curium_string_append(cg->output, "curium_");
            curium_string_append(cg->output, target_type);
            curium_string_append(cg->output, "_");
            curium_string_append(cg->output, method_name->data);
            curium_string_append(cg->output, "(");
            
            /* Pass the object as the first argument (self) */
            /* If object needs to be cast or referenced, do it here. For now, pass directly. */
            curium_codegen_v2_generate_expr(cg, obj);
            
            curium_ast_v2_node_t* arg = call->as.call_expr.args;
            if (arg) {
                curium_string_append(cg->output, ", ");
            }
            while (arg) {
                curium_codegen_v2_generate_expr(cg, arg);
                if (arg->next) curium_string_append(cg->output, ", ");
                arg = arg->next;
            }
            curium_string_append(cg->output, ")");
            return;
        } else if (match_count > 1) {
            curium_string_append(cg->output, "/* ERROR: AMBIGUOUS METHOD CALL: ");
            curium_string_append(cg->output, method_name->data);
            curium_string_append(cg->output, " */ ");
            /* Fallthrough to normal generation just to prevent total crash */
        }
        /* If match_count == 0, it falls through to standard field access generation */
    }

    curium_codegen_v2_generate_expr(cg, call->as.call_expr.callee);
    curium_string_append(cg->output, "(");

    curium_ast_v2_node_t* arg = call->as.call_expr.args;
    while (arg) {
        curium_codegen_v2_generate_expr(cg, arg);
        if (arg->next) curium_string_append(cg->output, ", ");
        arg = arg->next;
    }

    curium_string_append(cg->output, ")");
}

static void curium_codegen_v2_generate_field_access(curium_codegen_v2_t* cg, curium_ast_v2_node_t* access) {
    curium_codegen_v2_generate_expr(cg, access->as.field_access.object);
    curium_string_append(cg->output, ".");
    curium_string_append(cg->output, access->as.field_access.field->data);
}

/* BUG FIX: bounds check is now emitted as a STATEMENT before the expression,
 * not inline inside it (which produced invalid C). The index expr is now pure. */
static void curium_codegen_v2_generate_index_stmt(curium_codegen_v2_t* cg, curium_ast_v2_node_t* index) {
    /* This helper emits the safety check as a standalone statement. */
    curium_codegen_v2_indent(cg);
    curium_string_append(cg->output, "CURIUM_BOUNDS_CHECK(");
    curium_codegen_v2_generate_expr(cg, index->as.index_expr.array);
    curium_string_append(cg->output, ", ");
    curium_codegen_v2_generate_expr(cg, index->as.index_expr.index);
    curium_string_append(cg->output, ");");
    curium_codegen_v2_newline(cg);
}

static void curium_codegen_v2_generate_index_expr(curium_codegen_v2_t* cg, curium_ast_v2_node_t* index) {
    /* Pure expression form — bounds check must be emitted separately as a stmt. */
    curium_codegen_v2_generate_expr(cg, index->as.index_expr.array);
    curium_string_append(cg->output, "[");
    curium_codegen_v2_generate_expr(cg, index->as.index_expr.index);
    curium_string_append(cg->output, "]");
}

static void curium_codegen_v2_generate_deref(curium_codegen_v2_t* cg, curium_ast_v2_node_t* deref) {
    /* Safe dereference — macro checks for NULL before deref (Rust-safety principle) */
    curium_string_append(cg->output, "CURIUM_SAFE_DEREF(");
    curium_codegen_v2_generate_expr(cg, deref->as.deref_expr.expr);
    curium_string_append(cg->output, ")");
}

static void curium_codegen_v2_generate_addr_of(curium_codegen_v2_t* cg, curium_ast_v2_node_t* addr_of) {
    curium_string_append(cg->output, "CURIUM_SAFE_ADDR_OF(");
    curium_codegen_v2_generate_expr(cg, addr_of->as.addr_of_expr.expr);
    curium_string_append(cg->output, ")");
}

/* Interpolated string: "hello {name}, age {age}"
 * BUG FIX: was calling generate_expr(parts) but parts was always NULL.
 * Now properly parses the template to extract variable names inline. */
static void curium_codegen_v2_generate_interpolated_string(curium_codegen_v2_t* cg, curium_ast_v2_node_t* interp) {
    const char* tmpl = interp->as.interpolated_string.template
                     ? interp->as.interpolated_string.template->data
                     : "";

    /* Build format string and collect variable names in one pass */
    curium_string_t* fmt_str  = curium_string_new("\"");
    curium_string_t* var_args = curium_string_new("");
    int first_arg = 1;

    const char* p = tmpl;
    while (*p) {
        if (*p == '{') {
            const char* end = strchr(p + 1, '}');
            if (end && end > p + 1) {
                size_t var_len = (size_t)(end - p - 1);
                char var_buf[256];
                if (var_len < sizeof(var_buf) - 1) {
                    memcpy(var_buf, p + 1, var_len);
                    var_buf[var_len] = '\0';

                    /* FIX #002/#008: always use %s — curium_any_to_str() converts
                     * any scalar (int, double, bool, string*) to char* safely,
                     * eliminating the UB of passing int to %s format specifier. */
                    curium_string_append(fmt_str, "%s");

                    if (!first_arg) curium_string_append(var_args, ", ");
                    first_arg = 0;

                    /* Wrap in curium_any_to_str() so the arg is always char* */
                    curium_string_append(var_args, "curium_any_to_str(");
                    curium_string_t* mangled = curium_codegen_v2_mangle(var_buf);
                    curium_string_append(var_args, mangled->data);
                    curium_string_free(mangled);
                    curium_string_append(var_args, ")");
                }
                p = end + 1;
                continue;
            }
        }
        if (*p == '"') {
            curium_string_append(fmt_str, "\\\"");
        } else if (*p == '\n') {
            curium_string_append(fmt_str, "\\n");
        } else if (*p == '\t') {
            curium_string_append(fmt_str, "\\t");
        } else if (*p == '\\') {
            curium_string_append(fmt_str, "\\\\");
        } else {
            char buf[2] = {*p, '\0'};
            curium_string_append(fmt_str, buf);
        }
        p++;
    }
    curium_string_append(fmt_str, "\"");

    curium_string_append(cg->output, "curium_string_format(");
    curium_string_append(cg->output, fmt_str->data);
    if (var_args->length > 0) {
        curium_string_append(cg->output, ", ");
        curium_string_append(cg->output, var_args->data);
    }
    curium_string_append(cg->output, ")");

    curium_string_free(fmt_str);
    curium_string_free(var_args);
}

static void curium_codegen_v2_generate_literal(curium_codegen_v2_t* cg, curium_ast_v2_node_t* lit) {
    switch (lit->kind) {
        case CURIUM_AST_V2_STRING_LITERAL:
            curium_string_append(cg->output, "curium_string_new(\"");
            {
                const char* s = lit->as.string_literal.value
                              ? lit->as.string_literal.value->data : "";
                while (*s) {
                    if (*s == '"')       curium_string_append(cg->output, "\\\"");
                    else if (*s == '\n') curium_string_append(cg->output, "\\n");
                    else if (*s == '\t') curium_string_append(cg->output, "\\t");
                    else if (*s == '\\') curium_string_append(cg->output, "\\\\");
                    else {
                        char buf[2] = {*s, '\0'};
                        curium_string_append(cg->output, buf);
                    }
                    s++;
                }
            }
            curium_string_append(cg->output, "\")");
            break;

        case CURIUM_AST_V2_NUMBER:
            curium_string_append(cg->output, lit->as.number_literal.value
                             ? lit->as.number_literal.value->data : "0");
            break;

        case CURIUM_AST_V2_BOOL:
            curium_string_append(cg->output, lit->as.bool_literal.value ? "1" : "0");
            break;

        case CURIUM_AST_V2_IDENTIFIER: {
            /* BUG FIX: was using static buffer — now properly heap-allocated */
            curium_string_t* mangled = curium_codegen_v2_mangle(
                lit->as.identifier.value ? lit->as.identifier.value->data : "");
            curium_string_append(cg->output, mangled->data);
            curium_string_free(mangled);
            break;
        }

        case CURIUM_AST_V2_OPTION_NONE:
            curium_string_append(cg->output, "CURIUM_OPTION_NONE");
            break;

        case CURIUM_AST_V2_OPTION_SOME:
            curium_string_append(cg->output, "CURIUM_OPTION_SOME(");
            curium_codegen_v2_generate_expr(cg, lit->as.option_some.value);
            curium_string_append(cg->output, ")");
            break;

        case CURIUM_AST_V2_RESULT_OK:
            curium_string_append(cg->output, "CURIUM_RESULT_OK(");
            curium_codegen_v2_generate_expr(cg, lit->as.result_ok.value);
            curium_string_append(cg->output, ")");
            break;

        case CURIUM_AST_V2_RESULT_ERR:
            curium_string_append(cg->output, "CURIUM_RESULT_ERR(");
            curium_codegen_v2_generate_expr(cg, lit->as.result_err.value);
            curium_string_append(cg->output, ")");
            break;

        default:
            curium_string_append(cg->output, "/* unknown literal */");
            break;
    }
}

/* Dynamic operator call expression: x action y → _curium_dyn_action(curium_string_format(...), curium_action, curium_string_format(...)) */
static void curium_codegen_v2_generate_dyn_call(curium_codegen_v2_t* cg, curium_ast_v2_node_t* expr) {
    curium_string_append(cg->output, "_curium_dyn_");
    curium_string_append(cg->output, expr->as.dyn_call.op_name->data);
    curium_string_append(cg->output, "(");
    /* Wrap left operand: convert to curium_string_t* */
    curium_string_append(cg->output, "curium_string_format(\"%g\", (double)(");
    curium_codegen_v2_generate_expr(cg, expr->as.dyn_call.left);
    curium_string_append(cg->output, "))");
    curium_string_append(cg->output, ", ");
    {
        curium_string_t* mangled = curium_codegen_v2_mangle(expr->as.dyn_call.op_name->data);
        curium_string_append(cg->output, mangled->data);
        curium_string_free(mangled);
    }
    curium_string_append(cg->output, ", ");
    /* Wrap right operand: convert to curium_string_t* */
    curium_string_append(cg->output, "curium_string_format(\"%g\", (double)(");
    curium_codegen_v2_generate_expr(cg, expr->as.dyn_call.right);
    curium_string_append(cg->output, "))");
    curium_string_append(cg->output, ")");
}

static void curium_codegen_v2_generate_expr(curium_codegen_v2_t* cg, curium_ast_v2_node_t* expr) {
    if (!expr) return;

    switch (expr->kind) {
        case CURIUM_AST_V2_BINARY_OP:           curium_codegen_v2_generate_binary_op(cg, expr); break;
        case CURIUM_AST_V2_UNARY_OP:            curium_codegen_v2_generate_unary_op(cg, expr); break;
        case CURIUM_AST_V2_CALL:                curium_codegen_v2_generate_call(cg, expr); break;
        case CURIUM_AST_V2_FIELD_ACCESS:        curium_codegen_v2_generate_field_access(cg, expr); break;
        case CURIUM_AST_V2_INDEX:               curium_codegen_v2_generate_index_expr(cg, expr); break;
        case CURIUM_AST_V2_DEREF:               curium_codegen_v2_generate_deref(cg, expr); break;
        case CURIUM_AST_V2_ADDR_OF:             curium_codegen_v2_generate_addr_of(cg, expr); break;
        case CURIUM_AST_V2_INTERPOLATED_STRING: curium_codegen_v2_generate_interpolated_string(cg, expr); break;
        case CURIUM_AST_V2_DYN_CALL:            curium_codegen_v2_generate_dyn_call(cg, expr); break;
        case CURIUM_AST_V2_TRY_EXPR:
            curium_string_append(cg->output, "curium_result_unwrap_or_throw(");
            curium_codegen_v2_generate_expr(cg, expr->as.try_expr.expr);
            curium_string_append(cg->output, ")");
            break;

        case CURIUM_AST_V2_SPAWN:
            curium_string_append(cg->output, "curium_thread_create((void*(*)(void*))");
            curium_codegen_v2_generate_expr(cg, expr->as.spawn_stmt.closure);
            curium_string_append(cg->output, ", NULL)");
            break;

        case CURIUM_AST_V2_CLOSURE: {
            char name[64];
            snprintf(name, sizeof(name), "_curium_lambda_%d", cg->temp_counter++);
            
            curium_string_append(cg->forward_decls, "static inline ");
            curium_string_append(cg->closures, "static inline ");
            
            const char* ret_c = expr->as.closure_expr.return_type ? curium_codegen_v2_type_to_c(expr->as.closure_expr.return_type) : "int";
            curium_string_append(cg->forward_decls, ret_c);
            curium_string_append(cg->closures, ret_c);
            
            curium_string_append(cg->forward_decls, " ");
            curium_string_append(cg->closures, " ");
            
            curium_string_append(cg->forward_decls, name);
            curium_string_append(cg->closures, name);
            
            curium_string_append(cg->forward_decls, "(");
            curium_string_append(cg->closures, "(");
            
            curium_ast_v2_node_t* param = expr->as.closure_expr.params;
            if (!param) {
                curium_string_append(cg->forward_decls, "void");
                curium_string_append(cg->closures, "void");
            }
            while (param) {
                const char* pt = param->as.param.type ? curium_codegen_v2_type_to_c(param->as.param.type) : "int";
                curium_string_append(cg->forward_decls, pt);
                curium_string_append(cg->closures, pt);
                
                curium_string_append(cg->forward_decls, " ");
                curium_string_append(cg->closures, " ");
                
                curium_string_t* pname = curium_codegen_v2_mangle(param->as.param.name ? param->as.param.name->data : "");
                curium_string_append(cg->closures, pname->data);
                // forward decl doesn't technically need the parameter name but it's fine
                curium_string_free(pname);
                
                if (param->next) {
                    curium_string_append(cg->forward_decls, ", ");
                    curium_string_append(cg->closures, ", ");
                }
                param = param->next;
            }
            curium_string_append(cg->forward_decls, ");\n");
            curium_string_append(cg->closures, ") {\n");
            
            curium_string_t* old_out = cg->output;
            int old_indent = cg->indent_level;
            
            cg->output = cg->closures;
            cg->indent_level = 1;
            
            curium_codegen_v2_generate_stmt(cg, expr->as.closure_expr.body);
            
            cg->output = old_out;
            cg->indent_level = old_indent;
            
            curium_string_append(cg->closures, "}\n\n");
            curium_string_append(cg->output, "(void*)");
            curium_string_append(cg->output, name);
            break;
        }

        case CURIUM_AST_V2_STRING_LITERAL:
        case CURIUM_AST_V2_NUMBER:
        case CURIUM_AST_V2_BOOL:
        case CURIUM_AST_V2_IDENTIFIER:
        case CURIUM_AST_V2_OPTION_NONE:
        case CURIUM_AST_V2_OPTION_SOME:
        case CURIUM_AST_V2_RESULT_OK:
        case CURIUM_AST_V2_RESULT_ERR:
            curium_codegen_v2_generate_literal(cg, expr);
            break;

        default:
            curium_string_append(cg->output, "/* unknown expression */");
            break;
    }
}

/* ============================================================================
 * Statement generators
 * ==========================================================================*/

static void curium_codegen_v2_generate_let_mut(curium_codegen_v2_t* cg, curium_ast_v2_node_t* decl, int is_mutable) {
    const char* type_str;
    curium_ast_v2_node_t* decl_data;

    /* Both let_decl and mut_decl share the same layout — use let_decl fields */
    decl_data = decl;

    if (decl_data->as.let_decl.type) {
        type_str = curium_codegen_v2_type_to_c(decl_data->as.let_decl.type);
    } else if (decl_data->as.let_decl.init &&
               decl_data->as.let_decl.init->kind == CURIUM_AST_V2_DYN_CALL) {
        /* DYN_CALL returns curium_string_t* */
        type_str = "curium_string_t*";
    } else if (decl_data->as.let_decl.init &&
               decl_data->as.let_decl.init->kind == CURIUM_AST_V2_SPAWN) {
        type_str = "CMThread";
    } else {
        /* BUG FIX: was using C++ 'auto' which is invalid as a type in C99.
         * Default to 'int' (Go-style: most common numeric default). */
        type_str = "int";
    }

    curium_codegen_v2_indent(cg);

    /* v5.0 Phase 3 — Developer Cache Control: #[hot] → register hint
     *
     * Cache Metaphor: `register` tells the C compiler (the Chef's assistant)
     * to try to keep this ingredient on the Cutting Board (CPU registers)
     * instead of fetching it from the Fridge (RAM/stack) on every use.
     *
     * C99 §6.7.1: `register` is a valid storage-class specifier.
     * Portable pragma block suppresses "deprecated in C++11" warnings
     * when the generated C is compiled as C++.                           */
    if (decl_data->as.let_decl.is_hot) {
        curium_string_append(cg->output,
            "#if defined(__cplusplus)\n"
            "#  pragma GCC diagnostic push\n"
            "#  pragma GCC diagnostic ignored \"-Wregister\"\n"
            "#endif\n");
        curium_codegen_v2_indent(cg);
        curium_string_append(cg->output, "register ");
    }

    if (!is_mutable) {
        /* let → const, immutable by default like Rust */
        curium_string_append(cg->output, "const ");
    }

    curium_string_append(cg->output, type_str);
    curium_string_append(cg->output, " ");

    {
        curium_string_t* mangled = curium_codegen_v2_mangle(
            decl_data->as.let_decl.name ? decl_data->as.let_decl.name->data : "");
        curium_string_append(cg->output, mangled->data);
        curium_string_free(mangled);
    }

    if (decl_data->as.let_decl.init) {
        curium_string_append(cg->output, " = ");
        curium_codegen_v2_generate_expr(cg, decl_data->as.let_decl.init);
    } else {
        /* Default construct to prevent C undefined behavior */
        curium_string_append(cg->output, " = 0");
    }
    curium_string_append(cg->output, ";");
    curium_codegen_v2_newline(cg);
    /* v5.0 Phase 3: close the C++ register-pragma guard opened for #[hot] */
    if (decl_data->as.let_decl.is_hot) {
        curium_string_append(cg->output,
            "#if defined(__cplusplus)\n"
            "#  pragma GCC diagnostic pop\n"
            "#endif\n");
    }
}


static void curium_codegen_v2_generate_fn(curium_codegen_v2_t* cg, curium_ast_v2_node_t* fn) {
    curium_codegen_v2_indent(cg);

    /* Add static if not public and not main */
    const char* fn_name = fn->as.fn_decl.name ? fn->as.fn_decl.name->data : "";
    if (!fn->as.fn_decl.is_public && strcmp(fn_name, "main") != 0 && strcmp(fn_name, "test_runner_main") != 0) {
        curium_string_append(cg->output, "static ");
    }

    /* Return type */
    curium_string_append(cg->output, curium_codegen_v2_type_to_c(fn->as.fn_decl.return_type));
    curium_string_append(cg->output, " ");

    /* Function name */
    {
        curium_string_t* mangled = curium_codegen_v2_mangle(
            fn->as.fn_decl.name ? fn->as.fn_decl.name->data : "");
        curium_string_append(cg->output, mangled->data);
        curium_string_free(mangled);
    }

    /* Parameters */
    curium_string_append(cg->output, "(");
    {
        curium_ast_v2_node_t* param = fn->as.fn_decl.params;
        while (param) {
            const char* param_type = curium_codegen_v2_type_to_c(param->as.param.type);
            if (!param->as.param.is_mutable) {
                curium_string_append(cg->output, "const ");
            }
            curium_string_append(cg->output, param_type);
            curium_string_append(cg->output, " ");
            {
                curium_string_t* pname = curium_codegen_v2_mangle(
                    param->as.param.name ? param->as.param.name->data : "");
                curium_string_append(cg->output, pname->data);
                curium_string_free(pname);
            }
            if (param->next) curium_string_append(cg->output, ", ");
            param = param->next;
        }
    }
    curium_string_append(cg->output, ")");
    curium_codegen_v2_newline(cg);

    /* Body */
    curium_codegen_v2_indent(cg);
    curium_string_append(cg->output, "{");
    curium_codegen_v2_newline(cg);

    cg->indent_level++;
    curium_string_set(cg->current_function,
        fn->as.fn_decl.name ? fn->as.fn_decl.name->data : "");

    {
        curium_ast_v2_node_t* s = fn->as.fn_decl.body;
        while (s) {
            curium_codegen_v2_generate_stmt(cg, s);
            s = s->next;
        }
    }

    cg->indent_level--;
    curium_codegen_v2_indent(cg);
    curium_string_append(cg->output, "}");
    curium_codegen_v2_newline(cg);
    curium_codegen_v2_newline(cg);
}

static void curium_codegen_v2_generate_struct(curium_codegen_v2_t* cg, curium_ast_v2_node_t* st) {
    curium_codegen_v2_indent(cg);
    curium_string_append(cg->output, "struct ");
    curium_string_append(cg->output,
        st->as.struct_decl.name ? st->as.struct_decl.name->data : "unknown_struct");
    curium_string_append(cg->output, " {");
    curium_codegen_v2_newline(cg);

    cg->indent_level++;
    {
        curium_ast_v2_node_t* f = st->as.struct_decl.fields;
        while (f) {
            curium_codegen_v2_indent(cg);
            curium_string_append(cg->output, curium_codegen_v2_type_to_c(f->as.param.type));
            curium_string_append(cg->output, " ");
            curium_string_append(cg->output,
                f->as.param.name ? f->as.param.name->data : "field");
            curium_string_append(cg->output, ";");
            curium_codegen_v2_newline(cg);
            f = f->next;
        }
    }
    cg->indent_level--;

    curium_codegen_v2_indent(cg);
    curium_string_append(cg->output, "};");
    curium_codegen_v2_newline(cg);
    curium_codegen_v2_newline(cg);
}

static void curium_codegen_v2_generate_enum(curium_codegen_v2_t* cg, curium_ast_v2_node_t* en) {
    const char* name = en->as.enum_decl.name ? en->as.enum_decl.name->data : "unknown_enum";
    
    /* 1. Emit the tag enum */
    curium_codegen_v2_indent(cg);
    curium_string_append(cg->output, "typedef enum {");
    curium_codegen_v2_newline(cg);
    cg->indent_level++;
    
    curium_ast_v2_node_t* v = en->as.enum_decl.fields;
    while (v) {
        curium_codegen_v2_indent(cg);
        curium_string_append(cg->output, "CURIUM_ENUM_");
        curium_string_append(cg->output, name);
        curium_string_append(cg->output, "_");
        curium_string_append(cg->output, v->as.enum_variant.name->data);
        if (v->next) curium_string_append(cg->output, ",");
        curium_codegen_v2_newline(cg);
        v = v->next;
    }
    
    cg->indent_level--;
    curium_codegen_v2_indent(cg);
    curium_string_append(cg->output, "} curium_enum_tag_");
    curium_string_append(cg->output, name);
    curium_string_append(cg->output, "_t;");
    curium_codegen_v2_newline(cg);
    curium_codegen_v2_newline(cg);
    
    /* 2. Emit the struct with union */
    curium_codegen_v2_indent(cg);
    curium_string_append(cg->output, "struct ");
    curium_string_append(cg->output, name);
    curium_string_append(cg->output, " {");
    curium_codegen_v2_newline(cg);
    cg->indent_level++;
    
    curium_codegen_v2_indent(cg);
    curium_string_append(cg->output, "curium_enum_tag_");
    curium_string_append(cg->output, name);
    curium_string_append(cg->output, "_t tag;");
    curium_codegen_v2_newline(cg);
    
    curium_codegen_v2_indent(cg);
    curium_string_append(cg->output, "union {");
    curium_codegen_v2_newline(cg);
    cg->indent_level++;
    
    v = en->as.enum_decl.fields;
    int has_associated_dt = 0;
    while (v) {
        if (v->as.enum_variant.associated_types) {
            has_associated_dt = 1;
            curium_codegen_v2_indent(cg);
            curium_string_append(cg->output, "struct { ");
            curium_ast_v2_node_t* at = v->as.enum_variant.associated_types;
            int idx = 0;
            while (at) {
                curium_string_append(cg->output, curium_codegen_v2_type_to_c(at));
                char buf[32];
                snprintf(buf, sizeof(buf), " _%d; ", idx++);
                curium_string_append(cg->output, buf);
                at = at->next;
            }
            curium_string_append(cg->output, "} ");
            curium_string_append(cg->output, v->as.enum_variant.name->data);
            curium_string_append(cg->output, ";");
            curium_codegen_v2_newline(cg);
        }
        v = v->next;
    }
    
    if (!has_associated_dt) {
        curium_codegen_v2_indent(cg);
        curium_string_append(cg->output, "char _empty;");
        curium_codegen_v2_newline(cg);
    }
    
    cg->indent_level--;
    curium_codegen_v2_indent(cg);
    curium_string_append(cg->output, "} as;");
    curium_codegen_v2_newline(cg);
    
    cg->indent_level--;
    curium_codegen_v2_indent(cg);
    curium_string_append(cg->output, "};");
    curium_codegen_v2_newline(cg);
    curium_codegen_v2_newline(cg);
}

static void curium_codegen_v2_generate_if(curium_codegen_v2_t* cg, curium_ast_v2_node_t* stmt) {
    curium_codegen_v2_indent(cg);
    curium_string_append(cg->output, "if (");
    curium_codegen_v2_generate_expr(cg, stmt->as.if_stmt.condition);
    curium_string_append(cg->output, ") {");
    curium_codegen_v2_newline(cg);

    cg->indent_level++;
    {
        curium_ast_v2_node_t* s = stmt->as.if_stmt.then_branch;
        while (s) { curium_codegen_v2_generate_stmt(cg, s); s = s->next; }
    }
    cg->indent_level--;

    curium_codegen_v2_indent(cg);
    curium_string_append(cg->output, "}");

    if (stmt->as.if_stmt.else_branch) {
        curium_string_append(cg->output, " else {");
        curium_codegen_v2_newline(cg);

        cg->indent_level++;
        {
            curium_ast_v2_node_t* s = stmt->as.if_stmt.else_branch;
            while (s) { curium_codegen_v2_generate_stmt(cg, s); s = s->next; }
        }
        cg->indent_level--;

        curium_codegen_v2_indent(cg);
        curium_string_append(cg->output, "}");
    }
    curium_codegen_v2_newline(cg);
}

static void curium_codegen_v2_generate_while(curium_codegen_v2_t* cg, curium_ast_v2_node_t* stmt) {
    curium_codegen_v2_indent(cg);
    curium_string_append(cg->output, "while (");
    curium_codegen_v2_generate_expr(cg, stmt->as.while_stmt.condition);
    curium_string_append(cg->output, ") {");
    curium_codegen_v2_newline(cg);

    cg->indent_level++;
    {
        curium_ast_v2_node_t* s = stmt->as.while_stmt.body;
        while (s) { curium_codegen_v2_generate_stmt(cg, s); s = s->next; }
    }
    cg->indent_level--;

    curium_codegen_v2_indent(cg);
    curium_string_append(cg->output, "}");
    curium_codegen_v2_newline(cg);
}

/* for i in collection { body }
 * FIX #007: (a) infer element type from iterable kind;
 *           (b) cache iterable expression in a temp var — FIX #017
 *               (iterable was evaluated TWICE: once for len(), once for element).
 * Emitted C:
 *   {   // for i in ...
 *       <iter_type>* _curium_iter_N = (collection);       <- evaluated ONCE
 *       int _curium_idx_N;
 *       for (_curium_idx_N = 0; _curium_idx_N < curium_len(_curium_iter_N); ...)
 *       {
 *           <elem_type> curium_i = _curium_iter_N[idx];   <- typed correctly
 *           <body>
 *       }
 *   }
 */
static void curium_codegen_v2_generate_for(curium_codegen_v2_t* cg, curium_ast_v2_node_t* stmt) {
    const char* var = stmt->as.for_stmt.var_name
                    ? stmt->as.for_stmt.var_name->data : "_i";
    int id = cg->temp_counter++;
    char idx_buf[64];  snprintf(idx_buf,  sizeof(idx_buf),  "_curium_idx_%d",  id);
    char iter_buf[64]; snprintf(iter_buf, sizeof(iter_buf), "_curium_iter_%d", id);

    /* Infer iterable type and element type from the AST node kind */
    curium_ast_v2_node_t* iterable  = stmt->as.for_stmt.iterable;
    int is_string_iter = (iterable && iterable->kind == CURIUM_AST_V2_STRING_LITERAL);
    const char* iter_c_type = is_string_iter ? "const curium_string_t*"
                                              : "curium_array_t*";
    const char* elem_c_type = is_string_iter ? "char" : "int";

    curium_codegen_v2_indent(cg);
    curium_string_append(cg->output, "{ /* for ");
    curium_string_append(cg->output, var);
    curium_string_append(cg->output, " in ... */");
    curium_codegen_v2_newline(cg);
    cg->indent_level++;

    /* FIX #017: cache iterable in a named temp — evaluated ONCE */
    curium_codegen_v2_indent(cg);
    curium_string_append(cg->output, iter_c_type);
    curium_string_append(cg->output, " ");
    curium_string_append(cg->output, iter_buf);
    curium_string_append(cg->output, " = (");
    curium_codegen_v2_generate_expr(cg, iterable);
    curium_string_append(cg->output, ");  /* iterable cached */");
    curium_codegen_v2_newline(cg);

    /* Emit loop header using cached iter */
    curium_codegen_v2_indent(cg);
    curium_string_append(cg->output, "int ");
    curium_string_append(cg->output, idx_buf);
    curium_string_append(cg->output, "; for (");
    curium_string_append(cg->output, idx_buf);
    curium_string_append(cg->output, " = 0; ");
    curium_string_append(cg->output, idx_buf);
    curium_string_append(cg->output, " < (int)curium_len(");
    curium_string_append(cg->output, iter_buf);
    curium_string_append(cg->output, "); ");
    curium_string_append(cg->output, idx_buf);
    curium_string_append(cg->output, "++) {");
    curium_codegen_v2_newline(cg);
    cg->indent_level++;

    /* FIX #007: element variable uses inferred type, not hardcoded 'int' */
    {
        curium_string_t* mangled = curium_codegen_v2_mangle(var);
        curium_codegen_v2_indent(cg);
        curium_string_append(cg->output, elem_c_type);
        curium_string_append(cg->output, " ");
        curium_string_append(cg->output, mangled->data);
        curium_string_append(cg->output, " = (");
        curium_string_append(cg->output, iter_buf);
        curium_string_append(cg->output, ")[");
        curium_string_append(cg->output, idx_buf);
        curium_string_append(cg->output, "];");
        curium_codegen_v2_newline(cg);
        curium_string_free(mangled);
    }

    {
        curium_ast_v2_node_t* s = stmt->as.for_stmt.body;
        while (s) { curium_codegen_v2_generate_stmt(cg, s); s = s->next; }
    }

    cg->indent_level--;
    curium_codegen_v2_indent(cg);
    curium_string_append(cg->output, "} /* end for loop */");
    curium_codegen_v2_newline(cg);
    cg->indent_level--;
    curium_codegen_v2_indent(cg);
    curium_string_append(cg->output, "}");
    curium_codegen_v2_newline(cg);
}

static void curium_codegen_v2_generate_match(curium_codegen_v2_t* cg, curium_ast_v2_node_t* stmt) {
    char tmp[32];
    int  tmp_id = cg->temp_counter++;
    snprintf(tmp, sizeof(tmp), "_curium_match_%d", tmp_id);

    /* FIX #011: detect string-based subjects by checking if the first arm
     * pattern is a STRING_LITERAL.  If so, emit strcmp comparisons instead
     * of == on a truncated (int) cast, which was corrupting pointer values. */
    int is_string_match = 0;
    {
        curium_ast_v2_node_t* first_arm = stmt->as.match_expr.arms;
        if (first_arm && first_arm->kind == CURIUM_AST_V2_MATCH_ARM &&
            first_arm->as.match_arm.pattern &&
            first_arm->as.match_arm.pattern->kind == CURIUM_AST_V2_STRING_LITERAL) {
            is_string_match = 1;
        }
    }

    curium_codegen_v2_indent(cg);
    curium_string_append(cg->output, "{ /* match */");
    curium_codegen_v2_newline(cg);
    cg->indent_level++;

    curium_codegen_v2_indent(cg);
    if (is_string_match) {
        /* String match: store subject as curium_string_t* for strcmp */
        curium_string_append(cg->output, "curium_string_t* ");
        curium_string_append(cg->output, tmp);
        curium_string_append(cg->output, " = (");
        curium_codegen_v2_generate_expr(cg, stmt->as.match_expr.expr);
        curium_string_append(cg->output, ");");
    } else {
        /* Integer/enum match: keep int cast (existing behaviour) */
        curium_string_append(cg->output, "int ");
        curium_string_append(cg->output, tmp);
        curium_string_append(cg->output, " = (int)(");
        curium_codegen_v2_generate_expr(cg, stmt->as.match_expr.expr);
        curium_string_append(cg->output, ");");
    }
    curium_codegen_v2_newline(cg);

    {
        curium_ast_v2_node_t* arm   = stmt->as.match_expr.arms;
        int               first = 1;

        while (arm) {
            if (arm->kind == CURIUM_AST_V2_MATCH_ARM) {
                int is_catch_all = 0;
                if (arm->as.match_arm.pattern->kind == CURIUM_AST_V2_IDENTIFIER &&
                    strcmp(arm->as.match_arm.pattern->as.identifier.value->data, "_") == 0) {
                    is_catch_all = 1;
                }

                curium_codegen_v2_indent(cg);
                if (is_catch_all) {
                    curium_string_append(cg->output, first ? "if (1) {" : "else {");
                } else if (is_string_match) {
                    /* FIX #011: string arm — emit strcmp instead of == */
                    curium_string_append(cg->output, first ? "if (" : "else if (");
                    curium_string_append(cg->output, "strcmp(");
                    curium_string_append(cg->output, tmp);
                    curium_string_append(cg->output, "->data, ");
                    curium_codegen_v2_generate_expr(cg, arm->as.match_arm.pattern);
                    curium_string_append(cg->output, "->data) == 0) {");
                } else {
                    curium_string_append(cg->output, first ? "if (" : "else if (");
                    curium_codegen_v2_generate_expr(cg, arm->as.match_arm.pattern);
                    curium_string_append(cg->output, " == ");
                    curium_string_append(cg->output, tmp);
                    curium_string_append(cg->output, ") {");
                }
                first = 0;
                curium_codegen_v2_newline(cg);

                cg->indent_level++;
                curium_codegen_v2_indent(cg);
                curium_codegen_v2_generate_expr(cg, arm->as.match_arm.expr);
                curium_string_append(cg->output, ";");
                curium_codegen_v2_newline(cg);
                cg->indent_level--;

                curium_codegen_v2_indent(cg);
                curium_string_append(cg->output, "}");
                curium_codegen_v2_newline(cg);
            }
            arm = arm->next;
        }
    }

    cg->indent_level--;
    curium_codegen_v2_indent(cg);
    curium_string_append(cg->output, "} /* end match */");
    curium_codegen_v2_newline(cg);
}

static void curium_codegen_v2_generate_assign(curium_codegen_v2_t* cg, curium_ast_v2_node_t* stmt) {
    /* If the LHS is an index expression, emit the bounds check first */
    if (stmt->as.assign_stmt.target &&
        stmt->as.assign_stmt.target->kind == CURIUM_AST_V2_INDEX) {
        curium_codegen_v2_generate_index_stmt(cg, stmt->as.assign_stmt.target);
    }

    curium_codegen_v2_indent(cg);
    curium_codegen_v2_generate_expr(cg, stmt->as.assign_stmt.target);
    curium_string_append(cg->output, " = ");
    curium_codegen_v2_generate_expr(cg, stmt->as.assign_stmt.value);
    curium_string_append(cg->output, ";");
    curium_codegen_v2_newline(cg);
}

static void curium_codegen_v2_generate_return(curium_codegen_v2_t* cg, curium_ast_v2_node_t* stmt) {
    curium_codegen_v2_indent(cg);
    curium_string_append(cg->output, "return");
    if (stmt->as.return_stmt.value) {
        curium_string_append(cg->output, " ");
        if (cg->in_dyn_body) {
            /* Wrap return value with curium_string_format for dyn functions */
            curium_string_append(cg->output, "curium_string_format(\"%g\", (double)(");
            curium_codegen_v2_generate_expr(cg, stmt->as.return_stmt.value);
            curium_string_append(cg->output, "))");
        } else {
            curium_codegen_v2_generate_expr(cg, stmt->as.return_stmt.value);
        }
    }
    curium_string_append(cg->output, ";");
    curium_codegen_v2_newline(cg);
}

/* Dynamic operator definition:
 * dyn action in ( "+" => { ... }, "avg" => { ... } ) dyn($) { fallback };
 * Emits a static C dispatch function. */
static void curium_codegen_v2_generate_dyn_op(curium_codegen_v2_t* cg, curium_ast_v2_node_t* stmt) {
    const char* name = stmt->as.dyn_op.name ? stmt->as.dyn_op.name->data : "unknown";

    /* Emit: static curium_string_t* _curium_dyn_<name>(curium_string_t* _left, curium_string_t* _op, curium_string_t* _right) { */
    curium_codegen_v2_indent(cg);
    curium_string_append(cg->output, "static inline curium_string_t* _curium_dyn_");
    curium_string_append(cg->output, name);
    curium_string_append(cg->output, "(curium_string_t* _left, curium_string_t* _op, curium_string_t* _right) {");
    curium_codegen_v2_newline(cg);
    cg->indent_level++;

    /* Local convenience variables: curium_x, curium_y match mangled names */
    curium_codegen_v2_indent(cg);
    curium_string_append(cg->output, "double curium_x = strtod(_left->data, NULL);");
    curium_codegen_v2_newline(cg);
    curium_codegen_v2_indent(cg);
    curium_string_append(cg->output, "double curium_y = strtod(_right->data, NULL);");
    curium_codegen_v2_newline(cg);

    /* Emit arms as if/else if chains */
    curium_ast_v2_node_t* arm = stmt->as.dyn_op.arms;
    int first = 1;
    while (arm) {
        if (arm->kind == CURIUM_AST_V2_MATCH_ARM && arm->as.match_arm.pattern) {
            curium_codegen_v2_indent(cg);
            curium_string_append(cg->output, first ? "if (strcmp(_op->data, " : "else if (strcmp(_op->data, ");
            first = 0;

            /* Emit pattern (string literal) */
            curium_string_append(cg->output, "\"");
            if (arm->as.match_arm.pattern->kind == CURIUM_AST_V2_STRING_LITERAL &&
                arm->as.match_arm.pattern->as.string_literal.value) {
                curium_string_append(cg->output, arm->as.match_arm.pattern->as.string_literal.value->data);
            }
            curium_string_append(cg->output, "\") == 0) {");
            curium_codegen_v2_newline(cg);

            cg->indent_level++;
            cg->in_dyn_body = 1;
            /* Emit body statements */
            curium_ast_v2_node_t* s = arm->as.match_arm.expr;
            while (s) {
                curium_codegen_v2_generate_stmt(cg, s);
                s = s->next;
            }
            cg->in_dyn_body = 0;
            cg->indent_level--;

            curium_codegen_v2_indent(cg);
            curium_string_append(cg->output, "}");
            curium_codegen_v2_newline(cg);
        }
        arm = arm->next;
    }

    /* Fallbacks */
    curium_ast_v2_node_t* fb = stmt->as.dyn_op.fallbacks;
    while (fb) {
        if (fb->kind == CURIUM_AST_V2_DYN_FALLBACK) {
            curium_codegen_v2_indent(cg);
            if (fb->as.dyn_fallback.cond) {
                curium_string_append(cg->output, first ? "if (" : "else if (");
                curium_codegen_v2_generate_expr(cg, fb->as.dyn_fallback.cond);
                curium_string_append(cg->output, ") { /* dynamic condition */");
                first = 0;
            } else {
                curium_string_append(cg->output, first ? "{ /* catch-all fallback */" : "else { /* catch-all fallback */");
                first = 0;
            }
            curium_codegen_v2_newline(cg);
            
            cg->indent_level++;
            cg->in_dyn_body = 1;
            curium_ast_v2_node_t* s = fb->as.dyn_fallback.body;
            while (s) {
                curium_codegen_v2_generate_stmt(cg, s);
                s = s->next;
            }
            cg->in_dyn_body = 0;
            cg->indent_level--;
            
            curium_codegen_v2_indent(cg);
            curium_string_append(cg->output, "}");
            curium_codegen_v2_newline(cg);
        }
        fb = fb->next;
    }

    if (first) {
        /* Default fallback: return "0" */
        curium_codegen_v2_indent(cg);
        curium_string_append(cg->output, "{ return curium_string_new(\"0\"); }");
        curium_codegen_v2_newline(cg);
    }

    /* Ensure the function always returns */
    curium_codegen_v2_indent(cg);
    curium_string_append(cg->output, "return curium_string_new(\"0\"); /* safety fallthrough */");
    curium_codegen_v2_newline(cg);

    cg->indent_level--;
    curium_codegen_v2_indent(cg);
    curium_string_append(cg->output, "}");
    curium_codegen_v2_newline(cg);

    /* v4.0: dyn.ops() introspection — emit static array of operation names */
    curium_codegen_v2_indent(cg);
    curium_string_append(cg->output, "static const char* _curium_dyn_ops_");
    curium_string_append(cg->output, name);
    curium_string_append(cg->output, "[] = {");
    {
        curium_ast_v2_node_t* a = stmt->as.dyn_op.arms;
        while (a) {
            if (a->kind == CURIUM_AST_V2_MATCH_ARM && a->as.match_arm.pattern &&
                a->as.match_arm.pattern->kind == CURIUM_AST_V2_STRING_LITERAL &&
                a->as.match_arm.pattern->as.string_literal.value) {
                curium_string_append(cg->output, "\"");
                curium_string_append(cg->output, a->as.match_arm.pattern->as.string_literal.value->data);
                curium_string_append(cg->output, "\", ");
            }
            a = a->next;
        }
    }
    curium_string_append(cg->output, "NULL};");
    curium_codegen_v2_newline(cg);

    /* Emit _curium_dyn_has_NAME(op) for runtime op checking */
    curium_codegen_v2_indent(cg);
    curium_string_append(cg->output, "static inline int _curium_dyn_has_");
    curium_string_append(cg->output, name);
    curium_string_append(cg->output, "(const char* _op) {");
    curium_codegen_v2_newline(cg);
    cg->indent_level++;
    curium_codegen_v2_indent(cg);
    curium_string_append(cg->output, "for (int _i = 0; _curium_dyn_ops_");
    curium_string_append(cg->output, name);
    curium_string_append(cg->output, "[_i]; _i++) {");
    curium_codegen_v2_newline(cg);
    cg->indent_level++;
    curium_codegen_v2_indent(cg);
    curium_string_append(cg->output, "if (strcmp(_curium_dyn_ops_");
    curium_string_append(cg->output, name);
    curium_string_append(cg->output, "[_i], _op) == 0) return 1;");
    curium_codegen_v2_newline(cg);
    cg->indent_level--;
    curium_codegen_v2_indent(cg);
    curium_string_append(cg->output, "}");
    curium_codegen_v2_newline(cg);
    curium_codegen_v2_indent(cg);
    curium_string_append(cg->output, "return 0;");
    curium_codegen_v2_newline(cg);
    cg->indent_level--;
    curium_codegen_v2_indent(cg);
    curium_string_append(cg->output, "}");
    curium_codegen_v2_newline(cg);
}

static void curium_codegen_v2_generate_try_catch(curium_codegen_v2_t* cg, curium_ast_v2_node_t* stmt) {
    curium_codegen_v2_indent(cg);
    curium_string_append(cg->output, "if (setjmp(_curium_ex_stack[_curium_ex_depth++]) == 0) {");
    curium_codegen_v2_newline(cg);

    cg->indent_level++;
    {
        curium_ast_v2_node_t* s = stmt->as.try_catch_stmt.try_block;
        while (s) { curium_codegen_v2_generate_stmt(cg, s); s = s->next; }
    }
    curium_codegen_v2_indent(cg);
    curium_string_append(cg->output, "_curium_ex_depth--;");
    curium_codegen_v2_newline(cg);
    cg->indent_level--;

    curium_codegen_v2_indent(cg);
    curium_string_append(cg->output, "}");

    if (stmt->as.try_catch_stmt.catch_block) {
        curium_string_append(cg->output, " else {");
        curium_codegen_v2_newline(cg);
        cg->indent_level++;

        curium_codegen_v2_indent(cg);
        curium_string_append(cg->output, "_curium_ex_depth--;");
        curium_codegen_v2_newline(cg);

        if (stmt->as.try_catch_stmt.catch_var) {
            curium_codegen_v2_indent(cg);
            /* Treat caught variable as a string pointer for simplicity in this dialect */
            curium_string_append(cg->output, "curium_string_t* ");
            {
                curium_string_t* mangled = curium_codegen_v2_mangle(stmt->as.try_catch_stmt.catch_var->data);
                curium_string_append(cg->output, mangled->data);
                curium_string_free(mangled);
            }
            curium_string_append(cg->output, " = (curium_string_t*)_curium_current_exception;");
            curium_codegen_v2_newline(cg);
        }

        {
            curium_ast_v2_node_t* s = stmt->as.try_catch_stmt.catch_block;
            while (s) { curium_codegen_v2_generate_stmt(cg, s); s = s->next; }
        }

        cg->indent_level--;
        curium_codegen_v2_indent(cg);
        curium_string_append(cg->output, "}");
    }
    curium_codegen_v2_newline(cg);
}

static void curium_codegen_v2_generate_throw(curium_codegen_v2_t* cg, curium_ast_v2_node_t* stmt) {
    curium_codegen_v2_indent(cg);
    curium_string_append(cg->output, "CURIUM_THROW_IMPL(");
    curium_codegen_v2_generate_expr(cg, stmt->as.throw_stmt.expr);
    curium_string_append(cg->output, ");");
    curium_codegen_v2_newline(cg);
}

static void curium_codegen_v2_generate_stmt(curium_codegen_v2_t* cg, curium_ast_v2_node_t* stmt) {
    if (!stmt) return;

    switch (stmt->kind) {
        case CURIUM_AST_V2_FN:     curium_codegen_v2_generate_fn(cg, stmt);          break;
        case CURIUM_AST_V2_LET:    curium_codegen_v2_generate_let_mut(cg, stmt, 0);  break;
        case CURIUM_AST_V2_MUT:    curium_codegen_v2_generate_let_mut(cg, stmt, 1);  break;
        case CURIUM_AST_V2_STRUCT: curium_codegen_v2_generate_struct(cg, stmt);      break;
        case CURIUM_AST_V2_ENUM:   curium_codegen_v2_generate_enum(cg, stmt);        break;
        case CURIUM_AST_V2_IF:     curium_codegen_v2_generate_if(cg, stmt);          break;
        case CURIUM_AST_V2_WHILE:  curium_codegen_v2_generate_while(cg, stmt);       break;
        case CURIUM_AST_V2_FOR:    curium_codegen_v2_generate_for(cg, stmt);         break;
        case CURIUM_AST_V2_MATCH:  curium_codegen_v2_generate_match(cg, stmt);       break;
        case CURIUM_AST_V2_ASSIGN: curium_codegen_v2_generate_assign(cg, stmt);      break;
        case CURIUM_AST_V2_RETURN: curium_codegen_v2_generate_return(cg, stmt);      break;
        case CURIUM_AST_V2_TRY_CATCH: curium_codegen_v2_generate_try_catch(cg, stmt); break;
        case CURIUM_AST_V2_THROW:  curium_codegen_v2_generate_throw(cg, stmt);       break;
        case CURIUM_AST_V2_DYN_OP:
            /* No-op here — dyn functions are pre-emitted at top level */
            break;

        /* v4.0: Reactor memory model blocks */
        case CURIUM_AST_V2_REACTOR: {
            const char* mode = stmt->as.reactor_stmt.mode ? stmt->as.reactor_stmt.mode->data : "rc";
            if (strcmp(mode, "arena") == 0) {
                curium_codegen_v2_indent(cg);
                curium_string_append(cg->output, "CURIUM_WITH_ARENA(\"");
                curium_string_append(cg->output, cg->current_function ? cg->current_function->data : "anon");
                curium_string_append(cg->output, "\", ");
                if (stmt->as.reactor_stmt.size_expr) {
                    curium_codegen_v2_generate_expr(cg, stmt->as.reactor_stmt.size_expr);
                } else {
                    curium_string_append(cg->output, "4096");
                }
                curium_string_append(cg->output, ") {");
                curium_codegen_v2_newline(cg);
                cg->indent_level++;
                {
                    curium_ast_v2_node_t* s = stmt->as.reactor_stmt.body;
                    while (s) { curium_codegen_v2_generate_stmt(cg, s); s = s->next; }
                }
                cg->indent_level--;
                curium_codegen_v2_indent(cg);
                curium_string_append(cg->output, "} /* end reactor arena */");
                curium_codegen_v2_newline(cg);
            } else if (strcmp(mode, "manual") == 0) {
                curium_codegen_v2_indent(cg);
                curium_string_append(cg->output, "{ /* reactor manual — no RC */");
                curium_codegen_v2_newline(cg);
                cg->indent_level++;
                {
                    curium_ast_v2_node_t* s = stmt->as.reactor_stmt.body;
                    while (s) { curium_codegen_v2_generate_stmt(cg, s); s = s->next; }
                }
                cg->indent_level--;
                curium_codegen_v2_indent(cg);
                curium_string_append(cg->output, "} /* end reactor manual */");
                curium_codegen_v2_newline(cg);
            } else {
                /* rc (default) — just emit the body normally */
                curium_ast_v2_node_t* s = stmt->as.reactor_stmt.body;
                while (s) { curium_codegen_v2_generate_stmt(cg, s); s = s->next; }
            }
            break;
        }
        case CURIUM_AST_V2_IMPL: {
            /* FIX #022: guard against NULL / non-named target type before deref */
            if (!stmt->as.impl_decl.target_type                                                ||
                stmt->as.impl_decl.target_type->kind != CURIUM_AST_V2_TYPE_NAMED               ||
                !stmt->as.impl_decl.target_type->as.type_named.name                            ||
                !stmt->as.impl_decl.target_type->as.type_named.name->data) {
                fprintf(stderr, "curium error: impl block with invalid target type — skipped in codegen\n");
                break;
            }
            curium_ast_v2_node_t* m = stmt->as.impl_decl.methods;
            while (m) {
                if (!m->as.fn_decl.name || !m->as.fn_decl.name->data) { m = m->next; continue; }
                curium_string_t* old_name = m->as.fn_decl.name;
                m->as.fn_decl.name = curium_string_format("%s_%s",
                    stmt->as.impl_decl.target_type->as.type_named.name->data, old_name->data);
                curium_codegen_v2_generate_fn(cg, m);
                curium_string_free(m->as.fn_decl.name);
                m->as.fn_decl.name = old_name;
                m = m->next;
            }
            break;
        }

        case CURIUM_AST_V2_TRAIT:
            /* Handled in the preamble pass (generating vtable structs) */
            break;

        case CURIUM_AST_V2_BREAK:
            curium_codegen_v2_indent(cg);
            curium_string_append(cg->output, "break;");
            curium_codegen_v2_newline(cg);
            break;

        case CURIUM_AST_V2_CONTINUE:
            curium_codegen_v2_indent(cg);
            curium_string_append(cg->output, "continue;");
            curium_codegen_v2_newline(cg);
            break;

        case CURIUM_AST_V2_EXPR_STMT:
            if (stmt->as.expr_stmt.expr &&
                stmt->as.expr_stmt.expr->kind == CURIUM_AST_V2_INDEX) {
                curium_codegen_v2_generate_index_stmt(cg, stmt->as.expr_stmt.expr);
            }
            curium_codegen_v2_indent(cg);
            curium_codegen_v2_generate_expr(cg, stmt->as.expr_stmt.expr);
            curium_string_append(cg->output, ";");
            curium_codegen_v2_newline(cg);
            break;

        case CURIUM_AST_V2_POLYGLOT:
            curium_codegen_v2_indent(cg);
            if (stmt->as.polyglot.code && stmt->as.polyglot.code->data) {
                curium_string_append(cg->output, stmt->as.polyglot.code->data);
            }
            curium_codegen_v2_newline(cg);
            break;

        default:
            curium_codegen_v2_indent(cg);
            curium_string_append(cg->output, "/* unknown statement */");
            curium_codegen_v2_newline(cg);
            break;
    }
}

/* ============================================================================
 * Public API: generate C99 code from a parsed AST
 * ==========================================================================*/

curium_string_t* curium_codegen_v2_to_c(const curium_ast_v2_list_t* ast) {
    curium_codegen_v2_t cg;
    curium_codegen_v2_init(&cg);

    /* Pre-pass: Collect OOP methods for unique method dispatch.
     * FIX #022: guard every deref of target_type / type_named.name.
     * FIX #003: strdup both strings so the table owns them independently
     *           of the AST's curium_string_t heap allocations. */
    for (curium_ast_v2_node_t* n = ast->head; n; n = n->next) {
        if (n->kind == CURIUM_AST_V2_IMPL) {
            /* #022: skip impl blocks with absent or non-named target types */
            if (!n->as.impl_decl.target_type                                           ||
                n->as.impl_decl.target_type->kind != CURIUM_AST_V2_TYPE_NAMED          ||
                !n->as.impl_decl.target_type->as.type_named.name                       ||
                !n->as.impl_decl.target_type->as.type_named.name->data) {
                fprintf(stderr, "curium warning: skipping impl block with non-named or null target type\n");
                continue;
            }
            const char* t_name = n->as.impl_decl.target_type->as.type_named.name->data;
            for (curium_ast_v2_node_t* m = n->as.impl_decl.methods; m; m = m->next) {
                if (!m->as.fn_decl.name || !m->as.fn_decl.name->data) continue;
                if (cg.method_count < 512) {
                    /* #003: strdup so stored pointers survive AST name-swaps & string reallocations */
                    cg.methods[cg.method_count].method_name = strdup(m->as.fn_decl.name->data);
                    cg.methods[cg.method_count].type_name   = strdup(t_name);
                    cg.method_count++;
                }
            }
        }
    }

    /* Preamble */
    curium_string_append(cg.includes, "/* Generated by Curium v5 compiler — do not edit */\n");
    curium_string_append(cg.includes, "#include \"curium/core.h\"\n");
    curium_string_append(cg.includes, "#include \"curium/memory.h\"\n");
    curium_string_append(cg.includes, "#include \"curium/string.h\"\n");
    curium_string_append(cg.includes, "#include \"curium/list.h\"\n");
    curium_string_append(cg.includes, "#include \"curium/array.h\"\n");
    curium_string_append(cg.includes, "#include \"curium/map.h\"\n");
    curium_string_append(cg.includes, "#include \"curium/safe_ptr.h\"\n");
    curium_string_append(cg.includes, "#include \"curium/option.h\"\n");
    curium_string_append(cg.includes, "#include \"curium/result.h\"\n");
    curium_string_append(cg.includes, "#include \"curium/http.h\"\n");
    curium_string_append(cg.includes, "#include \"curium/thread.h\"\n");
    curium_string_append(cg.includes, "#include \"curium/file.h\"\n");
    curium_string_append(cg.includes, "#include <stdio.h>\n");
    curium_string_append(cg.includes, "#include <stdlib.h>\n");
    curium_string_append(cg.includes, "#include <string.h>\n");
    curium_string_append(cg.includes, "#include <setjmp.h>\n");
    curium_string_append(cg.includes, "#include <stdint.h>\n");
    curium_string_append(cg.includes, "\n");

    /* Exception Runtime Preamble */
    curium_string_append(cg.includes,
        "/* Exception Handling Runtime */\n"
        "#define CURIUM_MAX_EX_DEPTH 64\n"
        "/* FIX #014: exception globals are _Thread_local so that spawn {} threads\n"
        " * have independent exception stacks — a throw on one thread no longer\n"
        " * corrupts another thread's setjmp/longjmp state.                      */\n"
        "#if defined(__TINYC__)\n"
        "#  define CURIUM_TLS  /* TCC: no TLS support */\n"
        "#elif defined(_MSC_VER)\n"
        "#  define CURIUM_TLS __declspec(thread)\n"
        "#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L\n"
        "#  define CURIUM_TLS _Thread_local\n"
        "#elif defined(__GNUC__) || defined(__clang__)\n"
        "#  define CURIUM_TLS __thread\n"
        "#else\n"
        "#  define CURIUM_TLS  /* no TLS support — fallback to global */\n"
        "#endif\n"
        "CURIUM_TLS jmp_buf _curium_ex_stack[CURIUM_MAX_EX_DEPTH];\n"
        "CURIUM_TLS int _curium_ex_depth = 0;\n"
        "CURIUM_TLS void* _curium_current_exception = NULL;\n\n"
        "#define CURIUM_THROW_IMPL(ex) do { \\\n"
        "    _curium_current_exception = (void*)(ex); \\\n"
        "    if (_curium_ex_depth > 0) longjmp(_curium_ex_stack[_curium_ex_depth - 1], 1); \\\n"
        "    else { fprintf(stderr, \"Uncaught exception\\n\"); exit(1); } \\\n"
        "} while(0)\n\n"
        "static inline void* curium_result_unwrap_or_throw(curium_result_t res) {\n"
        "    if (res.kind == CURIUM_RESULT_ERR) CURIUM_THROW_IMPL(curium_string_new(\"Result Error\"));\n"
        "    return res.value;\n"
        "}\n\n"
        /* FIX #002: Replaced GNU __typeof__ statement-expressions with C99 compound literals.
         * curium_result_ok / option_some accept a void* + size; we pass the address of a
         * compound literal so no need for a named temporary variable.          */
        "#define CURIUM_RESULT_OK(val)  curium_result_ok( &(val),  sizeof(val) )\n"
        "#define CURIUM_RESULT_ERR(err) curium_result_err(&(err),  sizeof(err) )\n"
        "#define CURIUM_OPTION_SOME(val) curium_option_some(&(val), sizeof(val))\n"
        "#define CURIUM_OPTION_NONE CURIUM_OPTION_NONE_INIT\n\n"
        "/* Type Erasure for Generics (Fallback to void*) */\n"
        "typedef void* T;\n"
        "typedef void* U;\n"
        "typedef void* K;\n"
        "typedef void* V;\n\n"
        /* FIX #002 / #008: curium_any_to_str — converts any Curium scalar to a C string
         * for safe use as a printf %s argument inside interpolated strings.
         * Uses C11 _Generic (supported by GCC ≥4.9 and all MinGW targets we ship).   */
        "#define curium_any_to_str(v) _Generic((v),                                   \\\n"
        "    curium_string_t*: ((curium_string_t*)(v))->data,                          \\\n"
        "    char*:            (v),                                                    \\\n"
        "    const char*:      (v),                                                    \\\n"
        "    int:              (snprintf(_curium_fmt_buf, sizeof(_curium_fmt_buf), \"%d\", (int)(v)),      _curium_fmt_buf), \\\n"
        "    unsigned int:     (snprintf(_curium_fmt_buf, sizeof(_curium_fmt_buf), \"%u\", (unsigned int)(v)), _curium_fmt_buf), \\\n"
        "    long:             (snprintf(_curium_fmt_buf, sizeof(_curium_fmt_buf), \"%ld\", (long)(v)),    _curium_fmt_buf), \\\n"
        "    unsigned long:    (snprintf(_curium_fmt_buf, sizeof(_curium_fmt_buf), \"%lu\", (unsigned long)(v)), _curium_fmt_buf), \\\n"
        "    double:           (snprintf(_curium_fmt_buf, sizeof(_curium_fmt_buf), \"%g\",  (double)(v)),   _curium_fmt_buf), \\\n"
        "    float:            (snprintf(_curium_fmt_buf, sizeof(_curium_fmt_buf), \"%g\",  (double)(v)),   _curium_fmt_buf), \\\n"
        "    default:          (snprintf(_curium_fmt_buf, sizeof(_curium_fmt_buf), \"%p\",  (void*)(v)),    _curium_fmt_buf)  \\\n"
        ")\n"
        "static char _curium_fmt_buf[64];\n\n"
    );

    /* Pre-pass: Emit DYN_OP functions at top level (must be outside other functions in C99) */
    for (curium_ast_v2_node_t* fn_node = ast->head; fn_node; fn_node = fn_node->next) {
        if (fn_node->kind == CURIUM_AST_V2_FN && fn_node->as.fn_decl.body) {
            curium_ast_v2_node_t* body_stmt = fn_node->as.fn_decl.body;
            while (body_stmt) {
                if (body_stmt->kind == CURIUM_AST_V2_DYN_OP) {
                    /* Borrow cg.output locally for generating the dyn_op, then restore */
                    curium_string_t* old = cg.output;
                    cg.output = cg.closures; 
                    curium_codegen_v2_generate_dyn_op(&cg, body_stmt);
                    curium_string_append(cg.output, "\n");
                    cg.output = old;
                }
                body_stmt = body_stmt->next;
            }
        }
    }

    /* Pre-pass: Emit forward declarations for structs and functions */
    curium_string_append(cg.forward_decls, "/* Forward Declarations */\n");
    for (curium_ast_v2_node_t* n = ast->head; n; n = n->next) {
        if (n->kind == CURIUM_AST_V2_STRUCT) {
            const char* struct_name = n->as.struct_decl.name ? n->as.struct_decl.name->data : "unknown";
            curium_string_append(cg.forward_decls, "typedef struct ");
            curium_string_append(cg.forward_decls, struct_name);
            curium_string_append(cg.forward_decls, " ");
            curium_string_append(cg.forward_decls, struct_name);
            curium_string_append(cg.forward_decls, ";\n");
        } else if (n->kind == CURIUM_AST_V2_TRAIT) {
            const char* trait_name = n->as.trait_decl.name ? n->as.trait_decl.name->data : "unknown";
            curium_string_append(cg.forward_decls, "typedef struct {\n");
            curium_string_append(cg.forward_decls, "    void* self_ptr;\n");
            for (curium_ast_v2_node_t* sig = n->as.trait_decl.signatures; sig; sig = sig->next) {
                const char* ret = curium_codegen_v2_type_to_c(sig->as.fn_decl.return_type);
                curium_string_t* mangled = curium_codegen_v2_mangle(sig->as.fn_decl.name->data);
                
                curium_string_append(cg.forward_decls, "    ");
                curium_string_append(cg.forward_decls, ret);
                curium_string_append(cg.forward_decls, " (*");
                curium_string_append(cg.forward_decls, mangled->data);
                curium_string_append(cg.forward_decls, ")(void*"); /* self_ptr */
                
                curium_ast_v2_node_t* p = sig->as.fn_decl.params;
                if (p) p = p->next; /* skip first param assuming it is self */
                while(p) {
                    curium_string_append(cg.forward_decls, ", ");
                    curium_string_append(cg.forward_decls, curium_codegen_v2_type_to_c(p->as.param.type));
                    p = p->next;
                }
                curium_string_append(cg.forward_decls, ");\n");
                curium_string_free(mangled);
            }
            curium_string_append(cg.forward_decls, "} curium_");
            curium_string_append(cg.forward_decls, trait_name);
            curium_string_append(cg.forward_decls, ";\n");
        }
    }
    curium_string_append(cg.forward_decls, "\n");
    
    for (curium_ast_v2_node_t* fn_node = ast->head; fn_node; fn_node = fn_node->next) {
        if (fn_node->kind == CURIUM_AST_V2_FN) {
            curium_string_t* mangled = curium_codegen_v2_mangle(fn_node->as.fn_decl.name->data);
            const char* ret_c = fn_node->as.fn_decl.return_type ? curium_codegen_v2_type_to_c(fn_node->as.fn_decl.return_type) : "void";
            
            /* Add static inline if not public and not main */
            if (!fn_node->as.fn_decl.is_public && strcmp(fn_node->as.fn_decl.name->data, "main") != 0 && strcmp(fn_node->as.fn_decl.name->data, "test_runner_main") != 0) {
                curium_string_append(cg.forward_decls, "static inline ");
            }
            
            curium_string_append(cg.forward_decls, ret_c);
            curium_string_append(cg.forward_decls, " ");
            curium_string_append(cg.forward_decls, mangled->data);
            curium_string_append(cg.forward_decls, "(");
            curium_ast_v2_node_t* p = fn_node->as.fn_decl.params;
            if (!p) curium_string_append(cg.forward_decls, "void");
            while (p) {
                const char* t_c = curium_codegen_v2_type_to_c(p->as.param.type);
                curium_string_append(cg.forward_decls, t_c);
                if (p->next) curium_string_append(cg.forward_decls, ", ");
                p = p->next;
            }
            curium_string_append(cg.forward_decls, ");\n");
            curium_string_free(mangled);
        } else if (fn_node->kind == CURIUM_AST_V2_IMPL) {
            /* FIX #022: guard before dereferencing target_type in forward-decl pass */
            if (!fn_node->as.impl_decl.target_type                                                 ||
                fn_node->as.impl_decl.target_type->kind != CURIUM_AST_V2_TYPE_NAMED                ||
                !fn_node->as.impl_decl.target_type->as.type_named.name                             ||
                !fn_node->as.impl_decl.target_type->as.type_named.name->data) {
                fprintf(stderr, "curium warning: skipping impl forward-decl — invalid target type\n");
                continue;
            }
            const char* t_name = fn_node->as.impl_decl.target_type->as.type_named.name->data;
            for (curium_ast_v2_node_t* m = fn_node->as.impl_decl.methods; m; m = m->next) {
                if (!m->as.fn_decl.name || !m->as.fn_decl.name->data) continue;
                curium_string_t* old_name = m->as.fn_decl.name;
                m->as.fn_decl.name = curium_string_format("%s_%s", t_name, old_name->data);
                
                curium_string_t* mangled = curium_codegen_v2_mangle(m->as.fn_decl.name->data);
                const char* ret_c = m->as.fn_decl.return_type ? curium_codegen_v2_type_to_c(m->as.fn_decl.return_type) : "void";
                
                if (!m->as.fn_decl.is_public) {
                    curium_string_append(cg.forward_decls, "static inline ");
                }
                
                curium_string_append(cg.forward_decls, ret_c);
                curium_string_append(cg.forward_decls, " ");
                curium_string_append(cg.forward_decls, mangled->data);
                curium_string_append(cg.forward_decls, "(");
                curium_ast_v2_node_t* p = m->as.fn_decl.params;
                if (!p) curium_string_append(cg.forward_decls, "void");
                while (p) {
                    const char* t_c = curium_codegen_v2_type_to_c(p->as.param.type);
                    curium_string_append(cg.forward_decls, t_c);
                    if (p->next) curium_string_append(cg.forward_decls, ", ");
                    p = p->next;
                }
                curium_string_append(cg.forward_decls, ");\n");
                curium_string_free(mangled);
                
                curium_string_free(m->as.fn_decl.name);
                m->as.fn_decl.name = old_name;
            }
        }
    }
    curium_string_append(cg.forward_decls, "\n");

    /* All top-level statements */
    {
        curium_ast_v2_node_t* s = ast->head;
        while (s) {
            curium_codegen_v2_generate_stmt(&cg, s);
            s = s->next;
        }
    }

    /* Boilerplate main() that calls user's curium_main() */
    curium_string_append(cg.output,
        "\nint main(int argc, char** argv) {\n"
        "    (void)argc; (void)argv;\n"
        "    curium_gc_init();\n"
        "    curium_main();\n");
    
    if (curium_opt_show_stat) {
        curium_string_append(cg.output, "    curium_gc_stats();\n");
    }
    
    curium_string_append(cg.output,
        "    curium_gc_shutdown();\n"
        "    return 0;\n"
        "}\n");

    /* Assemble Final Output! */
    curium_string_t* final_out = curium_string_new(cg.includes->data);
    curium_string_append(final_out, cg.forward_decls->data);
    curium_string_append(final_out, cg.closures->data);
    curium_string_append(final_out, cg.output->data);

    curium_codegen_v2_destroy(&cg);
    return final_out;
}
