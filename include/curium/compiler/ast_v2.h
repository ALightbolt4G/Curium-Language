#ifndef CURIUM_AST_V2_H
#define CURIUM_AST_V2_H
#include "curium/compiler/tokens.h"
#include "curium/compiler/arena.h"  /* Phase 2 DOD: arena allocator */

/* CM v2 AST - Simplified structure for new syntax */

typedef enum {
    /* Declarations */
    CURIUM_AST_V2_FN,           // fn name() -> T { ... }
    CURIUM_AST_V2_LET,          // let name: T = expr
    CURIUM_AST_V2_MUT,          // mut name: T = expr
    CURIUM_AST_V2_STRUCT,       // struct Name { fields }
    CURIUM_AST_V2_UNION,        // union Name { variants }
    CURIUM_AST_V2_ENUM,         // enum Name { variants }
    CURIUM_AST_V2_ENUM_VARIANT, // Variant or Variant(types...)
    CURIUM_AST_V2_IMPL,         // impl Type { methods }
    CURIUM_AST_V2_TRAIT,        // trait Name { signatures }
    CURIUM_AST_V2_IMPORT,       // import "module"
    
    /* Statements */
    CURIUM_AST_V2_EXPR_STMT,    // expr;
    CURIUM_AST_V2_ASSIGN,       // name = expr;
    CURIUM_AST_V2_RETURN,       // return expr;
    CURIUM_AST_V2_IF,           // if cond { } else { }
    CURIUM_AST_V2_WHILE,        // while cond { }
    CURIUM_AST_V2_FOR,          // for i in 0..10 { }
    CURIUM_AST_V2_MATCH,        // match expr { arms }
    CURIUM_AST_V2_BREAK,        // break;
    CURIUM_AST_V2_CONTINUE,     // continue;
    CURIUM_AST_V2_DYN_OP,       // dyn name in ( arms ) dyn($) { fallback };
    CURIUM_AST_V2_DYN_FALLBACK, // dyn(cond) { body } or dyn($) { body }
    CURIUM_AST_V2_TRY_CATCH,    // try { } catch (e) { }
    CURIUM_AST_V2_THROW,        // throw expr;
    CURIUM_AST_V2_SPAWN,        // spawn { }
    CURIUM_AST_V2_REACTOR,      // reactor arena(size) { } (v4.0)
    
    /* Expressions */
    CURIUM_AST_V2_CALL,         // fn(args)
    CURIUM_AST_V2_FIELD_ACCESS, // obj.field
    CURIUM_AST_V2_INDEX,        // arr[index]  (bounds checked)
    CURIUM_AST_V2_DEREF,        // ptr^
    CURIUM_AST_V2_ADDR_OF,      // ^var
    CURIUM_AST_V2_UNARY_OP,     // -expr, !expr
    CURIUM_AST_V2_BINARY_OP,    // a + b, a && b
    CURIUM_AST_V2_DYN_CALL,     // x action y  (dynamic operator call)
    CURIUM_AST_V2_CLOSURE,      // |x, y| { x + y }
    CURIUM_AST_V2_TRY_EXPR,     // expr?       (early return error propagation)
    CURIUM_AST_V2_INTERPOLATED_STRING, // "hello {name}"
    CURIUM_AST_V2_STRING_LITERAL, // "hello"
    CURIUM_AST_V2_NUMBER,       // 42, 3.14
    CURIUM_AST_V2_BOOL,         // true, false
    CURIUM_AST_V2_IDENTIFIER,   // variable name
    CURIUM_AST_V2_OPTION_SOME,  // Some(value)
    CURIUM_AST_V2_OPTION_NONE,  // None
    CURIUM_AST_V2_RESULT_OK,    // Ok(value)
    CURIUM_AST_V2_RESULT_ERR,   // Err(error)
    CURIUM_AST_V2_STRUCT_LITERAL, // Type { field: val }
    
    /* Types */
    CURIUM_AST_V2_TYPE_NAMED,   // int, string, User
    CURIUM_AST_V2_TYPE_STRNUM,  // strnum primitive
    CURIUM_AST_V2_TYPE_DYN,     // dyn (dynamic type)
    CURIUM_AST_V2_TYPE_PTR,     // ^T (safe pointer)
    CURIUM_AST_V2_TYPE_OPTION,  // ?T
    CURIUM_AST_V2_TYPE_RESULT,  // Result<T, E>
    CURIUM_AST_V2_TYPE_ARRAY,   // array<T>
    CURIUM_AST_V2_TYPE_SLICE,   // slice<T>
    CURIUM_AST_V2_TYPE_MAP,     // map<K,V>
    CURIUM_AST_V2_TYPE_FN,      // fn(T) -> U
    
    /* Parameters */
    CURIUM_AST_V2_PARAM,        // name: type or mut name: type
    
    /* Match arms */
    CURIUM_AST_V2_MATCH_ARM,    // pattern => expr
    CURIUM_AST_V2_DYN_CALL_ARM, // pattern => call fn(args)
    
    /* Attributes */
    CURIUM_AST_V2_ATTRIBUTE,    // @route("/path")
    
    /* Legacy support */
    CURIUM_AST_V2_POLYGLOT,     // c{...} or cpp{...}
} curium_ast_v2_kind_t;

