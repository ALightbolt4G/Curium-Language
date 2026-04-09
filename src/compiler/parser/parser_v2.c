#include "curium/compiler/ast_v2.h"
#include "curium/compiler/lexer.h"
#include "curium/memory.h"
#include "curium/error.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ============================================================================
 * CM v2 Parser - Simplified syntax
 *
 * Design goals (language philosophy):
 *   - Speed like native C:   emit direct C99 via codegen
 *   - Easy like Go:          fn, let, mut, for x in, match, no header files
 *   - Modern C ergonomics:   Result<T,E>, Option<T>, ^ references, := inference
 *   - Safe like Rust:        immutable-by-default (let), explicit mut, bounds checks
 * ==========================================================================*/

typedef struct {
    curium_lexer_t lexer;
    curium_token_t current;
    curium_token_t previous;
    int had_error;
} curium_parser_v2_t;

static void curium_parser_v2_init(curium_parser_v2_t* p, const char* src) {
    memset(p, 0, sizeof(*p));
    curium_lexer_v2_init(&p->lexer, src);
    p->current = curium_lexer_v2_next_token(&p->lexer);
}

static void curium_parser_v2_advance(curium_parser_v2_t* p) {
    curium_token_free(&p->previous);
    p->previous = p->current;
    p->current  = curium_lexer_v2_next_token(&p->lexer);
}

static void curium_parser_v2_destroy(curium_parser_v2_t* p) {
    if (!p) return;
    curium_token_free(&p->current);
    curium_token_free(&p->previous);
}

/* Returns 1 and advances if current token matches kind, else 0. */
static int curium_parser_v2_match(curium_parser_v2_t* p, curium_token_kind_t kind) {
    if (p->current.kind == kind) {
        curium_parser_v2_advance(p);
        return 1;
    }
    return 0;
}

static void curium_parser_v2_expect(curium_parser_v2_t* p, curium_token_kind_t kind, const char* what) {
    if (!curium_parser_v2_match(p, kind)) {
        curium_string_t* msg = curium_string_format("Expected %s but found '%s'",
            what,
            p->current.lexeme ? p->current.lexeme->data : "EOF");
        
        char hint[256];
        snprintf(hint, sizeof(hint), "Did you forget %s?", what);
        
        curium_error_report_caret(p->lexer.src, "source file", p->current.line, p->current.column, 
            CURIUM_ERROR_PARSE, msg ? msg->data : "parse error", hint);
            
        fprintf(stderr, "%s\n", curium_error_get_message());
        p->had_error = 1;
        
        if (msg) curium_string_free(msg);
        CURIUM_THROW(CURIUM_ERROR_PARSE, "parse error");
    }
}

static void curium_parser_v2_synchronize(curium_parser_v2_t* p) {
    p->had_error = 1;
    
    /* Consume the token that caused the panic to ensure progress */
    if (p->current.kind != CURIUM_TOK_EOF) {
        curium_parser_v2_advance(p);
    }
    
    while (p->current.kind != CURIUM_TOK_EOF) {
        if (p->previous.kind == CURIUM_TOK_SEMI) return;
        
        switch (p->current.kind) {
            case CURIUM_TOK_KW_STRUCT:
            case CURIUM_TOK_KW_UNION:
            case CURIUM_TOK_KW_ENUM:
            case CURIUM_TOK_KW_TRAIT:
            case CURIUM_TOK_KW_IMPL:
            case CURIUM_TOK_KW_FN:
            case CURIUM_TOK_KW_LET:
            case CURIUM_TOK_KW_MUT:
            case CURIUM_TOK_KW_IF:
            case CURIUM_TOK_KW_WHILE:
            case CURIUM_TOK_KW_FOR:
            case CURIUM_TOK_KW_RETURN:
            case CURIUM_TOK_KW_MATCH:
                return;
            default:
                break;
        }
        
        curium_parser_v2_advance(p);
    }
}

/* Forward declarations */
static curium_ast_v2_node_t* curium_parser_v2_parse_type(curium_parser_v2_t* p);
static curium_ast_v2_node_t* curium_parser_v2_parse_expr(curium_parser_v2_t* p);
static curium_ast_v2_node_t* curium_parser_v2_parse_stmt(curium_parser_v2_t* p);
static curium_ast_v2_node_t* curium_parser_v2_parse_if(curium_parser_v2_t* p);

/* ============================================================================
 * Generic Type Arguments and Parameters: <T, U>
 * ==========================================================================*/
static curium_ast_v2_node_t* curium_parser_v2_parse_type_args(curium_parser_v2_t* p) {
    if (!curium_parser_v2_match(p, CURIUM_TOK_LT)) return NULL;
    
    curium_ast_v2_node_t* args = NULL;
    curium_ast_v2_node_t* tail = NULL;
    
    if (p->current.kind != CURIUM_TOK_GT) {
        do {
            curium_ast_v2_node_t* arg = curium_parser_v2_parse_type(p);
            if (!args) { args = arg; tail = arg; }
            else { tail->next = arg; tail = arg; }
        } while (curium_parser_v2_match(p, CURIUM_TOK_COMMA));
    }
    
    curium_parser_v2_expect(p, CURIUM_TOK_GT, ">");
    return args;
}

static curium_ast_v2_node_t* curium_parser_v2_parse_type_params(curium_parser_v2_t* p) {
    if (!curium_parser_v2_match(p, CURIUM_TOK_LT)) return NULL;
    
    curium_ast_v2_node_t* params = NULL;
    curium_ast_v2_node_t* tail = NULL;
    
    if (p->current.kind != CURIUM_TOK_GT) {
        do {
            curium_parser_v2_expect(p, CURIUM_TOK_IDENTIFIER, "type parameter name");
            curium_ast_v2_node_t* param = curium_ast_v2_new(CURIUM_AST_V2_IDENTIFIER, p->previous.line, p->previous.column);
            param->as.identifier.value = curium_string_new(p->previous.lexeme ? p->previous.lexeme->data : "");
            
            if (!params) { params = param; tail = param; }
            else { tail->next = param; tail = param; }
        } while (curium_parser_v2_match(p, CURIUM_TOK_COMMA));
    }
    
    curium_parser_v2_expect(p, CURIUM_TOK_GT, ">");
    return params;
}

/* ============================================================================
 * Type parsing: int, string, ^T, ?T, array<T>, fn(T)->U, Result<T,E>
 * ==========================================================================*/
static curium_ast_v2_node_t* curium_parser_v2_parse_type(curium_parser_v2_t* p) {
    size_t line = p->current.line;
    size_t col  = p->current.column;

    /* Pointer types ^T (safe reference, like Rust &T) */
    if (curium_parser_v2_match(p, CURIUM_TOK_ADDR_OF)) {
        curium_ast_v2_node_t* base     = curium_parser_v2_parse_type(p);
        curium_ast_v2_node_t* ptr_type = curium_ast_v2_new(CURIUM_AST_V2_TYPE_PTR, line, col);
        ptr_type->as.type_ptr.base = base;
        return ptr_type;
    }

    /* Option types ?T (like Rust Option<T> but more ergonomic) */
    if (curium_parser_v2_match(p, CURIUM_TOK_QUESTION)) {
        curium_ast_v2_node_t* base        = curium_parser_v2_parse_type(p);
        curium_ast_v2_node_t* option_type = curium_ast_v2_new(CURIUM_AST_V2_TYPE_OPTION, line, col);
        option_type->as.type_option.base = base;
        return option_type;
    }

    /* Generic container types: array<T>, slice<T>, map<K,V> */
    if (p->current.kind == CURIUM_TOK_KW_ARRAY ||
        p->current.kind == CURIUM_TOK_KW_SLICE ||
        p->current.kind == CURIUM_TOK_KW_MAP) {

        curium_token_kind_t container_kind = p->current.kind;
        curium_parser_v2_advance(p);
        curium_parser_v2_expect(p, CURIUM_TOK_LT, "<");

        curium_ast_v2_node_t* type_node = NULL;
        if (container_kind == CURIUM_TOK_KW_ARRAY || container_kind == CURIUM_TOK_KW_SLICE) {
            curium_ast_v2_node_t* element_type = curium_parser_v2_parse_type(p);
            type_node = curium_ast_v2_new(
                container_kind == CURIUM_TOK_KW_ARRAY ? CURIUM_AST_V2_TYPE_ARRAY : CURIUM_AST_V2_TYPE_SLICE,
                line, col);
            type_node->as.type_array.element_type = element_type;
        } else { /* MAP */
            curium_ast_v2_node_t* key_type   = curium_parser_v2_parse_type(p);
            curium_parser_v2_expect(p, CURIUM_TOK_COMMA, ",");
            curium_ast_v2_node_t* value_type = curium_parser_v2_parse_type(p);
            type_node = curium_ast_v2_new(CURIUM_AST_V2_TYPE_MAP, line, col);
            type_node->as.type_map.key_type   = key_type;
            type_node->as.type_map.value_type = value_type;
        }

        curium_parser_v2_expect(p, CURIUM_TOK_GT, ">");
        return type_node;
    }

    /* Function types: fn(T) -> U */
    if (p->current.kind == CURIUM_TOK_KW_FN) {
        curium_parser_v2_advance(p);
        curium_ast_v2_node_t* fn_type = curium_ast_v2_new(CURIUM_AST_V2_TYPE_FN, line, col);
        curium_parser_v2_expect(p, CURIUM_TOK_LPAREN, "(");

        if (!curium_parser_v2_match(p, CURIUM_TOK_RPAREN)) {
            curium_ast_v2_node_t* first_param = curium_parser_v2_parse_type(p);
            /* Link up additional parameter types */
            curium_ast_v2_node_t* tail = first_param;
            while (curium_parser_v2_match(p, CURIUM_TOK_COMMA)) {
                curium_ast_v2_node_t* next_param = curium_parser_v2_parse_type(p);
                tail->next = next_param;
                tail       = next_param;
            }
            curium_parser_v2_expect(p, CURIUM_TOK_RPAREN, ")");
            fn_type->as.type_fn.params = first_param;
        }

        if (curium_parser_v2_match(p, CURIUM_TOK_ARROW)) {
            fn_type->as.type_fn.return_type = curium_parser_v2_parse_type(p);
        }
        return fn_type;
    }

    /* Result<T, E> — Rust-style error handling */
    if (p->current.kind == CURIUM_TOK_KW_RESULT) {
        curium_parser_v2_advance(p);
        curium_parser_v2_expect(p, CURIUM_TOK_LT, "<");
        curium_ast_v2_node_t* ok_type  = curium_parser_v2_parse_type(p);
        curium_parser_v2_expect(p, CURIUM_TOK_COMMA, ",");
        curium_ast_v2_node_t* err_type = curium_parser_v2_parse_type(p);
        curium_parser_v2_expect(p, CURIUM_TOK_GT, ">");
        curium_ast_v2_node_t* result_type = curium_ast_v2_new(CURIUM_AST_V2_TYPE_RESULT, line, col);
        result_type->as.type_result.ok_type  = ok_type;
        result_type->as.type_result.err_type = err_type;
        return result_type;
    }

    /* Named types: int, float, string, bool, void, or user-defined */
    if (p->current.kind == CURIUM_TOK_IDENTIFIER  ||
        p->current.kind == CURIUM_TOK_KW_INT    ||
        p->current.kind == CURIUM_TOK_KW_FLOAT  ||
        p->current.kind == CURIUM_TOK_KW_STRING ||
        p->current.kind == CURIUM_TOK_KW_STRNUM ||
        p->current.kind == CURIUM_TOK_KW_DYN    ||
        p->current.kind == CURIUM_TOK_KW_BOOL   ||
        p->current.kind == CURIUM_TOK_KW_VOID   ||
        /* v4.0: Sized numeric types */
        p->current.kind == CURIUM_TOK_KW_I8     ||
        p->current.kind == CURIUM_TOK_KW_I16    ||
        p->current.kind == CURIUM_TOK_KW_I32    ||
        p->current.kind == CURIUM_TOK_KW_I64    ||
        p->current.kind == CURIUM_TOK_KW_U8     ||
        p->current.kind == CURIUM_TOK_KW_U16    ||
        p->current.kind == CURIUM_TOK_KW_U32    ||
        p->current.kind == CURIUM_TOK_KW_U64    ||
        p->current.kind == CURIUM_TOK_KW_F32    ||
        p->current.kind == CURIUM_TOK_KW_F64    ||
        p->current.kind == CURIUM_TOK_KW_USIZE) {

        curium_ast_v2_node_t* named_type = NULL;
        if (p->current.kind == CURIUM_TOK_KW_STRNUM) {
            named_type = curium_ast_v2_new(CURIUM_AST_V2_TYPE_STRNUM, line, col);
        } else if (p->current.kind == CURIUM_TOK_KW_DYN) {
            named_type = curium_ast_v2_new(CURIUM_AST_V2_TYPE_DYN, line, col);
        } else {
            named_type = curium_ast_v2_new(CURIUM_AST_V2_TYPE_NAMED, line, col);
            named_type->as.type_named.name =
                curium_string_new(p->current.lexeme ? p->current.lexeme->data : "");
        }
        curium_parser_v2_advance(p);
        
        /* Parse generic type arguments if any: User<int> */
        named_type->as.type_named.type_args = curium_parser_v2_parse_type_args(p);
        
        return named_type;
    }

    curium_error_set(CURIUM_ERROR_PARSE, "Expected type");
    CURIUM_THROW(CURIUM_ERROR_PARSE, "Expected type");
    return NULL;
}

