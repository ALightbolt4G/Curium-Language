#include "curium/compiler/ast_v2.h"
#include "curium/compiler/arena.h"
#include "curium/memory.h"
#include "curium/error.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ============================================================================
 * Phase 2 DOD — Parse-session arena
 *
 * curium_parse_v2() sets this pointer to a heap-allocated arena before
 * parsing begins, then clears it to NULL and transfers ownership to the
 * returned curium_ast_v2_list_t.arena field.
 *
 * curium_ast_v2_new() allocates from this arena when it is non-NULL.
 * If NULL (e.g., type-checker builtins created outside a parse session),
 * the old curium_alloc() path is used — so every existing call site
 * continues to work with NO signature change.
 * ==========================================================================*/
curium_ast_arena_t* curium_parse_arena = NULL;

/* ============================================================================
 * CM v2 AST Implementation
 * ==========================================================================*/

curium_ast_v2_node_t* curium_ast_v2_new(curium_ast_v2_kind_t kind, size_t line, size_t col) {
    curium_ast_v2_node_t* node;

    if (curium_parse_arena) {
        /* DOD fast path: bump-allocate from the contiguous session arena.
         * curium_arena_alloc() zero-initialises the memory — no memset needed.
         * All sibling nodes from this parse live within ~2-4 x 64 KB blocks,
         * so codegen traversal fits on the L2 Shelf with minimal Fridge trips. */
        node = (curium_ast_v2_node_t*)curium_ast_arena_alloc(
                   curium_parse_arena,
                   sizeof(curium_ast_v2_node_t),
                   CURIUM_AST_ARENA_DEFAULT_ALIGN);
    } else {
        /* Legacy path: individual GC-managed allocation (type-checker builtins). */
        node = (curium_ast_v2_node_t*)curium_alloc(sizeof(curium_ast_v2_node_t), "ast_node");
        if (node) memset(node, 0, sizeof(*node));
    }

    if (!node) return NULL;
    node->kind   = kind;
    node->line   = line;
    node->column = col;
    return node;
}

void curium_ast_v2_list_append(curium_ast_v2_list_t* list, curium_ast_v2_node_t* n) {
    if (!list || !n) return;
    
    if (!list->head) {
        list->head = list->tail = n;
    } else {
        list->tail->next = n;
        list->tail = n;
    }
}

static void curium_ast_v2_free_node_strings(curium_ast_v2_node_t* node);