/* Forward declarations */
typedef struct curium_ast_v2_node curium_ast_v2_node_t;
typedef struct curium_ast_v2_list curium_ast_v2_list_t;

/* AST node structure */
struct curium_ast_v2_node {
    curium_ast_v2_kind_t kind;
    size_t line;
    size_t column;
    union {
        /* Function declaration */
        struct {
            curium_string_t* name;
            int is_public;
            int is_dynamic;
            curium_ast_v2_node_t* type_params;
            curium_ast_v2_node_t* params;
            curium_ast_v2_node_t* return_type;
            curium_ast_v2_node_t* body;
            curium_ast_v2_node_t* attributes;
        } fn_decl;
        
        /* Variable declarations */
        struct {
            curium_string_t* name;
            int is_public;
            int is_dynamic;
            /* v5.0 Phase 3: set when #[hot] precedes this declaration.
             * Codegen emits 'register' to hint the CPU to keep this variable
             * on the Cutting Board (registers) instead of the Fridge (RAM). */
            int is_hot;
            curium_ast_v2_node_t* type;
            curium_ast_v2_node_t* init;
        } let_decl, mut_decl;
        
        /* Struct/Union/Enum declaration */
        struct {
            curium_string_t* name;
            int is_public;
            curium_ast_v2_node_t* type_params;
            curium_ast_v2_node_t* fields;
            curium_ast_v2_node_t* attributes;
        } struct_decl, union_decl, enum_decl;
        
        /* Enum variant */
        struct {
            curium_string_t* name;
            curium_ast_v2_node_t* associated_types;
        } enum_variant;
        
        /* Implementation block */
        struct {
            curium_ast_v2_node_t* type_params;
            curium_ast_v2_node_t* trait_type; /* Optional: 'Printable' in 'impl Printable for User' */
            curium_ast_v2_node_t* target_type;
            curium_ast_v2_node_t* methods;
        } impl_decl;
        
        /* Trait block */
        struct {
            curium_string_t* name;
            int is_public;
            curium_ast_v2_node_t* signatures;
        } trait_decl;
        
        /* Import statement */
        struct {
            curium_string_t* path;
        } import_decl;
        
        /* Expression statement */
        struct {
            curium_ast_v2_node_t* expr;
        } expr_stmt;
        
        /* Assignment */
        struct {
            curium_ast_v2_node_t* target;
            curium_ast_v2_node_t* value;
        } assign_stmt;
        
        /* Return statement */
        struct {
            curium_ast_v2_node_t* value;
        } return_stmt;
        
        /* Spawn statement */
        struct {
            curium_ast_v2_node_t* closure;
        } spawn_stmt;
        
        /* v4.0: Reactor block */
        struct {
            curium_string_t* mode;           /* "arena" | "manual" | "rc" */
            curium_ast_v2_node_t* size_expr; /* arena size (NULL for rc/manual) */
            curium_ast_v2_node_t* body;      /* block body */
        } reactor_stmt;
        
        /* If statement */
        struct {
            curium_ast_v2_node_t* condition;
            curium_ast_v2_node_t* then_branch;
            curium_ast_v2_node_t* else_branch;
        } if_stmt;
        
        /* While loop */
        struct {
            curium_ast_v2_node_t* condition;
            curium_ast_v2_node_t* body;
        } while_stmt;
        
        /* For loop: for var_name in iterable { body } */
        struct {
            curium_string_t*      var_name;  /* loop variable name */
            curium_ast_v2_node_t* iterable; /* the collection/range to iterate */
            curium_ast_v2_node_t* body;
        } for_stmt;
        
        /* Match expression */
        struct {
            curium_ast_v2_node_t* expr;
            curium_ast_v2_node_t* arms;
        } match_expr;
        
        /* Match arm pattern => expr */
        struct {
            curium_ast_v2_node_t* pattern;
            curium_ast_v2_node_t* expr;
        } match_arm;

        /* Dyn call arm pattern => call fn(args) */
        struct {
            curium_ast_v2_node_t* pattern;
            curium_string_t* target_fn;
            curium_ast_v2_node_t* args;
        } dyn_call_arm;

        /* Function call */
        struct {
            curium_ast_v2_node_t* callee;
            curium_ast_v2_node_t* type_args;
            curium_ast_v2_node_t* args;
        } call_expr;
        
        /* Closure expression */
        struct {
            curium_ast_v2_node_t* params;
            curium_ast_v2_node_t* return_type; /* Optional */
            curium_ast_v2_node_t* body;
        } closure_expr;
        
        /* Field access and indexing */
        struct {
            curium_ast_v2_node_t* object;
            curium_string_t* field;
        } field_access;
        