/* ============================================================================
 * Function parameter: [mut] name: type
 * ==========================================================================*/
static curium_ast_v2_node_t* curium_parser_v2_parse_param(curium_parser_v2_t* p) {
    size_t line       = p->current.line;
    size_t col        = p->current.column;
    int    is_mutable = curium_parser_v2_match(p, CURIUM_TOK_KW_MUT) ||
                        curium_parser_v2_match(p, CURIUM_TOK_KW_MUTABLE);

    curium_parser_v2_expect(p, CURIUM_TOK_IDENTIFIER, "parameter name");
    curium_string_t* name = curium_string_new(p->previous.lexeme ? p->previous.lexeme->data : "");

    curium_parser_v2_expect(p, CURIUM_TOK_COLON, ":");
    curium_ast_v2_node_t* type = curium_parser_v2_parse_type(p);

    curium_ast_v2_node_t* param = curium_ast_v2_new(CURIUM_AST_V2_PARAM, line, col);
    param->as.param.name       = name;
    param->as.param.type       = type;
    param->as.param.is_mutable = is_mutable;
    return param;
}

/* ============================================================================
 * Function declaration: fn name(params) -> RetType { body }
 * NOTE: the caller must NOT pre-advance past 'fn'. This function consumes it.
 * ==========================================================================*/
static curium_ast_v2_node_t* curium_parser_v2_parse_fn(curium_parser_v2_t* p) {
    size_t line = p->current.line;
    size_t col  = p->current.column;

    curium_parser_v2_expect(p, CURIUM_TOK_KW_FN, "fn"); /* BUG FIX: parse_stmt must NOT pre-advance */

    curium_parser_v2_expect(p, CURIUM_TOK_IDENTIFIER, "function name");
    curium_string_t* name = curium_string_new(p->previous.lexeme ? p->previous.lexeme->data : "");

    /* Parse generic type parameters: fn max<T>(... */
    curium_ast_v2_node_t* type_params = curium_parser_v2_parse_type_params(p);

    curium_parser_v2_expect(p, CURIUM_TOK_LPAREN, "(");

    /* Parse all parameters and link them — BUG FIX: was only keeping the first */
    curium_ast_v2_node_t* params      = NULL;
    curium_ast_v2_node_t* params_tail = NULL;
    if (!curium_parser_v2_match(p, CURIUM_TOK_RPAREN)) {
        params = curium_parser_v2_parse_param(p);
        params_tail = params;
        while (curium_parser_v2_match(p, CURIUM_TOK_COMMA)) {
            curium_ast_v2_node_t* next_param = curium_parser_v2_parse_param(p);
            params_tail->next = next_param;
            params_tail       = next_param;
        }
        curium_parser_v2_expect(p, CURIUM_TOK_RPAREN, ")");
    }

    /* Return type (defaults to void like Go) */
    curium_ast_v2_node_t* return_type = NULL;
    if (curium_parser_v2_match(p, CURIUM_TOK_ARROW)) {
        return_type = curium_parser_v2_parse_type(p);
    } else {
        return_type = curium_ast_v2_new(CURIUM_AST_V2_TYPE_NAMED, line, col);
        return_type->as.type_named.name = curium_string_new("void");
    }

    curium_parser_v2_expect(p, CURIUM_TOK_LBRACE, "{");

    /* Parse all body statements and link them — BUG FIX: was only keeping the first */
    curium_ast_v2_node_t* body      = NULL;
    curium_ast_v2_node_t* body_tail = NULL;
    if (!curium_parser_v2_match(p, CURIUM_TOK_RBRACE)) {
        body = curium_parser_v2_parse_stmt(p);
        body_tail = body;
        while (p->current.kind != CURIUM_TOK_RBRACE && p->current.kind != CURIUM_TOK_EOF) {
            curium_ast_v2_node_t* next_stmt = curium_parser_v2_parse_stmt(p);
            if (next_stmt) {
                body_tail->next = next_stmt;
                body_tail       = next_stmt;
            }
        }
        curium_parser_v2_expect(p, CURIUM_TOK_RBRACE, "}");
    }

    curium_ast_v2_node_t* fn_node = curium_ast_v2_new(CURIUM_AST_V2_FN, line, col);
    fn_node->as.fn_decl.name        = name;
    fn_node->as.fn_decl.type_params = type_params;
    fn_node->as.fn_decl.params      = params;
    fn_node->as.fn_decl.return_type = return_type;
    fn_node->as.fn_decl.body        = body;
    fn_node->as.fn_decl.attributes  = NULL;
    return fn_node;
}

/* ============================================================================
 * Variable declarations: let name[: type] = expr;   (immutable, like Rust let)
 *                        mut name[: type] = expr;   (mutable,   like Rust let mut)
 * NOTE: the caller must NOT pre-advance.
 * ==========================================================================*/
static curium_ast_v2_node_t* curium_parser_v2_parse_let_mut(curium_parser_v2_t* p, int is_mutable) {
    size_t line = p->current.line;
    size_t col  = p->current.column;

    /* Consume 'let' or 'mut' keyword */
    curium_parser_v2_advance(p);

    curium_parser_v2_expect(p, CURIUM_TOK_IDENTIFIER, "variable name");
    curium_string_t* name = curium_string_new(p->previous.lexeme ? p->previous.lexeme->data : "");

    /* Optional explicit type annotation */
    curium_ast_v2_node_t* type = NULL;
    if (curium_parser_v2_match(p, CURIUM_TOK_COLON)) {
        type = curium_parser_v2_parse_type(p);
    }

    /* Value (optional) */
    curium_ast_v2_node_t* init = NULL;
    if (curium_parser_v2_match(p, CURIUM_TOK_EQUAL)) {
        init = curium_parser_v2_parse_expr(p);
    }
    curium_parser_v2_expect(p, CURIUM_TOK_SEMI, ";");

    curium_ast_v2_node_t* decl = curium_ast_v2_new(is_mutable ? CURIUM_AST_V2_MUT : CURIUM_AST_V2_LET, line, col);
    if (is_mutable) {
        decl->as.mut_decl.name = name;
        decl->as.mut_decl.type = type;
        decl->as.mut_decl.init = init;
    } else {
        decl->as.let_decl.name = name;
        decl->as.let_decl.type = type;
        decl->as.let_decl.init = init;
    }
    return decl;
}

/* ============================================================================
 * Struct declaration: struct Name { field: type; ... }
 * NOTE: the caller must NOT pre-advance.
 * BUG FIX: fields are now properly created and linked.
 * ==========================================================================*/
static curium_ast_v2_node_t* curium_parser_v2_parse_struct(curium_parser_v2_t* p) {
    size_t line = p->current.line;
    size_t col  = p->current.column;

    curium_parser_v2_expect(p, CURIUM_TOK_KW_STRUCT, "struct");

    curium_parser_v2_expect(p, CURIUM_TOK_IDENTIFIER, "struct name");
    curium_string_t* name = curium_string_new(p->previous.lexeme ? p->previous.lexeme->data : "");

    /* Parse generic type parameters: struct Box<T> */
    curium_ast_v2_node_t* type_params = curium_parser_v2_parse_type_params(p);

    curium_parser_v2_expect(p, CURIUM_TOK_LBRACE, "{");

    /* Parse fields — BUG FIX: was creating field_name/field_type then throwing them away */
    curium_ast_v2_node_t* fields      = NULL;
    curium_ast_v2_node_t* fields_tail = NULL;

    while (!curium_parser_v2_match(p, CURIUM_TOK_RBRACE)) {
        if (p->current.kind == CURIUM_TOK_EOF) break;

        size_t fline = p->current.line;
        size_t fcol  = p->current.column;

        curium_parser_v2_expect(p, CURIUM_TOK_IDENTIFIER, "field name");
        curium_string_t* field_name = curium_string_new(p->previous.lexeme ? p->previous.lexeme->data : "");
        curium_parser_v2_expect(p, CURIUM_TOK_COLON, ":");
        curium_ast_v2_node_t* field_type = curium_parser_v2_parse_type(p);
        curium_parser_v2_expect(p, CURIUM_TOK_SEMI, ";");

        /* Create PARAM node to represent a struct field */
        curium_ast_v2_node_t* field_node = curium_ast_v2_new(CURIUM_AST_V2_PARAM, fline, fcol);
        field_node->as.param.name       = field_name;
        field_node->as.param.type       = field_type;
        field_node->as.param.is_mutable = 0;

        if (!fields) {
            fields      = field_node;
            fields_tail = field_node;
        } else {
            fields_tail->next = field_node;
            fields_tail       = field_node;
        }
    }

    curium_ast_v2_node_t* struct_node = curium_ast_v2_new(CURIUM_AST_V2_STRUCT, line, col);
    struct_node->as.struct_decl.name       = name;
    struct_node->as.struct_decl.type_params = type_params;
    struct_node->as.struct_decl.fields     = fields;
    struct_node->as.struct_decl.attributes = NULL;
    return struct_node;
}

