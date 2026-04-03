#ifndef CURIUM_AST_H
#define CURIUM_AST_H
#include "curium/compiler/tokens.h"

typedef enum {
    CURIUM_AST_REQUIRE,
    CURIUM_AST_VAR_DECL,
    CURIUM_AST_EXPR_STMT,
    CURIUM_AST_POLYGLOT,
    CURIUM_AST_NAMESPACE,
    CURIUM_AST_CLASS,
    CURIUM_AST_METHOD,
    CURIUM_AST_PROPERTY,
    CURIUM_AST_IMPORT,
    CURIUM_AST_TRY_CATCH,
    CURIUM_AST_THROW,
    CURIUM_AST_ASYNC_AWAIT,
    CURIUM_AST_ATTRIBUTE,
    CURIUM_AST_INTERFACE,
    CURIUM_AST_IMPLEMENTS,
    CURIUM_AST_GENERIC_TYPE
} curium_ast_kind_t;

typedef enum { CURIUM_POLY_C, CURIUM_POLY_CPP } curium_poly_kind_t;

typedef struct curium_ast_node curium_ast_node_t;
struct curium_ast_node {
    curium_ast_kind_t kind;
    size_t line;
    size_t column;
    union {
        struct { curium_string_t* path; } require_stmt;
        struct { curium_string_t* type_name; curium_string_t* var_name; curium_string_t* init_expr; } var_decl;
        struct { curium_string_t* expr_text; } expr_stmt;
        struct { curium_poly_kind_t lang; curium_string_t* code; } poly_block;
        struct { curium_string_t* name; curium_ast_node_t* body; } namespace_decl;
        struct { 
            curium_string_t* name; 
            curium_string_t* parent_class;
            curium_ast_node_t* members;
            int is_public;
        } class_decl;
        struct {
            curium_string_t* name;
            curium_string_t* return_type;
            curium_ast_node_t* params;
            curium_ast_node_t* body;
            int is_public;
            int is_static;
            int is_async;
        } method_decl;
        struct {
            curium_string_t* name;
            curium_string_t* type;
            curium_ast_node_t* getter;
            curium_ast_node_t* setter;
            int is_public;
        } property_decl;
        struct { curium_string_t* module; curium_string_t* symbol; } import_stmt;
        struct {
            curium_ast_node_t* try_body;
            curium_ast_node_t* catch_body;
            curium_string_t* catch_var;
        } try_catch;
        struct { curium_string_t* expr; } throw_stmt;
        struct { curium_ast_node_t* expr; } async_await;
        struct { curium_string_t* name; curium_ast_node_t* args; } attribute_decl;
        struct { curium_string_t* name; curium_ast_node_t* methods; } interface_decl;
        struct { curium_string_t* base; curium_ast_node_t* interfaces; } implements_decl;
        struct { curium_string_t* name; curium_ast_node_t* type_params; } generic_type;
    } as;
    curium_ast_node_t* next;
};

typedef struct {
    curium_ast_node_t* head;
    curium_ast_node_t* tail;
} curium_ast_list_t;

/* Function declarations */
curium_ast_node_t* curium_ast_new(curium_ast_kind_t kind, size_t line, size_t col);
void curium_ast_list_append(curium_ast_list_t* list, curium_ast_node_t* n);
void curium_ast_free_list(curium_ast_list_t* list);

#endif