        struct {
            curium_ast_v2_node_t* array;
            curium_ast_v2_node_t* index;
        } index_expr;
        
        /* Pointer operations */
        struct {
            curium_ast_v2_node_t* expr;
        } deref_expr, addr_of_expr;

        /* Struct literal: Type { field: val, ... } */
        struct {
            curium_ast_v2_node_t* type;
            curium_ast_v2_node_t* fields; /* linked list of CURIUM_AST_V2_ASSIGN */
        } struct_literal;
        
        /* Unary and binary operations */
        struct {
            curium_string_t* op;
            curium_ast_v2_node_t* expr;
        } unary_expr;
        
        struct {
            curium_string_t* op;
            curium_ast_v2_node_t* left;
            curium_ast_v2_node_t* right;
        } binary_expr;
        
        /* Dynamic operator definition */
        struct {
            curium_string_t* name;          /* operator variable name */
            curium_ast_v2_node_t* arms;     /* linked list of match_arm */
            curium_ast_v2_node_t* fallbacks; /* linked list of dyn_fallback */
        } dyn_op;
        
        /* Dynamic fallback */
        struct {
            curium_ast_v2_node_t* cond;     /* NULL if catch-all dyn($) */
            curium_ast_v2_node_t* body;
        } dyn_fallback;
        
        /* Dynamic operator call expression */
        struct {
            curium_string_t* op_name;       /* operator variable name */
            curium_ast_v2_node_t* left;     /* left operand */
            curium_ast_v2_node_t* right;    /* right operand */
        } dyn_call;
        
        /* Error handling */
        struct {
            curium_ast_v2_node_t* try_block;
            curium_string_t* catch_var;
            curium_ast_v2_node_t* catch_block;
        } try_catch_stmt;
        
        struct {
            curium_ast_v2_node_t* expr;
        } try_expr, throw_stmt;
        
        /* Literals */
        struct {
            curium_string_t* value;
        } string_literal, identifier;
        
        struct {
            curium_string_t* value;
            int is_float;
        } number_literal;
        
        struct {
            int value;
        } bool_literal;
        
        /* Option/Result constructors */
        struct {
            curium_ast_v2_node_t* value;
        } option_some, result_ok, result_err;
        
        /* Types */
        struct {
            curium_string_t* name;
            curium_ast_v2_node_t* type_args;
        } type_named;
        
        struct {
            curium_ast_v2_node_t* base;
        } type_ptr, type_option;
        
        struct {
            curium_ast_v2_node_t* ok_type;
            curium_ast_v2_node_t* err_type;
        } type_result;
        
        struct {
            curium_ast_v2_node_t* element_type;
        } type_array, type_slice;
        
        struct {
            curium_ast_v2_node_t* key_type;
            curium_ast_v2_node_t* value_type;
        } type_map;
        
        struct {
            curium_ast_v2_node_t* params;
            curium_ast_v2_node_t* return_type;
        } type_fn;
        
        /* Parameters */
        struct {
            curium_string_t* name;
            curium_ast_v2_node_t* type;
            int is_mutable;
        } param;
        
        
        /* Attributes */
        struct {
            curium_string_t* name;
            curium_ast_v2_node_t* args;
        } attribute;
        
        /* Interpolated strings */
        struct {
            curium_string_t* template;
            curium_ast_v2_node_t* parts;
        } interpolated_string;
        
        /* Polyglot blocks */
        struct {
            curium_string_t* code;
            int is_cpp;
        } polyglot;
    } as;
    curium_ast_v2_node_t* next;
};

/* AST list structure */
struct curium_ast_v2_list {
    curium_ast_v2_node_t* head;
    curium_ast_v2_node_t* tail;
    /* Phase 2 DOD: when non-NULL this arena *owns* every node in the list.
     * curium_ast_v2_free_list() destroys it in O(blocks) rather than O(nodes).
     * NULL means nodes were individually malloc'd (legacy path). */
    curium_ast_arena_t*       arena;
};

/* Function declarations */
curium_ast_v2_node_t* curium_ast_v2_new(curium_ast_v2_kind_t kind, size_t line, size_t col);
void curium_ast_v2_list_append(curium_ast_v2_list_t* list, curium_ast_v2_node_t* n);
void curium_ast_v2_free_list(curium_ast_v2_list_t* list);
void curium_ast_v2_free_node(curium_ast_v2_node_t* node);

/* Utility functions */
const char* curium_ast_v2_kind_to_string(curium_ast_v2_kind_t kind);

/* Phase 2 DOD: module-level parse-session arena.
 * Set to non-NULL by curium_parse_v2() before parsing begins;
 * cleared to NULL before returning. curium_ast_v2_new() allocates
 * from this arena when it is set, falling back to curium_alloc()
 * for any out-of-parse-context allocations (e.g. type-checker builtins).
 *
 * NOTE: type is curium_ast_arena_t (compiler-internal), NOT CuriumArena
 * (runtime Reactor allocator from memory.h). */
extern curium_ast_arena_t* curium_parse_arena;

#endif