/* ============================================================================
 * Enum declaration: enum Option { Some(T), None }
 * ==========================================================================*/
static curium_ast_v2_node_t* curium_parser_v2_parse_enum(curium_parser_v2_t* p) {
    size_t line = p->current.line;
    size_t col  = p->current.column;

    curium_parser_v2_expect(p, CURIUM_TOK_KW_ENUM, "enum");
    curium_parser_v2_expect(p, CURIUM_TOK_IDENTIFIER, "enum name");
    curium_string_t* name = curium_string_new(p->previous.lexeme ? p->previous.lexeme->data : "");
    
    /* Parse generic type parameters: enum Option<T> */
    curium_ast_v2_node_t* type_params = curium_parser_v2_parse_type_params(p);
    
    curium_parser_v2_expect(p, CURIUM_TOK_LBRACE, "{");

    curium_ast_v2_node_t* variants      = NULL;
    curium_ast_v2_node_t* variants_tail = NULL;

    while (!curium_parser_v2_match(p, CURIUM_TOK_RBRACE)) {
        if (p->current.kind == CURIUM_TOK_EOF) break;

        size_t vline = p->current.line;
        size_t vcol  = p->current.column;

        curium_parser_v2_expect(p, CURIUM_TOK_IDENTIFIER, "variant name");
        curium_string_t* variant_name = curium_string_new(p->previous.lexeme ? p->previous.lexeme->data : "");
        
        curium_ast_v2_node_t* assoc_types = NULL;
        curium_ast_v2_node_t* assoc_tail  = NULL;
        
        if (curium_parser_v2_match(p, CURIUM_TOK_LPAREN)) {
            while (!curium_parser_v2_match(p, CURIUM_TOK_RPAREN)) {
                if (p->current.kind == CURIUM_TOK_EOF) break;
                
                curium_ast_v2_node_t* t = curium_parser_v2_parse_type(p);
                if (t) {
                    if (!assoc_types) { assoc_types = t; assoc_tail = t; }
                    else { assoc_tail->next = t; assoc_tail = t; }
                }
                
                if (!curium_parser_v2_match(p, CURIUM_TOK_COMMA)) {
                    curium_parser_v2_expect(p, CURIUM_TOK_RPAREN, ")");
                    break;
                }
            }
        }
        
        curium_ast_v2_node_t* variant_node = curium_ast_v2_new(CURIUM_AST_V2_ENUM_VARIANT, vline, vcol);
        variant_node->as.enum_variant.name             = variant_name;
        variant_node->as.enum_variant.associated_types = assoc_types;
        
        if (!variants) {
            variants      = variant_node;
            variants_tail = variant_node;
        } else {
            variants_tail->next = variant_node;
            variants_tail       = variant_node;
        }
        
        /* Optional trailing comma */
        curium_parser_v2_match(p, CURIUM_TOK_COMMA);
    }

    curium_ast_v2_node_t* enum_node = curium_ast_v2_new(CURIUM_AST_V2_ENUM, line, col);
    enum_node->as.enum_decl.name       = name;
    enum_node->as.enum_decl.type_params = type_params;
    enum_node->as.enum_decl.fields     = variants;
    enum_node->as.enum_decl.attributes = NULL;
    return enum_node;
}

/* ============================================================================
 * Primary expression
 * ==========================================================================*/
static curium_ast_v2_node_t* curium_parser_v2_parse_primary(curium_parser_v2_t* p) {
    size_t line = p->current.line;
    size_t col  = p->current.column;

    if (curium_parser_v2_match(p, CURIUM_TOK_KW_SPAWN)) {
        curium_ast_v2_node_t* closure = curium_ast_v2_new(CURIUM_AST_V2_CLOSURE, line, col);
        closure->as.closure_expr.return_type = curium_ast_v2_new(CURIUM_AST_V2_TYPE_NAMED, line, col);
        closure->as.closure_expr.return_type->as.type_named.name = curium_string_new("void*");
        
        curium_ast_v2_node_t* param = curium_ast_v2_new(CURIUM_AST_V2_PARAM, line, col);
        param->as.param.name = curium_string_new("curium_arg");
        param->as.param.type = curium_ast_v2_new(CURIUM_AST_V2_TYPE_NAMED, line, col);
        param->as.param.type->as.type_named.name = curium_string_new("void*");
        param->as.param.is_mutable = 0;
        closure->as.closure_expr.params = param;

        curium_parser_v2_expect(p, CURIUM_TOK_LBRACE, "{");
        curium_ast_v2_node_t* body = NULL;
        curium_ast_v2_node_t* body_tail = NULL;
        while (p->current.kind != CURIUM_TOK_EOF && p->current.kind != CURIUM_TOK_RBRACE) {
            curium_ast_v2_node_t* stmt = curium_parser_v2_parse_stmt(p);
            if (!body) { body = stmt; body_tail = stmt; }
            else { body_tail->next = stmt; body_tail = stmt; }
        }
        curium_parser_v2_expect(p, CURIUM_TOK_RBRACE, "}");
        
        closure->as.closure_expr.body = body;

        curium_ast_v2_node_t* spawn = curium_ast_v2_new(CURIUM_AST_V2_SPAWN, line, col);
        spawn->as.spawn_stmt.closure = closure;
        return spawn;
    }

    if (curium_parser_v2_match(p, CURIUM_TOK_NUMBER)) {
        curium_ast_v2_node_t* num = curium_ast_v2_new(CURIUM_AST_V2_NUMBER, line, col);
        num->as.number_literal.value    = curium_string_new(p->previous.lexeme ? p->previous.lexeme->data : "");
        num->as.number_literal.is_float = (strchr(num->as.number_literal.value->data, '.') != NULL);
        return num;
    }

    if (curium_parser_v2_match(p, CURIUM_TOK_PIPE) || curium_parser_v2_match(p, CURIUM_TOK_PIPE_PIPE)) {
        curium_ast_v2_node_t* params = NULL;
        curium_ast_v2_node_t* params_tail = NULL;
        
        if (p->previous.kind == CURIUM_TOK_PIPE) {
            if (p->current.kind != CURIUM_TOK_PIPE) {
                do {
                    curium_ast_v2_node_t* param = curium_ast_v2_new(CURIUM_AST_V2_PARAM, p->current.line, p->current.column);
                    param->as.param.is_mutable = curium_parser_v2_match(p, CURIUM_TOK_KW_MUT) ? 1 : 0;
                    
                    curium_parser_v2_expect(p, CURIUM_TOK_IDENTIFIER, "closure parameter name");
                    param->as.param.name = curium_string_new(p->previous.lexeme ? p->previous.lexeme->data : "");
                    
                    if (curium_parser_v2_match(p, CURIUM_TOK_COLON)) {
                        param->as.param.type = curium_parser_v2_parse_type(p);
                    } else {
                        param->as.param.type = NULL;
                    }
                    
                    if (!params) { params = param; params_tail = param; }
                    else { params_tail->next = param; params_tail = param; }
                } while (curium_parser_v2_match(p, CURIUM_TOK_COMMA));
            }
            curium_parser_v2_expect(p, CURIUM_TOK_PIPE, "|");
        }
        
        curium_ast_v2_node_t* return_type = NULL;
        if (curium_parser_v2_match(p, CURIUM_TOK_ARROW)) {
            return_type = curium_parser_v2_parse_type(p);
        }
        
        curium_ast_v2_node_t* body = NULL;
        if (curium_parser_v2_match(p, CURIUM_TOK_LBRACE)) {
            curium_ast_v2_node_t* body_tail = NULL;
            while (p->current.kind != CURIUM_TOK_EOF && p->current.kind != CURIUM_TOK_RBRACE) {
                curium_ast_v2_node_t* stmt = curium_parser_v2_parse_stmt(p);
                if (!body) { body = stmt; body_tail = stmt; }
                else { body_tail->next = stmt; body_tail = stmt; }
            }
            curium_parser_v2_expect(p, CURIUM_TOK_RBRACE, "}");
        } else {
            curium_ast_v2_node_t* expr = curium_parser_v2_parse_expr(p);
            body = curium_ast_v2_new(CURIUM_AST_V2_RETURN, expr->line, expr->column);
            body->as.return_stmt.value = expr;
        }
        
        curium_ast_v2_node_t* closure = curium_ast_v2_new(CURIUM_AST_V2_CLOSURE, line, col);
        closure->as.closure_expr.params = params;
        closure->as.closure_expr.return_type = return_type;
        closure->as.closure_expr.body = body;
        return closure;
    }

    if (curium_parser_v2_match(p, CURIUM_TOK_STRING_LITERAL)) {
        curium_ast_v2_node_t* str = curium_ast_v2_new(CURIUM_AST_V2_STRING_LITERAL, line, col);
        str->as.string_literal.value = curium_string_new(p->previous.lexeme ? p->previous.lexeme->data : "");
        return str;
    }

    if (curium_parser_v2_match(p, CURIUM_TOK_INTERPOLATED_STRING)) {
        curium_ast_v2_node_t* interp = curium_ast_v2_new(CURIUM_AST_V2_INTERPOLATED_STRING, line, col);
        interp->as.interpolated_string.template = curium_string_new(p->previous.lexeme ? p->previous.lexeme->data : "");
        interp->as.interpolated_string.parts    = NULL;
        return interp;
    }

    if (curium_parser_v2_match(p, CURIUM_TOK_RAW_STRING)) {
        /* Treat raw strings as plain string literals in the AST */
        curium_ast_v2_node_t* str = curium_ast_v2_new(CURIUM_AST_V2_STRING_LITERAL, line, col);
        str->as.string_literal.value = curium_string_new(p->previous.lexeme ? p->previous.lexeme->data : "");
        return str;
    }

    if (curium_parser_v2_match(p, CURIUM_TOK_KW_TRUE)) {
        curium_ast_v2_node_t* b = curium_ast_v2_new(CURIUM_AST_V2_BOOL, line, col);
        b->as.bool_literal.value = 1;
        return b;
    }

    if (curium_parser_v2_match(p, CURIUM_TOK_KW_FALSE)) {
        curium_ast_v2_node_t* b = curium_ast_v2_new(CURIUM_AST_V2_BOOL, line, col);
        b->as.bool_literal.value = 0;
        return b;
    }

    if (curium_parser_v2_match(p, CURIUM_TOK_KW_NONE)) {
        return curium_ast_v2_new(CURIUM_AST_V2_OPTION_NONE, line, col);
    }

    if (curium_parser_v2_match(p, CURIUM_TOK_KW_SOME)) {
        curium_parser_v2_expect(p, CURIUM_TOK_LPAREN, "(");
        curium_ast_v2_node_t* value = curium_parser_v2_parse_expr(p);
        curium_parser_v2_expect(p, CURIUM_TOK_RPAREN, ")");
        curium_ast_v2_node_t* some = curium_ast_v2_new(CURIUM_AST_V2_OPTION_SOME, line, col);
        some->as.option_some.value = value;
        return some;
    }

    if (curium_parser_v2_match(p, CURIUM_TOK_KW_OK)) {
        curium_parser_v2_expect(p, CURIUM_TOK_LPAREN, "(");
        curium_ast_v2_node_t* value = curium_parser_v2_parse_expr(p);
        curium_parser_v2_expect(p, CURIUM_TOK_RPAREN, ")");
        curium_ast_v2_node_t* ok = curium_ast_v2_new(CURIUM_AST_V2_RESULT_OK, line, col);
        ok->as.result_ok.value = value;
        return ok;
    }

    if (curium_parser_v2_match(p, CURIUM_TOK_KW_ERR)) {
        curium_parser_v2_expect(p, CURIUM_TOK_LPAREN, "(");
        curium_ast_v2_node_t* value = curium_parser_v2_parse_expr(p);
        curium_parser_v2_expect(p, CURIUM_TOK_RPAREN, ")");
        curium_ast_v2_node_t* err = curium_ast_v2_new(CURIUM_AST_V2_RESULT_ERR, line, col);
        err->as.result_err.value = value;
        return err;
    }

    if (p->current.kind == CURIUM_TOK_IDENTIFIER || 
        p->current.kind == CURIUM_TOK_KW_GC         ||
        p->current.kind == CURIUM_TOK_KW_PRINT      ||
        p->current.kind == CURIUM_TOK_KW_MALLOC     ||
        p->current.kind == CURIUM_TOK_KW_FREE       ||
        p->current.kind == CURIUM_TOK_KW_INPUT      ||
        p->current.kind == CURIUM_TOK_KW_REQUIRE) {
        curium_ast_v2_node_t* ident = curium_ast_v2_new(CURIUM_AST_V2_IDENTIFIER, line, col);
        ident->as.identifier.value = curium_string_new(p->current.lexeme ? p->current.lexeme->data : "");
        curium_parser_v2_advance(p); /* Use advance instead of manual match for these grouped kinds */
        return ident;
    }

    if (curium_parser_v2_match(p, CURIUM_TOK_LPAREN)) {
        curium_ast_v2_node_t* expr = curium_parser_v2_parse_expr(p);
        curium_parser_v2_expect(p, CURIUM_TOK_RPAREN, ")");
        return expr;
    }

    char err_buf[256];
    snprintf(err_buf, sizeof(err_buf), "Expected expression but found '%s' (kind=%d)", p->current.lexeme ? p->current.lexeme->data : "EOF", p->current.kind);
    curium_error_set(CURIUM_ERROR_PARSE, err_buf);
    CURIUM_THROW(CURIUM_ERROR_PARSE, curium_error_get_message());
    return NULL;
}