static void curium_ast_v2_free_node_recursive(curium_ast_v2_node_t* node) {
    if (!node) return;
    
    // Free children first
    switch (node->kind) {
        case CURIUM_AST_V2_FN:
            if (node->as.fn_decl.name) curium_string_free(node->as.fn_decl.name);
            curium_ast_v2_free_node_recursive(node->as.fn_decl.type_params);
            curium_ast_v2_free_node_recursive(node->as.fn_decl.params);
            curium_ast_v2_free_node_recursive(node->as.fn_decl.return_type);
            curium_ast_v2_free_node_recursive(node->as.fn_decl.body);
            curium_ast_v2_free_node_recursive(node->as.fn_decl.attributes);
            break;
            
        case CURIUM_AST_V2_LET:
            if (node->as.let_decl.name) curium_string_free(node->as.let_decl.name);
            curium_ast_v2_free_node_recursive(node->as.let_decl.type);
            curium_ast_v2_free_node_recursive(node->as.let_decl.init);
            break;
            
        case CURIUM_AST_V2_MUT:
            if (node->as.mut_decl.name) curium_string_free(node->as.mut_decl.name);
            curium_ast_v2_free_node_recursive(node->as.mut_decl.type);
            curium_ast_v2_free_node_recursive(node->as.mut_decl.init);
            break;
            
        case CURIUM_AST_V2_STRUCT:
        case CURIUM_AST_V2_UNION:
        case CURIUM_AST_V2_ENUM:
            if (node->as.struct_decl.name) curium_string_free(node->as.struct_decl.name);
            curium_ast_v2_free_node_recursive(node->as.struct_decl.type_params);
            curium_ast_v2_free_node_recursive(node->as.struct_decl.fields);
            curium_ast_v2_free_node_recursive(node->as.struct_decl.attributes);
            break;
            
        case CURIUM_AST_V2_ENUM_VARIANT:
            if (node->as.enum_variant.name) curium_string_free(node->as.enum_variant.name);
            curium_ast_v2_free_node_recursive(node->as.enum_variant.associated_types);
            break;
            
            
        case CURIUM_AST_V2_IMPL:
            curium_ast_v2_free_node_recursive(node->as.impl_decl.target_type);
            curium_ast_v2_free_node_recursive(node->as.impl_decl.methods);
            break;
            
        case CURIUM_AST_V2_TRAIT:
            if (node->as.trait_decl.name) curium_string_free(node->as.trait_decl.name);
            curium_ast_v2_free_node_recursive(node->as.trait_decl.signatures);
            break;
            
        case CURIUM_AST_V2_IMPORT:
            if (node->as.import_decl.path) curium_string_free(node->as.import_decl.path);
            break;
            
        case CURIUM_AST_V2_EXPR_STMT:
            curium_ast_v2_free_node_recursive(node->as.expr_stmt.expr);
            break;
            
        case CURIUM_AST_V2_ASSIGN:
            curium_ast_v2_free_node_recursive(node->as.assign_stmt.target);
            curium_ast_v2_free_node_recursive(node->as.assign_stmt.value);
            break;
            
        case CURIUM_AST_V2_RETURN:
            curium_ast_v2_free_node_recursive(node->as.return_stmt.value);
            break;
            
        case CURIUM_AST_V2_IF:
            curium_ast_v2_free_node_recursive(node->as.if_stmt.condition);
            curium_ast_v2_free_node_recursive(node->as.if_stmt.then_branch);
            curium_ast_v2_free_node_recursive(node->as.if_stmt.else_branch);
            break;
            
        case CURIUM_AST_V2_WHILE:
            curium_ast_v2_free_node_recursive(node->as.while_stmt.condition);
            curium_ast_v2_free_node_recursive(node->as.while_stmt.body);
            break;
        case CURIUM_AST_V2_SPAWN:
            curium_ast_v2_free_node_recursive(node->as.spawn_stmt.closure);
            break;

        /* v4.0: Reactor memory model */
        case CURIUM_AST_V2_REACTOR:
            if (node->as.reactor_stmt.mode) curium_string_free(node->as.reactor_stmt.mode);
            curium_ast_v2_free_node_recursive(node->as.reactor_stmt.size_expr);
            curium_ast_v2_free_node_recursive(node->as.reactor_stmt.body);
            break;
        case CURIUM_AST_V2_FOR:
            if (node->as.for_stmt.var_name) curium_string_free(node->as.for_stmt.var_name);
            curium_ast_v2_free_node_recursive(node->as.for_stmt.iterable);
            curium_ast_v2_free_node_recursive(node->as.for_stmt.body);
            break;
            
        case CURIUM_AST_V2_MATCH:
            curium_ast_v2_free_node_recursive(node->as.match_expr.expr);
            curium_ast_v2_free_node_recursive(node->as.match_expr.arms);
            break;
            
        case CURIUM_AST_V2_CALL:
            curium_ast_v2_free_node_recursive(node->as.call_expr.callee);
            curium_ast_v2_free_node_recursive(node->as.call_expr.type_args);
            curium_ast_v2_free_node_recursive(node->as.call_expr.args);
            break;
            
        case CURIUM_AST_V2_CLOSURE:
            curium_ast_v2_free_node_recursive(node->as.closure_expr.params);
            curium_ast_v2_free_node_recursive(node->as.closure_expr.return_type);
            curium_ast_v2_free_node_recursive(node->as.closure_expr.body);
            break;
            
        case CURIUM_AST_V2_FIELD_ACCESS:
            curium_ast_v2_free_node_recursive(node->as.field_access.object);
            if (node->as.field_access.field) curium_string_free(node->as.field_access.field);
            break;
            
        case CURIUM_AST_V2_INDEX:
            curium_ast_v2_free_node_recursive(node->as.index_expr.array);
            curium_ast_v2_free_node_recursive(node->as.index_expr.index);
            break;
            
        case CURIUM_AST_V2_DEREF:
        case CURIUM_AST_V2_ADDR_OF:
            curium_ast_v2_free_node_recursive(node->as.deref_expr.expr);
            break;
            
        case CURIUM_AST_V2_UNARY_OP:
            if (node->as.unary_expr.op) curium_string_free(node->as.unary_expr.op);
            curium_ast_v2_free_node_recursive(node->as.unary_expr.expr);
            break;
            
        case CURIUM_AST_V2_BINARY_OP:
            if (node->as.binary_expr.op) curium_string_free(node->as.binary_expr.op);
            curium_ast_v2_free_node_recursive(node->as.binary_expr.left);
            curium_ast_v2_free_node_recursive(node->as.binary_expr.right);
            break;

        case CURIUM_AST_V2_DYN_OP:
            if (node->as.dyn_op.name) curium_string_free(node->as.dyn_op.name);
            curium_ast_v2_free_node_recursive(node->as.dyn_op.arms);
            curium_ast_v2_free_node_recursive(node->as.dyn_op.fallbacks);
            break;

        case CURIUM_AST_V2_DYN_FALLBACK:
            curium_ast_v2_free_node_recursive(node->as.dyn_fallback.cond);
            curium_ast_v2_free_node_recursive(node->as.dyn_fallback.body);
            break;

        case CURIUM_AST_V2_DYN_CALL:
            if (node->as.dyn_call.op_name) curium_string_free(node->as.dyn_call.op_name);
            curium_ast_v2_free_node_recursive(node->as.dyn_call.left);
            curium_ast_v2_free_node_recursive(node->as.dyn_call.right);
            break;
            
        case CURIUM_AST_V2_TRY_CATCH:
            curium_ast_v2_free_node_recursive(node->as.try_catch_stmt.try_block);
            if (node->as.try_catch_stmt.catch_var) curium_string_free(node->as.try_catch_stmt.catch_var);
            curium_ast_v2_free_node_recursive(node->as.try_catch_stmt.catch_block);
            break;
            
        case CURIUM_AST_V2_TRY_EXPR:
            curium_ast_v2_free_node_recursive(node->as.try_expr.expr);
            break;
            
        case CURIUM_AST_V2_THROW:
            curium_ast_v2_free_node_recursive(node->as.throw_stmt.expr);
            break;
            
        case CURIUM_AST_V2_STRING_LITERAL:
        case CURIUM_AST_V2_IDENTIFIER:
            if (node->as.string_literal.value) curium_string_free(node->as.string_literal.value);
            break;
            
        case CURIUM_AST_V2_NUMBER:
            if (node->as.number_literal.value) curium_string_free(node->as.number_literal.value);
            break;
            
        case CURIUM_AST_V2_OPTION_SOME:
        case CURIUM_AST_V2_RESULT_OK:
        case CURIUM_AST_V2_RESULT_ERR:
            curium_ast_v2_free_node_recursive(node->as.option_some.value);
            break;

        case CURIUM_AST_V2_STRUCT_LITERAL:
            curium_ast_v2_free_node_recursive(node->as.struct_literal.type);
            curium_ast_v2_free_node_recursive(node->as.struct_literal.fields);
            break;
            
        case CURIUM_AST_V2_TYPE_NAMED:
            if (node->as.type_named.name) curium_string_free(node->as.type_named.name);
            curium_ast_v2_free_node_recursive(node->as.type_named.type_args);
            break;
            
        case CURIUM_AST_V2_TYPE_PTR:
        case CURIUM_AST_V2_TYPE_OPTION:
            curium_ast_v2_free_node_recursive(node->as.type_ptr.base);
            break;
            
        case CURIUM_AST_V2_TYPE_RESULT:
            curium_ast_v2_free_node_recursive(node->as.type_result.ok_type);
            curium_ast_v2_free_node_recursive(node->as.type_result.err_type);
            break;
            
        case CURIUM_AST_V2_TYPE_ARRAY:
        case CURIUM_AST_V2_TYPE_SLICE:
            curium_ast_v2_free_node_recursive(node->as.type_array.element_type);
            break;
            
        case CURIUM_AST_V2_TYPE_MAP:
            curium_ast_v2_free_node_recursive(node->as.type_map.key_type);
            curium_ast_v2_free_node_recursive(node->as.type_map.value_type);
            break;
            
        case CURIUM_AST_V2_TYPE_FN:
            curium_ast_v2_free_node_recursive(node->as.type_fn.params);
            curium_ast_v2_free_node_recursive(node->as.type_fn.return_type);
            break;
            
        case CURIUM_AST_V2_PARAM:
            if (node->as.param.name) curium_string_free(node->as.param.name);
            curium_ast_v2_free_node_recursive(node->as.param.type);
            break;
            
        case CURIUM_AST_V2_MATCH_ARM:
            curium_ast_v2_free_node_recursive(node->as.match_arm.pattern);
            curium_ast_v2_free_node_recursive(node->as.match_arm.expr);
            break;

        case CURIUM_AST_V2_DYN_CALL_ARM:
            curium_ast_v2_free_node_recursive(node->as.dyn_call_arm.pattern);
            if (node->as.dyn_call_arm.target_fn) curium_string_free(node->as.dyn_call_arm.target_fn);
            curium_ast_v2_free_node_recursive(node->as.dyn_call_arm.args);
            break;
            
        case CURIUM_AST_V2_ATTRIBUTE:
            if (node->as.attribute.name) curium_string_free(node->as.attribute.name);
            curium_ast_v2_free_node_recursive(node->as.attribute.args);
            break;
            
        case CURIUM_AST_V2_INTERPOLATED_STRING:
            if (node->as.interpolated_string.template) curium_string_free(node->as.interpolated_string.template);
            curium_ast_v2_free_node_recursive(node->as.interpolated_string.parts);
            break;
            
        case CURIUM_AST_V2_POLYGLOT:
            if (node->as.polyglot.code) curium_string_free(node->as.polyglot.code);
            break;
            
        case CURIUM_AST_V2_BOOL:
        case CURIUM_AST_V2_OPTION_NONE:
        case CURIUM_AST_V2_BREAK:
        case CURIUM_AST_V2_CONTINUE:
        case CURIUM_AST_V2_TYPE_STRNUM:
        case CURIUM_AST_V2_TYPE_DYN:
            // No additional cleanup needed
            break;
    }
    
    // Free the node itself
    curium_free(node);
}

