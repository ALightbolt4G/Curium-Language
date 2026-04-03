#include "curium/compiler/ast_v2.h"
#include "curium/memory.h"
#include "curium/error.h"
#include "curium/string.h"
#include "curium/map.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

/* ============================================================================
 * CM v2 Type Checker
 * 
 * - Type inference for := operator
 * - Immutability enforcement (let vs mut)
 * - Mandatory error checking for ?T and Result<T,E>
 * - Function type checking
 * ==========================================================================*/

typedef struct curium_type_info {
    curium_ast_v2_node_t* type_node;    /* AST type node */
    int is_mutable;                  /* 1 if mutable, 0 if immutable */
    int is_initialized;              /* 1 if variable has been assigned */
    int is_error_type;               /* 1 if ?T or Result<T,E> */
    struct curium_type_info* next;
} curium_type_info_t;

typedef struct curium_scope {
    curium_map_t* bindings;             /* Variable name -> type_info */
    struct curium_scope* parent;         /* Enclosing scope */
} curium_scope_t;

typedef struct {
    curium_scope_t* current_scope;
    curium_ast_v2_node_t* current_function; /* For return type checking */
    const char* source_text;
    const char* file_path;
    int error_count;
    int warning_count;
} curium_typecheck_ctx_t;

static void curium_typecheck_report_error(curium_typecheck_ctx_t* ctx, size_t line, size_t col, const char* hint, const char* format, ...) {
    char msg[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(msg, sizeof(msg), format, args);
    va_end(args);

    if (ctx && ctx->source_text && ctx->file_path) {
        curium_error_report_caret(ctx->source_text, ctx->file_path, line, col, CURIUM_ERROR_TYPE, msg, hint ? hint : "Type mismatch or unknown type");
    }
    if (ctx) ctx->error_count++;
}


/* Forward declarations */
static int curium_typecheck_expr(curium_typecheck_ctx_t* ctx, curium_ast_v2_node_t* expr, curium_type_info_t** out_type);
static int curium_typecheck_stmt(curium_typecheck_ctx_t* ctx, curium_ast_v2_node_t* stmt);
static int curium_typecheck_type(curium_typecheck_ctx_t* ctx, curium_ast_v2_node_t* type);

/* Create new type info */
static curium_type_info_t* curium_type_info_new(curium_ast_v2_node_t* type_node, int is_mutable) {
    curium_type_info_t* info = (curium_type_info_t*)curium_alloc(sizeof(curium_type_info_t), "type_info");
    if (!info) return NULL;
    
    memset(info, 0, sizeof(*info));
    info->type_node = type_node;
    info->is_mutable = is_mutable;
    info->is_initialized = 1;
    
    /* Check if this is an error type */
    if (type_node) {
        if (type_node->kind == CURIUM_AST_V2_TYPE_OPTION ||
            type_node->kind == CURIUM_AST_V2_TYPE_RESULT) {
            info->is_error_type = 1;
        }
    }
    
    return info;
}

/* Copy type info */
static curium_type_info_t* curium_type_info_copy(curium_type_info_t* src) {
    if (!src) return NULL;
    curium_type_info_t* copy = curium_type_info_new(src->type_node, src->is_mutable);
    if (copy) {
        copy->is_initialized = src->is_initialized;
        copy->is_error_type = src->is_error_type;
    }
    return copy;
}

/* Free type info */
static void curium_type_info_free(curium_type_info_t* info) {
    curium_free(info);
}

/* Create new scope */
static curium_scope_t* curium_scope_new(curium_scope_t* parent) {
    curium_scope_t* scope = (curium_scope_t*)curium_alloc(sizeof(curium_scope_t), "scope");
    if (!scope) return NULL;
    
    memset(scope, 0, sizeof(*scope));
    scope->bindings = curium_map_new();
    scope->parent = parent;
    
    return scope;
}

/* Free scope */
static void curium_scope_free(curium_scope_t* scope) {
    if (!scope) return;
    
    /* Free all type info in bindings */
    if (scope->bindings) {
        curium_map_foreach(scope->bindings, key, val) {
            curium_type_info_t** p_info = (curium_type_info_t**)val;
            if (p_info && *p_info) {
                curium_type_info_free(*p_info);
            }
        }
        curium_map_free(scope->bindings);
    }
    
    curium_free(scope);
}

/* Look up variable in scope chain */
static curium_type_info_t* curium_scope_lookup(curium_scope_t* scope, const char* name) {
    while (scope) {
        curium_type_info_t** p_info = (curium_type_info_t**)curium_map_get(scope->bindings, name);
        if (p_info && *p_info) return *p_info;
        scope = scope->parent;
    }
    return NULL;
}

/* Add variable to current scope */
static int curium_scope_define(curium_typecheck_ctx_t* ctx, curium_scope_t* scope, const char* name, curium_type_info_t* info) {
    if (!scope || !name || !info) return 0;
    
    /* Check for redefinition in current scope */
    if (curium_map_get(scope->bindings, name)) {
        if (ctx) curium_typecheck_report_error(ctx, 0, 0, "Choose a different name", "Variable '%s' already defined in this scope", name);
        return 0;
    }
    
    curium_map_set(scope->bindings, name, &info, sizeof(curium_type_info_t*));
    return 1;
}

/* Push new scope */
static void curium_typecheck_push_scope(curium_typecheck_ctx_t* ctx) {
    ctx->current_scope = curium_scope_new(ctx->current_scope);
}

/* Pop scope */
static void curium_typecheck_pop_scope(curium_typecheck_ctx_t* ctx) {
    if (ctx->current_scope) {
        curium_scope_t* parent = ctx->current_scope->parent;
        curium_scope_free(ctx->current_scope);
        ctx->current_scope = parent;
    }
}

/* Type comparison - basic name matching for now */
static int curium_types_equal(curium_ast_v2_node_t* a, curium_ast_v2_node_t* b) {
    if (!a || !b) return 0;
    if (a->kind != b->kind) return 0;
    
    switch (a->kind) {
        case CURIUM_AST_V2_TYPE_NAMED:
            return strcmp(a->as.type_named.name->data, b->as.type_named.name->data) == 0;
        
        case CURIUM_AST_V2_TYPE_PTR:
        case CURIUM_AST_V2_TYPE_OPTION:
            return curium_types_equal(a->as.type_ptr.base, b->as.type_ptr.base);
        
        case CURIUM_AST_V2_TYPE_RESULT:
            return curium_types_equal(a->as.type_result.ok_type, b->as.type_result.ok_type) &&
                   curium_types_equal(a->as.type_result.err_type, b->as.type_result.err_type);
        
        default:
            return 1; /* Assume equal for complex types */
    }
}

/* Get type name for error messages */
static const char* curium_type_name(curium_ast_v2_node_t* type) {
    if (!type) return "<unknown>";
    
    switch (type->kind) {
        case CURIUM_AST_V2_TYPE_NAMED:
            return type->as.type_named.name->data;
        case CURIUM_AST_V2_TYPE_PTR:
            return "pointer";
        case CURIUM_AST_V2_TYPE_OPTION:
            return "option";
        case CURIUM_AST_V2_TYPE_RESULT:
            return "result";
        case CURIUM_AST_V2_TYPE_ARRAY:
            return "array";
        case CURIUM_AST_V2_TYPE_SLICE:
            return "slice";
        case CURIUM_AST_V2_TYPE_MAP:
            return "map";
        case CURIUM_AST_V2_TYPE_FN:
            return "function";
        default:
            return "<unknown>";
    }
}

/* Infer type from expression */
static curium_ast_v2_node_t* curium_infer_type(curium_typecheck_ctx_t* ctx, curium_ast_v2_node_t* expr) {
    if (!expr) return NULL;
    
    switch (expr->kind) {
        case CURIUM_AST_V2_NUMBER:
            /* Check if float */
            if (expr->as.number_literal.is_float) {
                curium_ast_v2_node_t* t = curium_ast_v2_new(CURIUM_AST_V2_TYPE_NAMED, expr->line, expr->column);
                t->as.type_named.name = curium_string_new("float");
                return t;
            } else {
                curium_ast_v2_node_t* t = curium_ast_v2_new(CURIUM_AST_V2_TYPE_NAMED, expr->line, expr->column);
                t->as.type_named.name = curium_string_new("int");
                return t;
            }
        
        case CURIUM_AST_V2_STRING_LITERAL:
        case CURIUM_AST_V2_INTERPOLATED_STRING: {
            curium_ast_v2_node_t* t = curium_ast_v2_new(CURIUM_AST_V2_TYPE_NAMED, expr->line, expr->column);
            t->as.type_named.name = curium_string_new("string");
            return t;
        }
        
        case CURIUM_AST_V2_BOOL: {
            curium_ast_v2_node_t* t = curium_ast_v2_new(CURIUM_AST_V2_TYPE_NAMED, expr->line, expr->column);
            t->as.type_named.name = curium_string_new("bool");
            return t;
        }
        
        case CURIUM_AST_V2_IDENTIFIER: {
            curium_type_info_t* info = curium_scope_lookup(ctx->current_scope, 
                expr->as.identifier.value->data);
            if (info) {
                return info->type_node;
            }
            return NULL;
        }
        
        case CURIUM_AST_V2_BINARY_OP: {
            /* Infer from operands */
            curium_ast_v2_node_t* left_type = curium_infer_type(ctx, expr->as.binary_expr.left);
            if (left_type) return left_type;
            return curium_infer_type(ctx, expr->as.binary_expr.right);
        }
        
        case CURIUM_AST_V2_CALL: {
            /* Return the return type of the function */
            if (expr->as.call_expr.callee && 
                expr->as.call_expr.callee->kind == CURIUM_AST_V2_IDENTIFIER) {
                curium_type_info_t* info = curium_scope_lookup(ctx->current_scope,
                    expr->as.call_expr.callee->as.identifier.value->data);
                if (info && info->type_node && info->type_node->kind == CURIUM_AST_V2_TYPE_FN) {
                    return info->type_node->as.type_fn.return_type;
                }
            }
            return NULL;
        }
        
        case CURIUM_AST_V2_OPTION_SOME: {
            curium_ast_v2_node_t* base = curium_infer_type(ctx, expr->as.option_some.value);
            if (base) {
                curium_ast_v2_node_t* opt = curium_ast_v2_new(CURIUM_AST_V2_TYPE_OPTION, expr->line, expr->column);
                opt->as.type_option.base = base;
                return opt;
            }
            return NULL;
        }
        
        case CURIUM_AST_V2_OPTION_NONE: {
            /* Cannot infer the inner type from None alone */
            curium_ast_v2_node_t* opt = curium_ast_v2_new(CURIUM_AST_V2_TYPE_OPTION, expr->line, expr->column);
            opt->as.type_option.base = curium_ast_v2_new(CURIUM_AST_V2_TYPE_NAMED, expr->line, expr->column);
            opt->as.type_option.base->as.type_named.name = curium_string_new("void");
            return opt;
        }
        
        default:
            return NULL;
    }
}

/* Check expression and return its type */
static int curium_typecheck_expr(curium_typecheck_ctx_t* ctx, curium_ast_v2_node_t* expr, curium_type_info_t** out_type) {
    if (!expr) {
        if (out_type) *out_type = NULL;
        return 1;
    }
    
    switch (expr->kind) {
        case CURIUM_AST_V2_NUMBER:
        case CURIUM_AST_V2_STRING_LITERAL:
        case CURIUM_AST_V2_BOOL:
            if (out_type) *out_type = curium_type_info_new(curium_infer_type(ctx, expr), 0);
            return 1;
        
        case CURIUM_AST_V2_IDENTIFIER: {
            const char* name = expr->as.identifier.value->data;
            curium_type_info_t* info = curium_scope_lookup(ctx->current_scope, name);
            if (!info) {
                curium_typecheck_report_error(ctx, expr->line, expr->column, "Check variable spelling", "Undefined variable: %s", name);
                return 0;
            }
            if (out_type) *out_type = curium_type_info_copy(info);
            return 1;
        }
        
        case CURIUM_AST_V2_BINARY_OP: {
            curium_type_info_t* left = NULL;
            curium_type_info_t* right = NULL;
            int ok = curium_typecheck_expr(ctx, expr->as.binary_expr.left, &left) &&
                     curium_typecheck_expr(ctx, expr->as.binary_expr.right, &right);
            
            if (ok && left && right && !curium_types_equal(left->type_node, right->type_node)) {
                /* Allow some implicit conversions? For now, strict */
                if (strcmp(expr->as.binary_expr.op->data, "==") != 0 &&
                    strcmp(expr->as.binary_expr.op->data, "!=") != 0) {
                    curium_typecheck_report_error(ctx, expr->line, expr->column, "Check operand types", "Type mismatch in binary expression: %s vs %s",
                        curium_type_name(left->type_node), curium_type_name(right->type_node));
                    ok = 0;
                }
            }
            
            if (out_type) *out_type = left ? curium_type_info_copy(left) : NULL;
            if (right) curium_type_info_free(right);
            return ok;
        }
        
        case CURIUM_AST_V2_UNARY_OP: {
            curium_type_info_t* inner = NULL;
            int ok = curium_typecheck_expr(ctx, expr->as.unary_expr.expr, &inner);
            
            if (ok && inner) {
                /* Check ! operator on error types - this handles the error propagation */
                if (strcmp(expr->as.unary_expr.op->data, "!") == 0) {
                    if (!inner->is_error_type) {
                        curium_error_set(CURIUM_ERROR_TYPE, 
                            "Error propagation operator (!) used on non-error type");
                        ctx->warning_count++;
                    }
                    /* The ! operator unwraps the error type */
                    if (inner->type_node && inner->type_node->kind == CURIUM_AST_V2_TYPE_OPTION) {
                        if (out_type) {
                            *out_type = curium_type_info_new(inner->type_node->as.type_option.base, inner->is_mutable);
                            (*out_type)->is_error_type = 0;
                        }
                    } else {
                        if (out_type) *out_type = inner;
                        inner = NULL;
                    }
                } else if (strcmp(expr->as.unary_expr.op->data, "^") == 0) {
                    /* Address-of creates a pointer type */
                    curium_ast_v2_node_t* ptr_type = curium_ast_v2_new(CURIUM_AST_V2_TYPE_PTR, 
                        expr->line, expr->column);
                    ptr_type->as.type_ptr.base = inner ? inner->type_node : NULL;
                    if (out_type) *out_type = curium_type_info_new(ptr_type, 0);
                } else {
                    if (out_type) *out_type = inner;
                    inner = NULL;
                }
            }
            
            if (inner) curium_type_info_free(inner);
            return ok;
        }
        
        case CURIUM_AST_V2_CALL: {
            /* Check callee and arguments */
            curium_type_info_t* callee_type = NULL;
            int ok = curium_typecheck_expr(ctx, expr->as.call_expr.callee, &callee_type);
            
            if (ok && callee_type && callee_type->type_node) {
                if (callee_type->type_node->kind != CURIUM_AST_V2_TYPE_FN && 
                    callee_type->type_node->kind != CURIUM_AST_V2_TYPE_NAMED) {
                    curium_typecheck_report_error(ctx, expr->line, expr->column, "Expression is not callable", "Cannot call non-function type: %s",
                        curium_type_name(callee_type->type_node));
                    ok = 0;
                }
            }

            /* Check arguments */
            curium_ast_v2_node_t* arg = expr->as.call_expr.args;
            while (arg && ok) {
                curium_type_info_t* arg_type = NULL;
                ok = curium_typecheck_expr(ctx, arg, &arg_type);
                if (arg_type) curium_type_info_free(arg_type);
                arg = arg->next;
            }
            
            /* Get return type */
            if (ok && callee_type && callee_type->type_node &&
                callee_type->type_node->kind == CURIUM_AST_V2_TYPE_FN) {
                if (out_type) {
                    *out_type = curium_type_info_new(
                        callee_type->type_node->as.type_fn.return_type, 0);
                }
            } else {
                if (out_type) *out_type = callee_type ? curium_type_info_copy(callee_type) : NULL;
                if (callee_type) curium_type_info_free(callee_type);
            }
            
            return ok;
        }
        
        case CURIUM_AST_V2_FIELD_ACCESS: {
            curium_type_info_t* obj_type = NULL;
            int ok = curium_typecheck_expr(ctx, expr->as.field_access.object, &obj_type);
            /* For now, we can't check field access without struct definitions */
            if (out_type) *out_type = obj_type;
            else if (obj_type) curium_type_info_free(obj_type);
            return ok;
        }
        
        case CURIUM_AST_V2_INDEX: {
            curium_type_info_t* arr_type = NULL;
            curium_type_info_t* idx_type = NULL;
            int ok = curium_typecheck_expr(ctx, expr->as.index_expr.array, &arr_type) &&
                     curium_typecheck_expr(ctx, expr->as.index_expr.index, &idx_type);
            
            if (ok && arr_type && arr_type->type_node && 
                arr_type->type_node->kind != CURIUM_AST_V2_TYPE_ARRAY &&
                arr_type->type_node->kind != CURIUM_AST_V2_TYPE_SLICE &&
                arr_type->type_node->kind != CURIUM_AST_V2_TYPE_MAP &&
                !(arr_type->type_node->kind == CURIUM_AST_V2_TYPE_NAMED && 
                  (strcmp(arr_type->type_node->as.type_named.name->data, "string") == 0 ||
                   strcmp(arr_type->type_node->as.type_named.name->data, "str") == 0))) {
                
                curium_typecheck_report_error(ctx, expr->line, expr->column, "Check collection type", "Cannot index type: %s", curium_type_name(arr_type->type_node));
                ok = 0;
            }
            if (ok && idx_type && idx_type->type_node && idx_type->type_node->kind == CURIUM_AST_V2_TYPE_NAMED) {
                const char* idx_name = idx_type->type_node->as.type_named.name->data;
                if (strcmp(idx_name, "int") != 0 && strcmp(idx_name, "uint") != 0) {
                    /* Only error if NOT indexing a map with a string */
                    if (!(arr_type && arr_type->type_node && arr_type->type_node->kind == CURIUM_AST_V2_TYPE_MAP && strcmp(idx_name, "string") == 0)) {
                        curium_typecheck_report_error(ctx, expr->as.index_expr.index->line, expr->as.index_expr.index->column, "Index must be an integer", "Array index must be int");
                        ok = 0;
                    }
                }
            }
            
            /* Return element type for array/slice types */
            if (ok && arr_type && arr_type->type_node) {
                if (arr_type->type_node->kind == CURIUM_AST_V2_TYPE_ARRAY ||
                    arr_type->type_node->kind == CURIUM_AST_V2_TYPE_SLICE) {
                    if (out_type) {
                        *out_type = curium_type_info_new(arr_type->type_node->as.type_array.element_type, 
                            arr_type->is_mutable);
                    }
                } else {
                    if (out_type) *out_type = curium_type_info_copy(arr_type);
                }
            }
            
            if (arr_type) curium_type_info_free(arr_type);
            if (idx_type) curium_type_info_free(idx_type);
            return ok;
        }
        
        case CURIUM_AST_V2_DEREF: {
            curium_type_info_t* ptr_type = NULL;
            int ok = curium_typecheck_expr(ctx, expr->as.deref_expr.expr, &ptr_type);
            
            if (ok && ptr_type && ptr_type->type_node &&
                ptr_type->type_node->kind != CURIUM_AST_V2_TYPE_PTR) {
                curium_typecheck_report_error(ctx, expr->line, expr->column, "Variable is not a pointer", "Cannot dereference non-pointer type");
                ok = 0;
            }
            
            if (ok && ptr_type && ptr_type->type_node &&
                ptr_type->type_node->kind == CURIUM_AST_V2_TYPE_PTR) {
                if (out_type) {
                    *out_type = curium_type_info_new(ptr_type->type_node->as.type_ptr.base,
                        ptr_type->is_mutable);
                }
            } else {
                if (out_type) *out_type = NULL;
            }
            
            if (ptr_type) curium_type_info_free(ptr_type);
            return ok;
        }
        
        case CURIUM_AST_V2_OPTION_SOME:
        case CURIUM_AST_V2_RESULT_OK:
        case CURIUM_AST_V2_RESULT_ERR: {
            curium_type_info_t* inner = NULL;
            int ok = curium_typecheck_expr(ctx, 
                expr->kind == CURIUM_AST_V2_OPTION_SOME ? expr->as.option_some.value :
                expr->kind == CURIUM_AST_V2_RESULT_OK ? expr->as.result_ok.value :
                expr->as.result_err.value, &inner);
            
            if (out_type) {
                curium_ast_v2_node_t* wrapper = curium_ast_v2_new(
                    expr->kind == CURIUM_AST_V2_OPTION_SOME ? CURIUM_AST_V2_TYPE_OPTION : CURIUM_AST_V2_TYPE_RESULT,
                    expr->line, expr->column);
                if (expr->kind == CURIUM_AST_V2_OPTION_SOME) {
                    wrapper->as.type_option.base = inner ? inner->type_node : NULL;
                } else {
                    wrapper->as.type_result.ok_type = inner ? inner->type_node : NULL;
                    wrapper->as.type_result.err_type = curium_ast_v2_new(CURIUM_AST_V2_TYPE_NAMED, 
                        expr->line, expr->column);
                    wrapper->as.type_result.err_type->as.type_named.name = curium_string_new("Error");
                }
                *out_type = curium_type_info_new(wrapper, 0);
                (*out_type)->is_error_type = 1;
            }
            
            if (inner) curium_type_info_free(inner);
            return ok;
        }
        
        case CURIUM_AST_V2_OPTION_NONE: {
            if (out_type) {
                curium_ast_v2_node_t* opt = curium_ast_v2_new(CURIUM_AST_V2_TYPE_OPTION, 
                    expr->line, expr->column);
                opt->as.type_option.base = curium_ast_v2_new(CURIUM_AST_V2_TYPE_NAMED,
                    expr->line, expr->column);
                opt->as.type_option.base->as.type_named.name = curium_string_new("void");
                *out_type = curium_type_info_new(opt, 0);
                (*out_type)->is_error_type = 1;
            }
            return 1;
        }
        
        default:
            if (out_type) *out_type = NULL;
            return 1;
    }
}

/* Check type node is valid */
static int curium_typecheck_type(curium_typecheck_ctx_t* ctx, curium_ast_v2_node_t* type) {
    if (!type) return 1;
    
    switch (type->kind) {
        case CURIUM_AST_V2_TYPE_PTR:
            return curium_typecheck_type(ctx, type->as.type_ptr.base);
        case CURIUM_AST_V2_TYPE_OPTION:
            return curium_typecheck_type(ctx, type->as.type_option.base);
        case CURIUM_AST_V2_TYPE_RESULT:
            return curium_typecheck_type(ctx, type->as.type_result.ok_type) &&
                   curium_typecheck_type(ctx, type->as.type_result.err_type);
        case CURIUM_AST_V2_TYPE_ARRAY:
        case CURIUM_AST_V2_TYPE_SLICE:
            return curium_typecheck_type(ctx, type->as.type_array.element_type);
        case CURIUM_AST_V2_TYPE_MAP:
            return curium_typecheck_type(ctx, type->as.type_map.key_type) &&
                   curium_typecheck_type(ctx, type->as.type_map.value_type);
        case CURIUM_AST_V2_TYPE_FN: {
            curium_ast_v2_node_t* param = type->as.type_fn.params;
            while (param) {
                if (param->kind == CURIUM_AST_V2_PARAM) {
                    if (!curium_typecheck_type(ctx, param->as.param.type)) return 0;
                }
                param = param->next;
            }
            return curium_typecheck_type(ctx, type->as.type_fn.return_type);
        }
        default:
            return 1;
    }
}

/* Check statement */
static int curium_typecheck_stmt(curium_typecheck_ctx_t* ctx, curium_ast_v2_node_t* stmt) {
    if (!stmt) return 1;
    
    switch (stmt->kind) {
        case CURIUM_AST_V2_FN: {
            /* Add function to scope */
            curium_ast_v2_node_t* fn_type = curium_ast_v2_new(CURIUM_AST_V2_TYPE_FN, stmt->line, stmt->column);
            fn_type->as.type_fn.params = stmt->as.fn_decl.params;
            /* Define function in current scope BEFORE checking body to allow recursion */
            curium_type_info_t* fn_info = curium_type_info_new(stmt->as.fn_decl.return_type, 0);
            curium_scope_define(ctx, ctx->current_scope, stmt->as.fn_decl.name->data, fn_info);
            
            /* Check return type */
            curium_typecheck_type(ctx, stmt->as.fn_decl.return_type);
            
            /* Push function scope */
            curium_typecheck_push_scope(ctx);
            
            /* Add parameters to scope */
            curium_ast_v2_node_t* param = stmt->as.fn_decl.params;
            while (param) {
                if (param->kind == CURIUM_AST_V2_PARAM) {
                    curium_typecheck_type(ctx, param->as.param.type);
                    curium_type_info_t* param_info = curium_type_info_new(param->as.param.type,
                        param->as.param.is_mutable);
                    curium_scope_define(ctx, ctx->current_scope, param->as.param.name->data, param_info);
                }
                param = param->next;
            }
            
            /* Set current function for return checking */
            curium_ast_v2_node_t* prev_fn = ctx->current_function;
            ctx->current_function = stmt;
            
            /* Check body */
            curium_ast_v2_node_t* body_stmt = stmt->as.fn_decl.body;
            while (body_stmt) {
                curium_typecheck_stmt(ctx, body_stmt);
                body_stmt = body_stmt->next;
            }
            
            ctx->current_function = prev_fn;
            curium_typecheck_pop_scope(ctx);
            return 1;
        }
        
        case CURIUM_AST_V2_LET:
        case CURIUM_AST_V2_MUT: {
            int is_mutable = (stmt->kind == CURIUM_AST_V2_MUT);
            const char* name = stmt->kind == CURIUM_AST_V2_LET ?
                stmt->as.let_decl.name->data : stmt->as.mut_decl.name->data;
            curium_ast_v2_node_t* declared_type = stmt->kind == CURIUM_AST_V2_LET ?
                stmt->as.let_decl.type : stmt->as.mut_decl.type;
            curium_ast_v2_node_t* init = stmt->kind == CURIUM_AST_V2_LET ?
                stmt->as.let_decl.init : stmt->as.mut_decl.init;
            
            /* Check init expression */
            curium_type_info_t* init_type = NULL;
            int ok = curium_typecheck_expr(ctx, init, &init_type);
            
            /* Infer or check type */
            curium_ast_v2_node_t* final_type = declared_type;
            if (!declared_type && init_type) {
                /* Type inference from init */
                final_type = init_type->type_node;
            } else if (declared_type && init_type && init_type->type_node) {
                /* Check type compatibility */
                if (!curium_types_equal(declared_type, init_type->type_node)) {
                    curium_typecheck_report_error(ctx, stmt->line, stmt->column, "Fix initialization type", "Type mismatch in initialization: expected %s, got %s",
                        is_mutable ? "mut" : "let",
                        curium_type_name(declared_type),
                        curium_type_name(init_type->type_node));
                    ok = 0;
                }
            }
            
            /* Add to scope */
            if (ok) {
                curium_type_info_t* var_info = curium_type_info_new(final_type, is_mutable);
                curium_scope_define(ctx, ctx->current_scope, name, var_info);
            }
            
            if (init_type) curium_type_info_free(init_type);
            return ok;
        }
        
        case CURIUM_AST_V2_ASSIGN: {
            curium_type_info_t* target_type = NULL;
            curium_type_info_t* value_type = NULL;
            
            int ok = curium_typecheck_expr(ctx, stmt->as.assign_stmt.target, &target_type) &&
                     curium_typecheck_expr(ctx, stmt->as.assign_stmt.value, &value_type);
            
            /* Check mutability */
            if (ok && target_type && !target_type->is_mutable) {
                curium_typecheck_report_error(ctx, stmt->as.assign_stmt.target->line, stmt->as.assign_stmt.target->column, "Variable is immutable", "Cannot assign to immutable variable");
                ok = 0;
            }
            
            /* Check type compatibility */
            if (ok && target_type && value_type && value_type->type_node &&
                !curium_types_equal(target_type->type_node, value_type->type_node)) {
                curium_typecheck_report_error(ctx, stmt->as.assign_stmt.value->line, stmt->as.assign_stmt.value->column, "Type mismatch", "Type mismatch in assignment: expected %s, got %s",
                    curium_type_name(target_type->type_node),
                    curium_type_name(value_type->type_node));
                ok = 0;
            }
            
            /* Check mandatory error handling */
            if (ok && value_type && value_type->is_error_type) {
                curium_typecheck_report_error(ctx, stmt->as.assign_stmt.value->line, stmt->as.assign_stmt.value->column, "Handle error value", 
                    "Error value must be handled with ! or match expression");
                ok = 0;
            }
            
            if (target_type) curium_type_info_free(target_type);
            if (value_type) curium_type_info_free(value_type);
            return ok;
        }
        
        case CURIUM_AST_V2_EXPR_STMT: {
            curium_type_info_t* expr_type = NULL;
            int ok = curium_typecheck_expr(ctx, stmt->as.expr_stmt.expr, &expr_type);
            
            /* Check mandatory error handling */
            if (ok && expr_type && expr_type->is_error_type) {
                /* Allow if the expression uses ! operator */
                curium_ast_v2_node_t* expr = stmt->as.expr_stmt.expr;
                if (!(expr->kind == CURIUM_AST_V2_UNARY_OP && 
                      strcmp(expr->as.unary_expr.op->data, "!") == 0)) {
                    curium_typecheck_report_error(ctx, stmt->as.expr_stmt.expr->line, stmt->as.expr_stmt.expr->column, "Handle error value",
                        "Error value must be handled with ! or match expression");
                    ok = 0;
                }
            }
            
            if (expr_type) curium_type_info_free(expr_type);
            return ok;
        }
        
        case CURIUM_AST_V2_RETURN: {
            curium_type_info_t* return_type = NULL;
            int ok = curium_typecheck_expr(ctx, stmt->as.return_stmt.value, &return_type);
            
            /* Check against function return type */
            if (ok && ctx->current_function && return_type) {
                curium_ast_v2_node_t* expected = ctx->current_function->as.fn_decl.return_type;
                if (expected && !curium_types_equal(expected, return_type->type_node)) {
                    curium_typecheck_report_error(ctx, stmt->as.return_stmt.value->line, stmt->as.return_stmt.value->column, "Return type mismatch", "Return type mismatch: expected %s, got %s",
                        curium_type_name(expected), curium_type_name(return_type->type_node));
                    ok = 0;
                }
            }
            
            if (return_type) curium_type_info_free(return_type);
            return ok;
        }
        
        case CURIUM_AST_V2_IF: {
            curium_type_info_t* cond_type = NULL;
            int ok = curium_typecheck_expr(ctx, stmt->as.if_stmt.condition, &cond_type);
            
            /* Condition should be bool */
            if (ok && cond_type && cond_type->type_node &&
                cond_type->type_node->kind == CURIUM_AST_V2_TYPE_NAMED &&
                strcmp(cond_type->type_node->as.type_named.name->data, "bool") != 0) {
                curium_error_set(CURIUM_ERROR_TYPE, "If condition must be bool");
                ctx->error_count++;
                ok = 0;
            }
            
            if (cond_type) curium_type_info_free(cond_type);
            
            /* Check branches */
            curium_typecheck_push_scope(ctx);
            curium_ast_v2_node_t* branch_stmt = stmt->as.if_stmt.then_branch;
            while (branch_stmt) {
                curium_typecheck_stmt(ctx, branch_stmt);
                branch_stmt = branch_stmt->next;
            }
            curium_typecheck_pop_scope(ctx);
            
            if (stmt->as.if_stmt.else_branch) {
                curium_typecheck_push_scope(ctx);
                branch_stmt = stmt->as.if_stmt.else_branch;
                while (branch_stmt) {
                    curium_typecheck_stmt(ctx, branch_stmt);
                    branch_stmt = branch_stmt->next;
                }
                curium_typecheck_pop_scope(ctx);
            }
            
            return ok;
        }
        
        case CURIUM_AST_V2_WHILE: {
            curium_type_info_t* cond_type = NULL;
            int ok = curium_typecheck_expr(ctx, stmt->as.while_stmt.condition, &cond_type);
            if (cond_type) curium_type_info_free(cond_type);
            curium_typecheck_push_scope(ctx);
            curium_ast_v2_node_t* body_stmt = stmt->as.while_stmt.body;
            while (body_stmt) {
                curium_typecheck_stmt(ctx, body_stmt);
                body_stmt = body_stmt->next;
            }
            curium_typecheck_pop_scope(ctx);
            return ok;
        }
        
        case CURIUM_AST_V2_FOR: {
            /* for x in iterable { body } — just walk the body in a new scope */
            curium_type_info_t* iter_type = NULL;
            curium_typecheck_expr(ctx, stmt->as.for_stmt.iterable, &iter_type);
            if (iter_type) curium_type_info_free(iter_type);
            curium_typecheck_push_scope(ctx);
            curium_ast_v2_node_t* body_stmt = stmt->as.for_stmt.body;
            while (body_stmt) {
                curium_typecheck_stmt(ctx, body_stmt);
                body_stmt = body_stmt->next;
            }
            curium_typecheck_pop_scope(ctx);
            return 1;
        }
        
        case CURIUM_AST_V2_MATCH: {
            curium_type_info_t* expr_type = NULL;
            int ok = curium_typecheck_expr(ctx, stmt->as.match_expr.expr, &expr_type);
            if (expr_type) curium_type_info_free(expr_type);
            
            /* Check match arms */
            curium_ast_v2_node_t* arm = stmt->as.match_expr.arms;
            while (arm && ok) {
                if (arm->kind == CURIUM_AST_V2_MATCH_ARM) {
                    curium_typecheck_push_scope(ctx);
                    ok = curium_typecheck_expr(ctx, arm->as.match_arm.expr, NULL);
                    curium_typecheck_pop_scope(ctx);
                }
                arm = arm->next;
            }
            
            return ok;
        }
        
        case CURIUM_AST_V2_STRUCT:
        case CURIUM_AST_V2_UNION:
            /* Check field types */
            return curium_typecheck_type(ctx, stmt->as.struct_decl.fields);
        
        case CURIUM_AST_V2_ENUM: {
            /* Register enum variants in scope as opaque named types */
            curium_ast_v2_node_t* variant = stmt->as.enum_decl.fields;
            while (variant) {
                if (variant->kind == CURIUM_AST_V2_ENUM_VARIANT && variant->as.enum_variant.name) {
                    curium_ast_v2_node_t* vtype = curium_ast_v2_new(CURIUM_AST_V2_TYPE_NAMED, variant->line, variant->column);
                    vtype->as.type_named.name = curium_string_new(stmt->as.enum_decl.name->data);
                    curium_type_info_t* vi = curium_type_info_new(vtype, 0);
                    curium_scope_define(ctx, ctx->current_scope, variant->as.enum_variant.name->data, vi);
                }
                variant = variant->next;
            }
            return 1;
        }
        
        case CURIUM_AST_V2_BREAK:
        case CURIUM_AST_V2_CONTINUE:
            return 1;
        
        default:
            return 1;
    }
}

/* Public API */

curium_typecheck_ctx_t* curium_typecheck_new(const char* source_text, const char* file_path) {
    curium_typecheck_ctx_t* ctx = (curium_typecheck_ctx_t*)curium_alloc(sizeof(curium_typecheck_ctx_t), "typecheck_ctx");
    if (!ctx) return NULL;
    
    memset(ctx, 0, sizeof(*ctx));
    ctx->current_scope = curium_scope_new(NULL);
    ctx->source_text = source_text ? source_text : "";
    ctx->file_path = file_path ? file_path : "unknown";
    
    return ctx;
}

void curium_typecheck_free(curium_typecheck_ctx_t* ctx) {
    if (!ctx) return;
    
    while (ctx->current_scope) {
        curium_scope_t* parent = ctx->current_scope->parent;
        curium_scope_free(ctx->current_scope);
        ctx->current_scope = parent;
    }
    
    curium_free(ctx);
}

int curium_typecheck_module(curium_typecheck_ctx_t* ctx, curium_ast_v2_list_t* ast) {
    if (!ctx || !ast) return 0;
    
    curium_ast_v2_node_t* stmt = ast->head;
    while (stmt) {
        curium_typecheck_stmt(ctx, stmt);
        stmt = stmt->next;
    }
    
    return ctx->error_count == 0;
}

int curium_typecheck_get_error_count(curium_typecheck_ctx_t* ctx) {
    return ctx ? ctx->error_count : 0;
}

int curium_typecheck_get_warning_count(curium_typecheck_ctx_t* ctx) {
    return ctx ? ctx->warning_count : 0;
}