/* ============================================================================
 * Postfix expressions: calls, field access, indexing, deref, error propagation
 * ==========================================================================*/
static curium_ast_v2_node_t* curium_parser_v2_parse_postfix(curium_parser_v2_t* p) {
    curium_ast_v2_node_t* expr = curium_parser_v2_parse_primary(p);

    for (;;) {
        size_t line = p->current.line;
        size_t col  = p->current.column;

        /* Function call: expr(args) */
        if (curium_parser_v2_match(p, CURIUM_TOK_LPAREN)) {
            curium_ast_v2_node_t* args      = NULL;
            curium_ast_v2_node_t* args_tail = NULL;

            if (!curium_parser_v2_match(p, CURIUM_TOK_RPAREN)) {
                args = curium_parser_v2_parse_expr(p);
                args_tail = args;
                while (curium_parser_v2_match(p, CURIUM_TOK_COMMA)) {
                    curium_ast_v2_node_t* arg = curium_parser_v2_parse_expr(p);
                    args_tail->next = arg;
                    args_tail       = arg;
                }
                curium_parser_v2_expect(p, CURIUM_TOK_RPAREN, ")");
            }

            curium_ast_v2_node_t* call = curium_ast_v2_new(CURIUM_AST_V2_CALL, line, col);
            call->as.call_expr.callee = expr;
            call->as.call_expr.args   = args;
            expr = call;
        }
        /* Field access: expr.field */
        else if (curium_parser_v2_match(p, CURIUM_TOK_DOT)) {
            curium_parser_v2_expect(p, CURIUM_TOK_IDENTIFIER, "field name");
            curium_string_t* field_name = curium_string_new(p->previous.lexeme ? p->previous.lexeme->data : "");
            curium_ast_v2_node_t* fa = curium_ast_v2_new(CURIUM_AST_V2_FIELD_ACCESS, line, col);
            fa->as.field_access.object = expr;
            fa->as.field_access.field  = field_name;
            expr = fa;
        }
        /* Array indexing: expr[index] — bounds-checked at codegen */
        else if (curium_parser_v2_match(p, CURIUM_TOK_LBRACKET)) {
            curium_ast_v2_node_t* index = curium_parser_v2_parse_expr(p);
            curium_parser_v2_expect(p, CURIUM_TOK_RBRACKET, "]");
            curium_ast_v2_node_t* idx = curium_ast_v2_new(CURIUM_AST_V2_INDEX, line, col);
            idx->as.index_expr.array = expr;
            idx->as.index_expr.index = index;
            expr = idx;
        }
        /* Postfix dereference: expr^ (like Go *ptr but postfix for readability) */
        else if (p->current.kind == CURIUM_TOK_ADDR_OF) {
            curium_parser_v2_advance(p);
            curium_ast_v2_node_t* deref = curium_ast_v2_new(CURIUM_AST_V2_DEREF, line, col);
            deref->as.deref_expr.expr = expr;
            expr = deref;
        }
        /* Error propagation: expr! (like Rust ? but postfix) */
        else if (curium_parser_v2_match(p, CURIUM_TOK_BANG)) {
            curium_ast_v2_node_t* unwrap = curium_ast_v2_new(CURIUM_AST_V2_UNARY_OP, line, col);
            unwrap->as.unary_expr.op   = curium_string_new("!");
            unwrap->as.unary_expr.expr = expr;
            expr = unwrap;
        }
        /* Early return error propagation: expr? */
        else if (curium_parser_v2_match(p, CURIUM_TOK_QUESTION)) {
            curium_ast_v2_node_t* try_expr = curium_ast_v2_new(CURIUM_AST_V2_TRY_EXPR, line, col);
            try_expr->as.try_expr.expr = expr;
            expr = try_expr;
        }
        else {
            break;
        }
    }

    return expr;
}

/* ============================================================================
 * Unary expressions: -expr, !expr, ^expr (prefix addr-of)
 * ==========================================================================*/
static curium_ast_v2_node_t* curium_parser_v2_parse_unary(curium_parser_v2_t* p) {
    size_t line = p->current.line;
    size_t col  = p->current.column;

    if (curium_parser_v2_match(p, CURIUM_TOK_MINUS) ||
        curium_parser_v2_match(p, CURIUM_TOK_BANG)  ||
        curium_parser_v2_match(p, CURIUM_TOK_ADDR_OF)) {
        curium_string_t* op   = curium_string_new(p->previous.lexeme ? p->previous.lexeme->data : "");
        curium_ast_v2_node_t* operand = curium_parser_v2_parse_unary(p);
        curium_ast_v2_node_t* unary = curium_ast_v2_new(CURIUM_AST_V2_UNARY_OP, line, col);
        unary->as.unary_expr.op   = op;
        unary->as.unary_expr.expr = operand;
        return unary;
    }

    return curium_parser_v2_parse_postfix(p);
}

/* ============================================================================
 * Binary expression (left-associative, precedence climbing)
 * Precedence levels:
 *   1  - ||
 *   2  - &&
 *   3  - == != < > <=  >=
 *   4  - + -
 *   5  - * /
 *   6  - identifier (dynamic operator)
 * ==========================================================================*/
static curium_ast_v2_node_t* curium_parser_v2_parse_binary(curium_parser_v2_t* p, int precedence) {
    curium_ast_v2_node_t* left = curium_parser_v2_parse_unary(p);

    for (;;) {
        size_t line = p->current.line;
        size_t col  = p->current.column;
        curium_string_t* op = NULL;
        int next_prec   = 0;

        if (p->current.kind == CURIUM_TOK_STAR || p->current.kind == CURIUM_TOK_SLASH) {
            next_prec = 5;
        } else if (p->current.kind == CURIUM_TOK_PLUS || p->current.kind == CURIUM_TOK_MINUS) {
            next_prec = 4;
        } else if (p->current.kind == CURIUM_TOK_EQUAL_EQUAL || p->current.kind == CURIUM_TOK_NOT_EQUAL ||
                   p->current.kind == CURIUM_TOK_LT          || p->current.kind == CURIUM_TOK_GT        ||
                   p->current.kind == CURIUM_TOK_LT_EQUAL    || p->current.kind == CURIUM_TOK_GT_EQUAL)  {
            next_prec = 3;
        } else if (p->current.kind == CURIUM_TOK_AND_AND) {
            next_prec = 2;
        } else if (p->current.kind == CURIUM_TOK_PIPE_PIPE) {
            next_prec = 1;
        } else if (p->current.kind == CURIUM_TOK_IDENTIFIER) {
            /* Dynamic operator: x action y  → DYN_CALL node */
            next_prec = 6;
        }

        if (next_prec <= precedence) {
            break;
        }

        /* For dyn calls (identifier as infix), create a DYN_CALL node */
        if (p->current.kind == CURIUM_TOK_IDENTIFIER && next_prec == 6) {
            op = curium_string_new(p->current.lexeme ? p->current.lexeme->data : "");
            curium_parser_v2_advance(p);
            curium_ast_v2_node_t* right = curium_parser_v2_parse_binary(p, next_prec);
            curium_ast_v2_node_t* dyn_call = curium_ast_v2_new(CURIUM_AST_V2_DYN_CALL, line, col);
            dyn_call->as.dyn_call.op_name = op;
            dyn_call->as.dyn_call.left    = left;
            dyn_call->as.dyn_call.right   = right;
            left = dyn_call;
            continue;
        }

        curium_parser_v2_match(p, p->current.kind);
        op = curium_string_new(p->previous.lexeme ? p->previous.lexeme->data : "");

        curium_ast_v2_node_t* right = curium_parser_v2_parse_binary(p, next_prec);

        {
            curium_ast_v2_node_t* binary = curium_ast_v2_new(CURIUM_AST_V2_BINARY_OP, line, col);
            binary->as.binary_expr.op    = op;
            binary->as.binary_expr.left  = left;
            binary->as.binary_expr.right = right;
            left = binary;
        }
    }

    return left;
}

/* ============================================================================
 * Expression
 * ==========================================================================*/
static curium_ast_v2_node_t* curium_parser_v2_parse_expr(curium_parser_v2_t* p) {
    return curium_parser_v2_parse_binary(p, 0);
}

/* ============================================================================
 * Assignment: target = expr;    or    target := expr;  (type-inferred)
 * ==========================================================================*/