/* ============================================================================
 * Phase 2 DOD: curium_ast_v2_free_node_strings
 *
 * Mirror of curium_ast_v2_free_node_recursive EXCEPT it does NOT call
 * curium_free(node) at the end.  Used by curium_ast_v2_free_list() when the
 * list is arena-owned: string members still need per-object free(), but the
 * node structs themselves are freed in bulk by curium_arena_destroy().
 * ==========================================================================*/
static void curium_ast_v2_free_node_strings(curium_ast_v2_node_t* node) {
    if (!node) return;
    switch (node->kind) {
        case CURIUM_AST_V2_FN:
            if (node->as.fn_decl.name) curium_string_free(node->as.fn_decl.name);
            break;
        case CURIUM_AST_V2_LET:
            if (node->as.let_decl.name) curium_string_free(node->as.let_decl.name);
            break;
        case CURIUM_AST_V2_MUT:
            if (node->as.mut_decl.name) curium_string_free(node->as.mut_decl.name);
            break;
        case CURIUM_AST_V2_STRUCT:
        case CURIUM_AST_V2_UNION:
        case CURIUM_AST_V2_ENUM:
            if (node->as.struct_decl.name) curium_string_free(node->as.struct_decl.name);
            break;
        case CURIUM_AST_V2_ENUM_VARIANT:
            if (node->as.enum_variant.name) curium_string_free(node->as.enum_variant.name);
            break;
        case CURIUM_AST_V2_TRAIT:
            if (node->as.trait_decl.name) curium_string_free(node->as.trait_decl.name);
            break;
        case CURIUM_AST_V2_IMPORT:
            if (node->as.import_decl.path) curium_string_free(node->as.import_decl.path);
            break;
        case CURIUM_AST_V2_REACTOR:
            if (node->as.reactor_stmt.mode) curium_string_free(node->as.reactor_stmt.mode);
            break;
        case CURIUM_AST_V2_FOR:
            if (node->as.for_stmt.var_name) curium_string_free(node->as.for_stmt.var_name);
            break;
        case CURIUM_AST_V2_UNARY_OP:
            if (node->as.unary_expr.op) curium_string_free(node->as.unary_expr.op);
            break;
        case CURIUM_AST_V2_BINARY_OP:
            if (node->as.binary_expr.op) curium_string_free(node->as.binary_expr.op);
            break;
        case CURIUM_AST_V2_DYN_OP:
            if (node->as.dyn_op.name) curium_string_free(node->as.dyn_op.name);
            break;
        case CURIUM_AST_V2_DYN_CALL:
            if (node->as.dyn_call.op_name) curium_string_free(node->as.dyn_call.op_name);
            break;
        case CURIUM_AST_V2_TRY_CATCH:
            if (node->as.try_catch_stmt.catch_var) curium_string_free(node->as.try_catch_stmt.catch_var);
            break;
        case CURIUM_AST_V2_FIELD_ACCESS:
            if (node->as.field_access.field) curium_string_free(node->as.field_access.field);
            break;
        case CURIUM_AST_V2_STRING_LITERAL:
        case CURIUM_AST_V2_IDENTIFIER:
            if (node->as.string_literal.value) curium_string_free(node->as.string_literal.value);
            break;
        case CURIUM_AST_V2_NUMBER:
            if (node->as.number_literal.value) curium_string_free(node->as.number_literal.value);
            break;
        case CURIUM_AST_V2_TYPE_NAMED:
            if (node->as.type_named.name) curium_string_free(node->as.type_named.name);
            break;
        case CURIUM_AST_V2_PARAM:
            if (node->as.param.name) curium_string_free(node->as.param.name);
            break;
        case CURIUM_AST_V2_DYN_CALL_ARM:
            if (node->as.dyn_call_arm.target_fn) curium_string_free(node->as.dyn_call_arm.target_fn);
            break;
        case CURIUM_AST_V2_ATTRIBUTE:
            if (node->as.attribute.name) curium_string_free(node->as.attribute.name);
            break;
        case CURIUM_AST_V2_INTERPOLATED_STRING:
            if (node->as.interpolated_string.template) curium_string_free(node->as.interpolated_string.template);
            break;
        case CURIUM_AST_V2_STRUCT_LITERAL:
            /* Strings inside are in the child identifiers/expressions */
            break;
        case CURIUM_AST_V2_POLYGLOT:
            if (node->as.polyglot.code) curium_string_free(node->as.polyglot.code);
            break;
        default:
            break; /* scalar nodes — no strings to free */
    }
    /* NOTE: node struct itself is NOT freed here — the arena owns it. */
}


