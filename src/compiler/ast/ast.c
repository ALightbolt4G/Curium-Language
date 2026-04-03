#include "curium/curium_lang.h"
#include "curium/memory.h"
#include <string.h>

/* ============================================================================
 * AST
 * ========================================================================== */

curium_ast_node_t* curium_ast_new(curium_ast_kind_t kind, size_t line, size_t col) {
    curium_ast_node_t* n = (curium_ast_node_t*)curium_alloc(sizeof(curium_ast_node_t), "curium_ast_node");
    if (!n) return NULL;
    memset(n, 0, sizeof(*n));
    n->kind   = kind;
    n->line   = line;
    n->column = col;
    return n;
}

void curium_ast_list_append(curium_ast_list_t* list, curium_ast_node_t* n) {
    if (!list || !n) return;
    if (!list->head) {
        list->head = list->tail = n;
    } else {
        list->tail->next = n;
        list->tail = n;
    }
}

void curium_ast_free_list(curium_ast_list_t* list) {
    if (!list) return;
    curium_ast_node_t* cur = list->head;
    while (cur) {
        curium_ast_node_t* next = cur->next;
        switch (cur->kind) {
            case CURIUM_AST_REQUIRE:
                if (cur->as.require_stmt.path) curium_string_free(cur->as.require_stmt.path);
                break;
            case CURIUM_AST_VAR_DECL:
                if (cur->as.var_decl.type_name) curium_string_free(cur->as.var_decl.type_name);
                if (cur->as.var_decl.var_name) curium_string_free(cur->as.var_decl.var_name);
                if (cur->as.var_decl.init_expr) curium_string_free(cur->as.var_decl.init_expr);
                break;
            case CURIUM_AST_EXPR_STMT:
                if (cur->as.expr_stmt.expr_text) curium_string_free(cur->as.expr_stmt.expr_text);
                break;
            case CURIUM_AST_POLYGLOT:
                if (cur->as.poly_block.code) curium_string_free(cur->as.poly_block.code);
                break;
            case CURIUM_AST_NAMESPACE:
            case CURIUM_AST_CLASS:
            case CURIUM_AST_METHOD:
            case CURIUM_AST_PROPERTY:
            case CURIUM_AST_IMPORT:
            case CURIUM_AST_TRY_CATCH:
            case CURIUM_AST_THROW:
            case CURIUM_AST_ASYNC_AWAIT:
            case CURIUM_AST_ATTRIBUTE:
            case CURIUM_AST_INTERFACE:
            case CURIUM_AST_IMPLEMENTS:
            case CURIUM_AST_GENERIC_TYPE:
                /* New node types - cleanup handled in curium_lang.c */
                break;
        }
        curium_free(cur);
        cur = next;
    }
    list->head = list->tail = NULL;
}