static curium_ast_v2_node_t* curium_parser_v2_parse_assignment(curium_parser_v2_t* p, curium_ast_v2_node_t* target) {
    size_t line = p->current.line;
    size_t col  = p->current.column;

    /* consume '=' or ':=' */
    curium_parser_v2_advance(p);

    curium_ast_v2_node_t* value = curium_parser_v2_parse_expr(p);
    curium_parser_v2_expect(p, CURIUM_TOK_SEMI, ";");

    curium_ast_v2_node_t* assign = curium_ast_v2_new(CURIUM_AST_V2_ASSIGN, line, col);
    assign->as.assign_stmt.target = target;
    assign->as.assign_stmt.value  = value;
    return assign;
}

/* ============================================================================
 * If / else if / else statement
 * NOTE: the caller must NOT pre-advance past 'if'.
 * ==========================================================================*/
static curium_ast_v2_node_t* curium_parser_v2_parse_if(curium_parser_v2_t* p) {
    size_t line = p->current.line;
    size_t col  = p->current.column;

    curium_parser_v2_expect(p, CURIUM_TOK_KW_IF, "if");

    curium_ast_v2_node_t* condition = curium_parser_v2_parse_expr(p);
    curium_parser_v2_expect(p, CURIUM_TOK_LBRACE, "{");

    curium_ast_v2_node_t* then_branch = NULL;
    curium_ast_v2_node_t* then_tail   = NULL;
    if (!curium_parser_v2_match(p, CURIUM_TOK_RBRACE)) {
        then_branch = curium_parser_v2_parse_stmt(p);
        then_tail   = then_branch;
        while (p->current.kind != CURIUM_TOK_RBRACE && p->current.kind != CURIUM_TOK_EOF) {
            curium_ast_v2_node_t* s = curium_parser_v2_parse_stmt(p);
            if (s) { then_tail->next = s; then_tail = s; }
        }
        curium_parser_v2_expect(p, CURIUM_TOK_RBRACE, "}");
    }

    curium_ast_v2_node_t* else_branch = NULL;
    if (curium_parser_v2_match(p, CURIUM_TOK_KW_ELSE)) {
        if (p->current.kind == CURIUM_TOK_KW_IF) {
            /* else if — recurse without advancing (parse_if consumes 'if') */
            else_branch = curium_parser_v2_parse_if(p);
        } else {
            curium_parser_v2_expect(p, CURIUM_TOK_LBRACE, "{");
            curium_ast_v2_node_t* else_tail = NULL;
            if (!curium_parser_v2_match(p, CURIUM_TOK_RBRACE)) {
                else_branch = curium_parser_v2_parse_stmt(p);
                else_tail   = else_branch;
                while (p->current.kind != CURIUM_TOK_RBRACE && p->current.kind != CURIUM_TOK_EOF) {
                    curium_ast_v2_node_t* s = curium_parser_v2_parse_stmt(p);
                    if (s) { else_tail->next = s; else_tail = s; }
                }
                curium_parser_v2_expect(p, CURIUM_TOK_RBRACE, "}");
            }
        }
    }

    curium_ast_v2_node_t* if_node = curium_ast_v2_new(CURIUM_AST_V2_IF, line, col);
    if_node->as.if_stmt.condition   = condition;
    if_node->as.if_stmt.then_branch = then_branch;
    if_node->as.if_stmt.else_branch = else_branch;
    return if_node;
}

/* ============================================================================
 * While statement
 * NOTE: the caller must NOT pre-advance past 'while'.
 * ==========================================================================*/
static curium_ast_v2_node_t* curium_parser_v2_parse_while(curium_parser_v2_t* p) {
    size_t line = p->current.line;
    size_t col  = p->current.column;

    curium_parser_v2_expect(p, CURIUM_TOK_KW_WHILE, "while");

    curium_ast_v2_node_t* condition = curium_parser_v2_parse_expr(p);
    curium_parser_v2_expect(p, CURIUM_TOK_LBRACE, "{");

    curium_ast_v2_node_t* body      = NULL;
    curium_ast_v2_node_t* body_tail = NULL;
    if (!curium_parser_v2_match(p, CURIUM_TOK_RBRACE)) {
        body = curium_parser_v2_parse_stmt(p);
        body_tail = body;
        while (p->current.kind != CURIUM_TOK_RBRACE && p->current.kind != CURIUM_TOK_EOF) {
            curium_ast_v2_node_t* s = curium_parser_v2_parse_stmt(p);
            if (s) { body_tail->next = s; body_tail = s; }
        }
        curium_parser_v2_expect(p, CURIUM_TOK_RBRACE, "}");
    }

    curium_ast_v2_node_t* while_node = curium_ast_v2_new(CURIUM_AST_V2_WHILE, line, col);
    while_node->as.while_stmt.condition = condition;
    while_node->as.while_stmt.body      = body;
    return while_node;
}

/* ============================================================================
 * For statement: for varName in iterable { body }
 * NOTE: the caller must NOT pre-advance past 'for'.
 * BUG FIX: variable name is now stored in the AST (was discarded before).
 * ==========================================================================*/
static curium_ast_v2_node_t* curium_parser_v2_parse_for(curium_parser_v2_t* p) {
    size_t line = p->current.line;
    size_t col  = p->current.column;

    curium_parser_v2_expect(p, CURIUM_TOK_KW_FOR, "for");

    /* Loop variable name — required */
    curium_parser_v2_expect(p, CURIUM_TOK_IDENTIFIER, "loop variable name");
    curium_string_t* var_name = curium_string_new(p->previous.lexeme ? p->previous.lexeme->data : "");

    /* Expect 'in' keyword (treated as identifier since it has no dedicated token) */
    if (p->current.kind == CURIUM_TOK_IDENTIFIER &&
        p->current.lexeme &&
        strcmp(p->current.lexeme->data, "in") == 0) {
        curium_parser_v2_advance(p); /* consume 'in' */
    } else {
        curium_error_set(CURIUM_ERROR_PARSE, "Expected 'in' after loop variable");
        CURIUM_THROW(CURIUM_ERROR_PARSE, "'in'");
    }

    /* Iterable expression */
    curium_ast_v2_node_t* iterable = curium_parser_v2_parse_expr(p);

    curium_parser_v2_expect(p, CURIUM_TOK_LBRACE, "{");

    curium_ast_v2_node_t* body      = NULL;
    curium_ast_v2_node_t* body_tail = NULL;
    if (!curium_parser_v2_match(p, CURIUM_TOK_RBRACE)) {
        body = curium_parser_v2_parse_stmt(p);
        body_tail = body;
        while (p->current.kind != CURIUM_TOK_RBRACE && p->current.kind != CURIUM_TOK_EOF) {
            curium_ast_v2_node_t* s = curium_parser_v2_parse_stmt(p);
            if (s) { body_tail->next = s; body_tail = s; }
        }
        curium_parser_v2_expect(p, CURIUM_TOK_RBRACE, "}");
    }

    curium_ast_v2_node_t* for_node = curium_ast_v2_new(CURIUM_AST_V2_FOR, line, col);
    for_node->as.for_stmt.var_name  = var_name;
    for_node->as.for_stmt.iterable  = iterable;
    for_node->as.for_stmt.body      = body;
    return for_node;
}

/* ============================================================================
 * Match arm: pattern => expr,
 * ==========================================================================*/
static curium_ast_v2_node_t* curium_parser_v2_parse_match_arm(curium_parser_v2_t* p) {
    size_t line = p->current.line;
    size_t col  = p->current.column;

    curium_ast_v2_node_t* pattern = curium_parser_v2_parse_expr(p);
    curium_parser_v2_expect(p, CURIUM_TOK_FAT_ARROW, "=>");
    curium_ast_v2_node_t* expr = curium_parser_v2_parse_expr(p);
    /* Trailing comma is optional before closing brace */
    curium_parser_v2_match(p, CURIUM_TOK_COMMA);

    curium_ast_v2_node_t* arm = curium_ast_v2_new(CURIUM_AST_V2_MATCH_ARM, line, col);
    arm->as.match_arm.pattern = pattern;
    arm->as.match_arm.expr    = expr;
    return arm;
}

/* ============================================================================
 * Match statement: match expr { pattern => expr, ... }
 * NOTE: the caller must NOT pre-advance past 'match'.
 * ==========================================================================*/
static curium_ast_v2_node_t* curium_parser_v2_parse_match(curium_parser_v2_t* p) {
    size_t line = p->current.line;
    size_t col  = p->current.column;

    curium_parser_v2_expect(p, CURIUM_TOK_KW_MATCH, "match");

    curium_ast_v2_node_t* expr = curium_parser_v2_parse_expr(p);
    curium_parser_v2_expect(p, CURIUM_TOK_LBRACE, "{");

    curium_ast_v2_node_t* arms      = NULL;
    curium_ast_v2_node_t* arms_tail = NULL;
    while (p->current.kind != CURIUM_TOK_RBRACE && p->current.kind != CURIUM_TOK_EOF) {
        curium_ast_v2_node_t* arm = curium_parser_v2_parse_match_arm(p);
        if (!arms) {
            arms      = arm;
            arms_tail = arm;
        } else {
            arms_tail->next = arm;
            arms_tail       = arm;
        }
    }

    curium_parser_v2_expect(p, CURIUM_TOK_RBRACE, "}");

    curium_ast_v2_node_t* match_node = curium_ast_v2_new(CURIUM_AST_V2_MATCH, line, col);
    match_node->as.match_expr.expr = expr;
    match_node->as.match_expr.arms = arms;
    return match_node;
}

/* ============================================================================
 * Return statement
 * NOTE: the caller must NOT pre-advance past 'return'.
 * ==========================================================================*/
static curium_ast_v2_node_t* curium_parser_v2_parse_return(curium_parser_v2_t* p) {
    size_t line = p->current.line;
    size_t col  = p->current.column;

    curium_parser_v2_expect(p, CURIUM_TOK_KW_RETURN, "return");

    curium_ast_v2_node_t* value = NULL;
    if (p->current.kind != CURIUM_TOK_SEMI) {
        value = curium_parser_v2_parse_expr(p);
    }
    curium_parser_v2_expect(p, CURIUM_TOK_SEMI, ";");

    curium_ast_v2_node_t* ret = curium_ast_v2_new(CURIUM_AST_V2_RETURN, line, col);
    ret->as.return_stmt.value = value;
    return ret;
}

/* ============================================================================
 * Impl declaration: impl Name { fn method() {} ... }
 * NOTE: the caller must NOT pre-advance.
 * ==========================================================================*/