void curium_ast_v2_free_node(curium_ast_v2_node_t* node) {
    curium_ast_v2_free_node_recursive(node);
}

void curium_ast_v2_free_list(curium_ast_v2_list_t* list) {
    if (!list) return;

    if (list->arena) {
        /* ── Phase 2 DOD fast path ──────────────────────────────────────────
         * The arena owns every node in this list.  Destroy it in O(blocks)
         * — typically 3-6 free() calls for a full program parse, versus
         * thousands of individual curium_free() calls in the legacy path.
         *
         * NOTE: curium_string_t members inside nodes are NOT owned by the
         * arena (they are allocated separately by curium_string_new).  The
         * per-node string cleanup below still runs for arena-owned nodes.
         * ──────────────────────────────────────────────────────────────── */
        curium_ast_v2_node_t* current = list->head;
        while (current) {
            curium_ast_v2_node_t* next = current->next;
            /* Free only the heap string members — not the node struct itself
             * (the arena owns that memory and frees it in bulk below). */
            curium_ast_v2_free_node_strings(current);
            current = next;
        }
        curium_ast_arena_destroy(list->arena);
        free(list->arena);             /* free the arena header itself */
        list->arena = NULL;
    } else {
        /* ── Legacy path ────────────────────────────────────────────────────
         * Nodes were individually malloc'd — walk and free each one. */
        curium_ast_v2_node_t* current = list->head;
        while (current) {
            curium_ast_v2_node_t* next = current->next;
            curium_ast_v2_free_node_recursive(current);
            current = next;
        }
    }

    list->head = list->tail = NULL;
}