static curium_ast_v2_node_t* curium_parser_v2_parse_impl(curium_parser_v2_t* p) {
    size_t line = p->current.line;
    size_t col  = p->current.column;

    curium_parser_v2_expect(p, CURIUM_TOK_KW_IMPL, "impl");

    /* Parse generic type parameters: impl<T> */
    curium_ast_v2_node_t* type_params = curium_parser_v2_parse_type_params(p);

    /* Parse the first type. This could be the Trait OR the Target Type */
    curium_ast_v2_node_t* trait_type = NULL;
    curium_ast_v2_node_t* target_type = curium_parser_v2_parse_type(p);

    /* If 'for' follows, the first type was the Trait, and the actual Target follows */
    if (curium_parser_v2_match(p, CURIUM_TOK_KW_FOR)) {
        trait_type = target_type;
        target_type = curium_parser_v2_parse_type(p);
    }

    curium_parser_v2_expect(p, CURIUM_TOK_LBRACE, "{");

    curium_ast_v2_node_t* methods      = NULL;
    curium_ast_v2_node_t* methods_tail = NULL;

    while (!curium_parser_v2_match(p, CURIUM_TOK_RBRACE)) {
        if (p->current.kind == CURIUM_TOK_EOF) break;
        
        if (p->current.kind == CURIUM_TOK_KW_FN) {
            curium_ast_v2_node_t* method_node = curium_parser_v2_parse_fn(p);
            if (!methods) {
                methods      = method_node;
                methods_tail = method_node;
            } else {
                methods_tail->next = method_node;
                methods_tail       = method_node;
            }
        } else {
            curium_error_set(CURIUM_ERROR_PARSE, "only functions are allowed in impl blocks");
            CURIUM_THROW(CURIUM_ERROR_PARSE, "only functions are allowed inside impl blocks");
        }
    }

    curium_ast_v2_node_t* impl_node = curium_ast_v2_new(CURIUM_AST_V2_IMPL, line, col);
    impl_node->as.impl_decl.type_params = type_params;
    impl_node->as.impl_decl.trait_type  = trait_type;
    impl_node->as.impl_decl.target_type = target_type;
    impl_node->as.impl_decl.methods     = methods;
    return impl_node;
}

/* ============================================================================
 * Trait declaration: trait Name { fn method(self) -> type; ... }
 * NOTE: the caller must NOT pre-advance.
 * ==========================================================================*/
static curium_ast_v2_node_t* curium_parser_v2_parse_trait(curium_parser_v2_t* p) {
    size_t line = p->current.line;
    size_t col  = p->current.column;

    curium_parser_v2_expect(p, CURIUM_TOK_KW_TRAIT, "trait");

    curium_parser_v2_expect(p, CURIUM_TOK_IDENTIFIER, "trait name");
    curium_string_t* name = curium_string_new(p->previous.lexeme ? p->previous.lexeme->data : "");

    curium_parser_v2_expect(p, CURIUM_TOK_LBRACE, "{");

    curium_ast_v2_node_t* signatures      = NULL;
    curium_ast_v2_node_t* signatures_tail = NULL;

    while (!curium_parser_v2_match(p, CURIUM_TOK_RBRACE)) {
        if (p->current.kind == CURIUM_TOK_EOF) break;
        
        if (p->current.kind == CURIUM_TOK_KW_FN) {
            size_t sline = p->current.line;
            size_t scol  = p->current.column;
            
            curium_parser_v2_advance(p); /* consume 'fn' */
            
            curium_parser_v2_expect(p, CURIUM_TOK_IDENTIFIER, "method name");
            curium_string_t* mname = curium_string_new(p->previous.lexeme ? p->previous.lexeme->data : "");
            
            curium_parser_v2_expect(p, CURIUM_TOK_LPAREN, "(");
            curium_ast_v2_node_t* params = NULL;
            curium_ast_v2_node_t* ptail = NULL;
            if (!curium_parser_v2_match(p, CURIUM_TOK_RPAREN)) {
                params = curium_parser_v2_parse_param(p);
                ptail = params;
                while (curium_parser_v2_match(p, CURIUM_TOK_COMMA)) {
                    curium_ast_v2_node_t* nxt = curium_parser_v2_parse_param(p);
                    ptail->next = nxt;
                    ptail = nxt;
                }
                curium_parser_v2_expect(p, CURIUM_TOK_RPAREN, ")");
            }
            
            curium_ast_v2_node_t* ret = NULL;
            if (curium_parser_v2_match(p, CURIUM_TOK_ARROW)) {
                ret = curium_parser_v2_parse_type(p);
            } else {
                ret = curium_ast_v2_new(CURIUM_AST_V2_TYPE_NAMED, sline, scol);
                ret->as.type_named.name = curium_string_new("void");
            }
            
            curium_parser_v2_expect(p, CURIUM_TOK_SEMI, ";");
            
            curium_ast_v2_node_t* sig_node = curium_ast_v2_new(CURIUM_AST_V2_FN, sline, scol);
            sig_node->as.fn_decl.name        = mname;
            sig_node->as.fn_decl.params      = params;
            sig_node->as.fn_decl.return_type = ret;
            sig_node->as.fn_decl.body        = NULL; /* No body */
            
            if (!signatures) { signatures = sig_node; signatures_tail = sig_node; }
            else { signatures_tail->next = sig_node; signatures_tail = sig_node; }
        } else {
            curium_error_set(CURIUM_ERROR_PARSE, "only function signatures are allowed in trait blocks");
            CURIUM_THROW(CURIUM_ERROR_PARSE, "invalid trait member");
        }
    }

    curium_ast_v2_node_t* trait_node = curium_ast_v2_new(CURIUM_AST_V2_TRAIT, line, col);
    trait_node->as.trait_decl.name       = name;
    trait_node->as.trait_decl.signatures = signatures;
    trait_node->as.trait_decl.is_public  = 0;
    return trait_node;
}

/* ============================================================================
 * Dynamic elements: dyn op in ( arms ) dyn($) { fallback };
 * or Dynamic Variable: dyn name = expr;
 * or Dynamic Function: dyn fn name(...) { ... }
 * ==========================================================================*/
static curium_ast_v2_node_t* curium_parser_v2_parse_dyn(curium_parser_v2_t* p) {
    size_t line = p->current.line;
    size_t col  = p->current.column;

    /* consume 'dyn' */
    curium_parser_v2_expect(p, CURIUM_TOK_KW_DYN, "dyn");

    /* dyn fn name(...) { ... } */
    if (p->current.kind == CURIUM_TOK_KW_FN) {
        curium_ast_v2_node_t* fn_decl = curium_parser_v2_parse_fn(p);
        fn_decl->as.fn_decl.is_dynamic = 1;
        return fn_decl;
    }

    /* read operator or variable name */
    curium_parser_v2_expect(p, CURIUM_TOK_IDENTIFIER, "dynamic name");
    curium_string_t* name = curium_string_new(p->previous.lexeme ? p->previous.lexeme->data : "");

    /* dyn var = expr; */
    if (curium_parser_v2_match(p, CURIUM_TOK_EQUAL) || curium_parser_v2_match(p, CURIUM_TOK_COLON_EQUAL)) {
        curium_ast_v2_node_t* init = curium_parser_v2_parse_expr(p);
        curium_parser_v2_expect(p, CURIUM_TOK_SEMI, ";");

        curium_ast_v2_node_t* decl = curium_ast_v2_new(CURIUM_AST_V2_LET, line, col);
        decl->as.let_decl.name = name;
        decl->as.let_decl.init = init;
        decl->as.let_decl.is_dynamic = 1;
        return decl;
    }

    /* dyn var_with_type: type = expr; */
    if (curium_parser_v2_match(p, CURIUM_TOK_COLON)) {
        curium_ast_v2_node_t* type = curium_parser_v2_parse_type(p);
        curium_parser_v2_expect(p, CURIUM_TOK_EQUAL, "=");
        curium_ast_v2_node_t* init = curium_parser_v2_parse_expr(p);
        curium_parser_v2_expect(p, CURIUM_TOK_SEMI, ";");

        curium_ast_v2_node_t* decl = curium_ast_v2_new(CURIUM_AST_V2_LET, line, col);
        decl->as.let_decl.name = name;
        decl->as.let_decl.type = type;
        decl->as.let_decl.init = init;
        decl->as.let_decl.is_dynamic = 1;
        return decl;
    }

    /* expect 'in' (parsed as identifier) */
    curium_parser_v2_expect(p, CURIUM_TOK_IDENTIFIER, "in");

    /* expect '(' */
    curium_parser_v2_expect(p, CURIUM_TOK_LPAREN, "(");

    /* parse arms: "key" => { body }, ... */
    curium_ast_v2_node_t* arms      = NULL;
    curium_ast_v2_node_t* arms_tail = NULL;

    while (p->current.kind != CURIUM_TOK_RPAREN && p->current.kind != CURIUM_TOK_EOF) {
        size_t arm_line = p->current.line;
        size_t arm_col  = p->current.column;

        /* pattern: string literal */
        curium_ast_v2_node_t* pattern = curium_parser_v2_parse_expr(p);

        /* expect '=>' */
        curium_parser_v2_expect(p, CURIUM_TOK_FAT_ARROW, "=>");

        if (p->current.kind == CURIUM_TOK_KW_CALL) {
            curium_parser_v2_advance(p); /* consume 'call' */
            curium_parser_v2_expect(p, CURIUM_TOK_IDENTIFIER, "function name after call");
            curium_string_t* target_fn = curium_string_new(p->previous.lexeme ? p->previous.lexeme->data : "");
            
            curium_parser_v2_expect(p, CURIUM_TOK_LPAREN, "(");
            curium_ast_v2_node_t* args = NULL;
            curium_ast_v2_node_t* args_tail = NULL;
            while (p->current.kind != CURIUM_TOK_RPAREN && p->current.kind != CURIUM_TOK_EOF) {
                curium_ast_v2_node_t* arg = curium_parser_v2_parse_expr(p);
                if (arg) {
                    if (!args) { args = arg; args_tail = arg; }
                    else { args_tail->next = arg; args_tail = arg; }
                }
                if (p->current.kind == CURIUM_TOK_COMMA) curium_parser_v2_advance(p);
            }
            curium_parser_v2_expect(p, CURIUM_TOK_RPAREN, ")");
            
            curium_ast_v2_node_t* arm = curium_ast_v2_new(CURIUM_AST_V2_DYN_CALL_ARM, arm_line, arm_col);
            arm->as.dyn_call_arm.pattern = pattern;
            arm->as.dyn_call_arm.target_fn = target_fn;
            arm->as.dyn_call_arm.args = args;

            if (!arms) { arms = arm; arms_tail = arm; }
            else { arms_tail->next = arm; arms_tail = arm; }
        } else {
            /* expect '{' */
            curium_parser_v2_expect(p, CURIUM_TOK_LBRACE, "{");

            /* parse body statements */
            curium_ast_v2_node_t* body      = NULL;
            curium_ast_v2_node_t* body_tail = NULL;
            while (p->current.kind != CURIUM_TOK_RBRACE && p->current.kind != CURIUM_TOK_EOF) {
                curium_ast_v2_node_t* s = curium_parser_v2_parse_stmt(p);
                if (s) {
                    if (!body) { body = s; body_tail = s; }
                    else { body_tail->next = s; body_tail = s; }
                }
            }
            curium_parser_v2_expect(p, CURIUM_TOK_RBRACE, "}");

            /* Create match arm node (reuse ARM kind) */
            curium_ast_v2_node_t* arm = curium_ast_v2_new(CURIUM_AST_V2_MATCH_ARM, arm_line, arm_col);
            arm->as.match_arm.pattern = pattern;
            arm->as.match_arm.expr    = body; /* body is a linked list of stmts */

            if (!arms) { arms = arm; arms_tail = arm; }
            else { arms_tail->next = arm; arms_tail = arm; }
        }

        /* optional comma between arms */
        curium_parser_v2_match(p, CURIUM_TOK_COMMA);
    }

    curium_parser_v2_expect(p, CURIUM_TOK_RPAREN, ")");

    /* Fallbacks: dyn(cond) { body } or dyn($) { body } */
    curium_ast_v2_node_t* fallbacks = NULL;
    curium_ast_v2_node_t* fb_list_tail = NULL;
    
    while (p->current.kind == CURIUM_TOK_KW_DYN) {
        size_t fb_line = p->current.line;
        size_t fb_col  = p->current.column;
        curium_parser_v2_advance(p); /* consume 'dyn' */
        curium_parser_v2_expect(p, CURIUM_TOK_LPAREN, "(");
        
        curium_ast_v2_node_t* cond = NULL;
        if (curium_parser_v2_match(p, CURIUM_TOK_DOLLAR)) {
            /* catch-all */
            cond = NULL;
        } else {
            cond = curium_parser_v2_parse_expr(p);
        }
        curium_parser_v2_expect(p, CURIUM_TOK_RPAREN, ")");
        curium_parser_v2_expect(p, CURIUM_TOK_LBRACE, "{");

        curium_ast_v2_node_t* body = NULL;
        curium_ast_v2_node_t* fb_tail = NULL;
        while (p->current.kind != CURIUM_TOK_RBRACE && p->current.kind != CURIUM_TOK_EOF) {
            curium_ast_v2_node_t* s = curium_parser_v2_parse_stmt(p);
            if (s) {
                if (!body) { body = s; fb_tail = s; }
                else { fb_tail->next = s; fb_tail = s; }
            }
        }
        curium_parser_v2_expect(p, CURIUM_TOK_RBRACE, "}");
        
        curium_ast_v2_node_t* fb_node = curium_ast_v2_new(CURIUM_AST_V2_DYN_FALLBACK, fb_line, fb_col);
        fb_node->as.dyn_fallback.cond = cond;
        fb_node->as.dyn_fallback.body = body;
        
        if (!fallbacks) { fallbacks = fb_node; fb_list_tail = fb_node; }
        else { fb_list_tail->next = fb_node; fb_list_tail = fb_node; }
    }

    /* expect ';' */
    curium_parser_v2_expect(p, CURIUM_TOK_SEMI, ";");

    curium_ast_v2_node_t* dyn_node = curium_ast_v2_new(CURIUM_AST_V2_DYN_OP, line, col);
    dyn_node->as.dyn_op.name      = name;
    dyn_node->as.dyn_op.arms      = arms;
    dyn_node->as.dyn_op.fallbacks = fallbacks;
    return dyn_node;
}

/* ============================================================================
 * Try/Catch statement: try { ... } catch (e) { ... }
 * NOTE: the caller must NOT pre-advance past 'try'.
 * ==========================================================================*/
static curium_ast_v2_node_t* curium_parser_v2_parse_try(curium_parser_v2_t* p) {
    size_t line = p->current.line;
    size_t col  = p->current.column;

    curium_parser_v2_expect(p, CURIUM_TOK_KW_TRY, "try");
    curium_parser_v2_expect(p, CURIUM_TOK_LBRACE, "{");

    curium_ast_v2_node_t* try_block = NULL;
    curium_ast_v2_node_t* try_tail  = NULL;
    if (!curium_parser_v2_match(p, CURIUM_TOK_RBRACE)) {
        try_block = curium_parser_v2_parse_stmt(p);
        try_tail  = try_block;
        while (p->current.kind != CURIUM_TOK_RBRACE && p->current.kind != CURIUM_TOK_EOF) {
            curium_ast_v2_node_t* s = curium_parser_v2_parse_stmt(p);
            if (s) { try_tail->next = s; try_tail = s; }
        }
        curium_parser_v2_expect(p, CURIUM_TOK_RBRACE, "}");
    }

    /* Optional catch block */
    curium_string_t* catch_var = NULL;
    curium_ast_v2_node_t* catch_block = NULL;

    if (curium_parser_v2_match(p, CURIUM_TOK_KW_CATCH)) {
        /* catch (e) */
        if (curium_parser_v2_match(p, CURIUM_TOK_LPAREN)) {
            curium_parser_v2_expect(p, CURIUM_TOK_IDENTIFIER, "catch variable name");
            catch_var = curium_string_new(p->previous.lexeme ? p->previous.lexeme->data : "");
            curium_parser_v2_expect(p, CURIUM_TOK_RPAREN, ")");
        }

        curium_parser_v2_expect(p, CURIUM_TOK_LBRACE, "{");
        curium_ast_v2_node_t* catch_tail = NULL;
        if (!curium_parser_v2_match(p, CURIUM_TOK_RBRACE)) {
            catch_block = curium_parser_v2_parse_stmt(p);
            catch_tail  = catch_block;
            while (p->current.kind != CURIUM_TOK_RBRACE && p->current.kind != CURIUM_TOK_EOF) {
                curium_ast_v2_node_t* s = curium_parser_v2_parse_stmt(p);
                if (s) { catch_tail->next = s; catch_tail = s; }
            }
            curium_parser_v2_expect(p, CURIUM_TOK_RBRACE, "}");
        }
    }

    curium_ast_v2_node_t* try_node = curium_ast_v2_new(CURIUM_AST_V2_TRY_CATCH, line, col);
    try_node->as.try_catch_stmt.try_block   = try_block;
    try_node->as.try_catch_stmt.catch_var   = catch_var;
    try_node->as.try_catch_stmt.catch_block = catch_block;
    return try_node;
}

/* ============================================================================
 * Throw statement: throw expr;
 * NOTE: the caller must NOT pre-advance past 'throw'.
 * ==========================================================================*/
static curium_ast_v2_node_t* curium_parser_v2_parse_throw(curium_parser_v2_t* p) {
    size_t line = p->current.line;
    size_t col  = p->current.column;

    curium_parser_v2_expect(p, CURIUM_TOK_KW_THROW, "throw");
    curium_ast_v2_node_t* expr = curium_parser_v2_parse_expr(p);
    curium_parser_v2_expect(p, CURIUM_TOK_SEMI, ";");

    curium_ast_v2_node_t* throw_node = curium_ast_v2_new(CURIUM_AST_V2_THROW, line, col);
    throw_node->as.throw_stmt.expr = expr;
    return throw_node;
}

/* ============================================================================
 * Import statement: import "path.cm";
 * ==========================================================================*/
static curium_ast_v2_node_t* curium_parser_v2_parse_import(curium_parser_v2_t* p) {
    size_t line = p->current.line;
    size_t col  = p->current.column;
    curium_parser_v2_advance(p); // consume import

    curium_parser_v2_expect(p, CURIUM_TOK_STRING_LITERAL, "module path string");
    curium_string_t* path = curium_string_new(p->previous.lexeme ? p->previous.lexeme->data : "");
    
    curium_parser_v2_expect(p, CURIUM_TOK_SEMI, ";");

    curium_ast_v2_node_t* import_node = curium_ast_v2_new(CURIUM_AST_V2_IMPORT, line, col);
    import_node->as.import_decl.path = path;
    return import_node;
}

/* ============================================================================
 * Statement dispatcher
 * CRITICAL BUG FIX: parse_stmt MUST NOT pre-advance past the leading keyword.
 * Each parse_XXX function is responsible for consuming its own keyword via
 * curium_parser_v2_expect(), so parse_stmt checks p->current.kind without advancing.
 * ==========================================================================*/