const char* curium_ast_v2_kind_to_string(curium_ast_v2_kind_t kind) {
    switch (kind) {
        case CURIUM_AST_V2_FN: return "fn";
        case CURIUM_AST_V2_LET: return "let";
        case CURIUM_AST_V2_MUT: return "mut";
        case CURIUM_AST_V2_STRUCT: return "struct";
        case CURIUM_AST_V2_UNION: return "union";
        case CURIUM_AST_V2_ENUM: return "enum";
        case CURIUM_AST_V2_ENUM_VARIANT: return "enum_variant";
        case CURIUM_AST_V2_IMPL: return "impl";
        case CURIUM_AST_V2_TRAIT: return "trait";
        case CURIUM_AST_V2_IMPORT: return "import";
        case CURIUM_AST_V2_EXPR_STMT: return "expr_stmt";
        case CURIUM_AST_V2_ASSIGN: return "assign";
        case CURIUM_AST_V2_RETURN: return "return";
        case CURIUM_AST_V2_IF: return "if";
        case CURIUM_AST_V2_WHILE: return "while";
        case CURIUM_AST_V2_FOR: return "for";
        case CURIUM_AST_V2_MATCH: return "match";
        case CURIUM_AST_V2_BREAK: return "break";
        case CURIUM_AST_V2_CONTINUE: return "continue";
        case CURIUM_AST_V2_CALL: return "call";
        case CURIUM_AST_V2_FIELD_ACCESS: return "field_access";
        case CURIUM_AST_V2_INDEX: return "index";
        case CURIUM_AST_V2_DEREF: return "deref";
        case CURIUM_AST_V2_ADDR_OF: return "addr_of";
        case CURIUM_AST_V2_UNARY_OP: return "unary_op";
        case CURIUM_AST_V2_BINARY_OP: return "binary_op";
        case CURIUM_AST_V2_INTERPOLATED_STRING: return "interpolated_string";
        case CURIUM_AST_V2_STRING_LITERAL: return "string_literal";
        case CURIUM_AST_V2_NUMBER: return "number";
        case CURIUM_AST_V2_BOOL: return "bool";
        case CURIUM_AST_V2_IDENTIFIER: return "identifier";
        case CURIUM_AST_V2_OPTION_SOME: return "Some";
        case CURIUM_AST_V2_OPTION_NONE: return "None";
        case CURIUM_AST_V2_RESULT_OK: return "Ok";
        case CURIUM_AST_V2_RESULT_ERR: return "Err";
        case CURIUM_AST_V2_STRUCT_LITERAL: return "struct_literal";
        case CURIUM_AST_V2_TYPE_NAMED: return "type_named";
        case CURIUM_AST_V2_TYPE_STRNUM: return "type_strnum";
        case CURIUM_AST_V2_TYPE_DYN: return "type_dyn";
        case CURIUM_AST_V2_TYPE_PTR: return "type_ptr";
        case CURIUM_AST_V2_TYPE_OPTION: return "type_option";
        case CURIUM_AST_V2_TYPE_RESULT: return "type_result";
        case CURIUM_AST_V2_TYPE_ARRAY: return "type_array";
        case CURIUM_AST_V2_TYPE_SLICE: return "type_slice";
        case CURIUM_AST_V2_TYPE_MAP: return "type_map";
        case CURIUM_AST_V2_TYPE_FN: return "type_fn";
        case CURIUM_AST_V2_PARAM: return "param";
        case CURIUM_AST_V2_MATCH_ARM: return "match_arm";
        case CURIUM_AST_V2_DYN_CALL_ARM: return "dyn_call_arm";
        case CURIUM_AST_V2_ATTRIBUTE: return "attribute";
        case CURIUM_AST_V2_POLYGLOT: return "polyglot";
        case CURIUM_AST_V2_DYN_OP: return "dyn_op";
        case CURIUM_AST_V2_DYN_FALLBACK: return "dyn_fallback";
        case CURIUM_AST_V2_DYN_CALL: return "dyn_call";
        case CURIUM_AST_V2_TRY_CATCH: return "try_catch";
        case CURIUM_AST_V2_TRY_EXPR: return "try_expr";
        case CURIUM_AST_V2_THROW: return "throw";
        case CURIUM_AST_V2_SPAWN: return "spawn";
        case CURIUM_AST_V2_REACTOR: return "reactor";
        case CURIUM_AST_V2_CLOSURE: return "closure";
        default: return "unknown";
    }
}