static curium_ast_v2_node_t* curium_parser_v2_parse_stmt(curium_parser_v2_t* p) {
    /* Visibility modifier */
    int is_public = 0;
    if (p->current.kind == CURIUM_TOK_KW_PUBLIC) {
        is_public = 1;
        curium_parser_v2_advance(p);
    }

    /* Declarations */
    if (p->current.kind == CURIUM_TOK_KW_FN) {
        curium_ast_v2_node_t* node = curium_parser_v2_parse_fn(p);
        if (node) node->as.fn_decl.is_public = is_public;
        return node;
    }
    if (p->current.kind == CURIUM_TOK_KW_IMPL) {
        /* impl blocks don't typically have visibility themselves, their methods do */
        return curium_parser_v2_parse_impl(p);
    }
    if (p->current.kind == CURIUM_TOK_KW_TRAIT) {
        curium_ast_v2_node_t* node = curium_parser_v2_parse_trait(p);
        if (node) node->as.trait_decl.is_public = is_public;
        return node;
    }
    /* v5.0 Phase 3: Developer Cache Control — #[hot] attribute
     *
     * #[hot] before a let/mut declaration tells the codegen to emit the
     * C `register` keyword, hinting the compiler to keep the variable
     * on the Cutting Board (CPU registers) rather than spilling to the
     * Fridge (RAM / stack).
     *
     * Syntax supported:
     *   #[hot] let x: int = 42;    → register const int curium_x = 42;
     *   #[hot] mut y: int = 0;     → register int curium_y = 0;
     *   @hot   let x: int = 42;    → same (legacy @ attribute syntax)
     */
    int is_hot = 0;
    if (p->current.kind == CURIUM_TOK_HASH_ATTR) {
        /* #[name] — only act on "hot"; silently skip unknown attributes */
        if (p->current.lexeme && strcmp(p->current.lexeme->data, "hot") == 0) {
            is_hot = 1;
        }
        curium_parser_v2_advance(p); /* consume attribute token */
    } else if (p->current.kind == CURIUM_TOK_AT) {
        /* @hot — legacy @ attribute syntax */
        curium_parser_v2_advance(p); /* consume '@' */
        if (p->current.kind == CURIUM_TOK_IDENTIFIER &&
            p->current.lexeme && strcmp(p->current.lexeme->data, "hot") == 0) {
            is_hot = 1;
            curium_parser_v2_advance(p); /* consume 'hot' */
        }
    }

    if (p->current.kind == CURIUM_TOK_KW_LET || p->current.kind == CURIUM_TOK_KW_MUT) {
        int is_mut = (p->current.kind == CURIUM_TOK_KW_MUT);
        curium_ast_v2_node_t* node = curium_parser_v2_parse_let_mut(p, is_mut);
        if (node) {
            if (!is_mut) {
                node->as.let_decl.is_public = is_public;
                node->as.let_decl.is_hot    = is_hot;
            } else {
                node->as.mut_decl.is_public = is_public;
                node->as.mut_decl.is_hot    = is_hot;
            }
        }
        return node;
    }
    if (p->current.kind == CURIUM_TOK_KW_STRUCT) {
        curium_ast_v2_node_t* node = curium_parser_v2_parse_struct(p);
        if (node) node->as.struct_decl.is_public = is_public;
        return node;
    }
    if (p->current.kind == CURIUM_TOK_KW_ENUM) {
        curium_ast_v2_node_t* node = curium_parser_v2_parse_enum(p);
        if (node) node->as.enum_decl.is_public = is_public;
        return node;
    }

    if (p->current.kind == CURIUM_TOK_KW_IMPORT) {
        return curium_parser_v2_parse_import(p);
    }

    if (p->current.kind == CURIUM_TOK_KW_REQUIRE) {
        size_t line = p->current.line;
        size_t col  = p->current.column;
        curium_parser_v2_advance(p);
        curium_ast_v2_node_t* path = curium_parser_v2_parse_expr(p);
        curium_parser_v2_expect(p, CURIUM_TOK_SEMI, ";");
        curium_ast_v2_node_t* req = curium_ast_v2_new(CURIUM_AST_V2_IMPORT, line, col);
        req->as.let_decl.init = path; /* Repurposed init for legacy require expr */
        return req;
    }

    if (is_public) {
        curium_error_set(CURIUM_ERROR_PARSE, "Keyword 'pub' must precede a declaration (fn, let, mut, struct, union)");
        CURIUM_THROW(CURIUM_ERROR_PARSE, "Invalid use of 'pub'");
    }

    if (p->current.kind == CURIUM_TOK_KW_GC || p->current.kind == CURIUM_TOK_KW_PRINT) {
        /* Fall through to expression statement for these built-ins */
    } else {
        /* Control flow */
        if (p->current.kind == CURIUM_TOK_KW_IF)     return curium_parser_v2_parse_if(p);
        if (p->current.kind == CURIUM_TOK_KW_WHILE)  return curium_parser_v2_parse_while(p);
        if (p->current.kind == CURIUM_TOK_KW_FOR)    return curium_parser_v2_parse_for(p);
        if (p->current.kind == CURIUM_TOK_KW_MATCH)  return curium_parser_v2_parse_match(p);
        if (p->current.kind == CURIUM_TOK_KW_RETURN) return curium_parser_v2_parse_return(p);
        if (p->current.kind == CURIUM_TOK_KW_DYN)    return curium_parser_v2_parse_dyn(p);
        if (p->current.kind == CURIUM_TOK_KW_TRY)    return curium_parser_v2_parse_try(p);
        if (p->current.kind == CURIUM_TOK_KW_THROW)  return curium_parser_v2_parse_throw(p);

        /* v4.0: Reactor memory model blocks */
        if (p->current.kind == CURIUM_TOK_KW_REACTOR) {
            size_t rline = p->current.line;
            size_t rcol  = p->current.column;
            curium_parser_v2_advance(p); /* consume 'reactor' */

            curium_string_t* mode = curium_string_new(p->current.lexeme ? p->current.lexeme->data : "rc");
            curium_parser_v2_advance(p); /* consume 'arena' / 'manual' / 'rc' */

            /* Parse optional size: arena(4096) */
            curium_ast_v2_node_t* size_expr = NULL;
            if (curium_parser_v2_match(p, CURIUM_TOK_LPAREN)) {
                size_expr = curium_parser_v2_parse_expr(p);
                curium_parser_v2_expect(p, CURIUM_TOK_RPAREN, ")");
            }

            curium_parser_v2_expect(p, CURIUM_TOK_LBRACE, "{");

            curium_ast_v2_node_t* body = NULL;
            curium_ast_v2_node_t* body_tail = NULL;
            while (p->current.kind != CURIUM_TOK_RBRACE && p->current.kind != CURIUM_TOK_EOF) {
                curium_ast_v2_node_t* s = curium_parser_v2_parse_stmt(p);
                if (s) {
                    if (!body) { body = s; body_tail = s; }
                    else { body_tail->next = s; body_tail = s; }
                }
            }
            curium_parser_v2_expect(p, CURIUM_TOK_RBRACE, "}");

            curium_ast_v2_node_t* reactor = curium_ast_v2_new(CURIUM_AST_V2_REACTOR, rline, rcol);
            reactor->as.reactor_stmt.mode      = mode;
            reactor->as.reactor_stmt.size_expr  = size_expr;
            reactor->as.reactor_stmt.body       = body;
            return reactor;
        }
    }
    
    if (p->current.kind == CURIUM_TOK_KW_C) {
        size_t line = p->current.line;
        size_t col  = p->current.column;
        curium_parser_v2_advance(p);
        curium_parser_v2_expect(p, CURIUM_TOK_LBRACE, "{");

        /* FIX #004: raw character capture — read directly from the lexer's source
         * buffer instead of reassembling from tokens.  Token-based reconstruction
         * mangles `->`, drops `#`, collapses whitespace, and mis-quotes strings.
         *
         * Algorithm: after consuming the opening `{` (done by expect above),
         * record the current lexer position, then scan raw chars counting braces
         * until depth reaches zero.  Advance the lexer past the closing `}`.   */
        curium_string_t* code = curium_string_new("");
        {
            /* The lexer position is stored in p->lexer.pos; src in p->lexer.src. */
            size_t raw_start = p->lexer.pos;
            size_t raw_i     = raw_start;
            size_t raw_len   = p->lexer.length;
            int brace_depth  = 1;
            while (raw_i < raw_len && brace_depth > 0) {
                char ch = p->lexer.src[raw_i];
                if (ch == '{') brace_depth++;
                else if (ch == '}') {
                    brace_depth--;
                    if (brace_depth == 0) break; /* stop before the closing } */
                }
                /* Track newlines for line counter accuracy */
                if (ch == '\n') { p->lexer.line++; p->lexer.column = 0; }
                else            { p->lexer.column++;                     }
                raw_i++;
            }
            if (brace_depth != 0) {
                curium_string_free(code);
                CURIUM_THROW(CURIUM_ERROR_PARSE, "unclosed `c { ... }` block");
            }
            /* Capture the raw content (excluding the closing `}`) */
            size_t content_len = raw_i - raw_start;
            if (content_len > 0) {
                char* tmp = (char*)malloc(content_len + 1);
                if (tmp) {
                    memcpy(tmp, p->lexer.src + raw_start, content_len);
                    tmp[content_len] = '\0';
                    curium_string_set(code, tmp);
                    free(tmp);
                }
            }
            /* Advance lexer past the closing `}` and sync parser */
            p->lexer.pos = raw_i + 1; /* +1 to skip `}` */
            p->lexer.column++;
            /* Re-lex the next token so the parser's current token is correct */
            p->current = curium_lexer_v2_next_token(&p->lexer);
        }

        
        curium_ast_v2_node_t* poly = curium_ast_v2_new(CURIUM_AST_V2_POLYGLOT, line, col);
        poly->as.polyglot.code   = code;
        poly->as.polyglot.is_cpp = 0;
        return poly;
    }

    if (p->current.kind == CURIUM_TOK_KW_BREAK) {
        size_t line = p->current.line;
        size_t col  = p->current.column;
        curium_parser_v2_advance(p);
        curium_parser_v2_expect(p, CURIUM_TOK_SEMI, ";");
        return curium_ast_v2_new(CURIUM_AST_V2_BREAK, line, col);
    }

    if (p->current.kind == CURIUM_TOK_KW_CONTINUE) {
        size_t line = p->current.line;
        size_t col  = p->current.column;
        curium_parser_v2_advance(p);
        curium_parser_v2_expect(p, CURIUM_TOK_SEMI, ";");
        return curium_ast_v2_new(CURIUM_AST_V2_CONTINUE, line, col);
    }

    /* Expression or assignment statement */
    {
        size_t line = p->current.line;
        size_t col  = p->current.column;
        curium_ast_v2_node_t* expr = curium_parser_v2_parse_expr(p);

        if (p->current.kind == CURIUM_TOK_EQUAL || p->current.kind == CURIUM_TOK_COLON_EQUAL) {
            return curium_parser_v2_parse_assignment(p, expr);
        }

        curium_parser_v2_expect(p, CURIUM_TOK_SEMI, ";");

        curium_ast_v2_node_t* expr_stmt = curium_ast_v2_new(CURIUM_AST_V2_EXPR_STMT, line, col);
        expr_stmt->as.expr_stmt.expr = expr;
        return expr_stmt;
    }
}

/* ============================================================================
 * Module parser
 * ==========================================================================*/
static curium_ast_v2_list_t curium_parser_v2_parse(curium_parser_v2_t* p) {
    curium_ast_v2_list_t list = {0};

    while (p->current.kind != CURIUM_TOK_EOF) {
        CURIUM_TRY() {
            curium_ast_v2_node_t* stmt = curium_parser_v2_parse_stmt(p);
            if (stmt) {
                curium_ast_v2_list_append(&list, stmt);
            }
        } CURIUM_CATCH() {
            curium_parser_v2_synchronize(p);
        }
    }

    return list;
}

/* ============================================================================
 * Public high-level parsing interface
 * ==========================================================================*/
curium_ast_v2_list_t curium_parse_v2(const char* src) {
    /* Phase 2 DOD: allocate a session arena so every AST node lands in a
     * contiguous 64 KB slab.  Codegen then traverses hot data on the L2
     * Shelf instead of bouncing to the Fridge for each scattered node.
     *
     * The arena is heap-allocated here so it outlives this stack frame and
     * can be owned by the returned curium_ast_v2_list_t. */
    curium_ast_arena_t* session_arena = (curium_ast_arena_t*)malloc(sizeof(curium_ast_arena_t));
    if (session_arena) {
        curium_ast_arena_init(session_arena);
        curium_parse_arena = session_arena; /* engage arena path in curium_ast_v2_new() */
    } else {
        curium_parse_arena = NULL;          /* OOM: fall back to per-node malloc gracefully */
    }

    curium_parser_v2_t parser;
    curium_parser_v2_init(&parser, src);

    curium_ast_v2_list_t ast = {0};
    CURIUM_TRY() {
        ast = curium_parser_v2_parse(&parser);
    } CURIUM_CATCH() {
        /* FIX #013: snapshot had_error BEFORE destroy so we don't read freed/zeroed data */
        int caught_error = parser.had_error;
        curium_parse_arena = NULL;          /* clear global before any allocs in destroy */
        curium_parser_v2_destroy(&parser);
        curium_ast_v2_free_list(&ast);      /* frees arena if it was set on ast */
        if (session_arena && !ast.arena) {
            /* ast.arena wasn't transferred yet — destroy the session arena here */
            curium_ast_arena_destroy(session_arena);
            free(session_arena);
        }
        if (caught_error) {
            curium_error_set(CURIUM_ERROR_PARSE, "aborting due to previous parse errors");
        }
        return ast;
    }

    /* FIX #013: capture the flag BEFORE destroy() can touch the struct */
    int had_error = parser.had_error;

    /* Disengage the global before destroy so any allocs inside destroy use
     * the legacy path (the arena now belongs to ast, not future allocs). */
    curium_parse_arena = NULL;
    curium_parser_v2_destroy(&parser);

    /* Transfer arena ownership to the AST list.
     * curium_ast_v2_free_list() will call curium_arena_destroy() on it. */
    ast.arena = session_arena;

    if (had_error) {
        curium_ast_v2_free_list(&ast); /* also destroys ast.arena */
        curium_error_set(CURIUM_ERROR_PARSE, "aborting due to previous parse errors");
    }

    /* FIX #001: removed duplicate 'return ast;' that was dead code */
    return ast;
}
