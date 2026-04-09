#include "curium/curium_lang.h"
#include "curium/memory.h"
#include "curium/error.h"
#include "curium/file.h"
#include "curium/cmd.h"
#include "curium_codegen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifndef _WIN32
#include <dirent.h>
#include <sys/stat.h>
#else
/* On Windows, ship or depend on a dirent/stat compatibility layer if needed. */
#include <direct.h>
#include <io.h>
#include <sys/stat.h>
#endif

/* ============================================================================
 * Lexer
 * ==========================================================================*/

typedef struct {
    const char* src;
    size_t      length;
    size_t      pos;
    size_t      line;
    size_t      column;
} curium_lexer_t;

static int curium_lexer_peek(curium_lexer_t* lx) {
    if (lx->pos >= lx->length) return EOF;
    return (unsigned char)lx->src[lx->pos];
}

static int curium_lexer_next(curium_lexer_t* lx) {
    if (lx->pos >= lx->length) return EOF;
    int c = (unsigned char)lx->src[lx->pos++];
    if (c == '\n') {
        lx->line++;
        lx->column = 1;
    } else {
        lx->column++;
    }
    return c;
}

static void curium_lexer_init(curium_lexer_t* lx, const char* src) {
    memset(lx, 0, sizeof(*lx));
    lx->src    = src;
    lx->length = src ? strlen(src) : 0;
    lx->line   = 1;
    lx->column = 1;
}

static int curium_is_ident_start(int c) {
    return isalpha(c) || c == '_';
}

static int curium_is_ident_part(int c) {
    return isalnum(c) || c == '_';
}

static curium_token_t curium_make_token(curium_token_kind_t kind, const char* text, size_t len,
                                size_t line, size_t col) {
    curium_token_t tok;
    tok.kind   = kind;
    tok.lexeme = curium_string_new(NULL);
    if (text && len > 0) {
        char* tmp = (char*)malloc(len + 1);
        if (tmp) {
            memcpy(tmp, text, len);
            tmp[len] = '\0';
            curium_string_set(tok.lexeme, tmp);
            free(tmp);
        }
    }
    tok.line   = line;
    tok.column = col;
    return tok;
}

static void curium_token_free(curium_token_t* tok) {
    if (!tok) return;
    if (tok->lexeme) {
        curium_string_free(tok->lexeme);
        tok->lexeme = NULL;
    }
}

static void curium_lexer_skip_ws_and_comments(curium_lexer_t* lx) {
    int c;
    for (;;) {
        c = curium_lexer_peek(lx);
        if (isspace(c)) {
            curium_lexer_next(lx);
            continue;
        }
        if (c == '/' && lx->pos + 1 < lx->length && lx->src[lx->pos + 1] == '/') {
            while ((c = curium_lexer_next(lx)) != EOF && c != '\n') {}
            continue;
        }
        break;
    }
}

static curium_token_t curium_lexer_next_token(curium_lexer_t* lx) {
    curium_lexer_skip_ws_and_comments(lx);
    size_t start_pos = lx->pos;
    size_t start_line = lx->line;
    size_t start_col  = lx->column;

    int c = curium_lexer_next(lx);
    if (c == EOF) {
        return curium_make_token(CURIUM_TOK_EOF, NULL, 0, lx->line, lx->column);
    }

    switch (c) {
        case '(': return curium_make_token(CURIUM_TOK_LPAREN, "(", 1, start_line, start_col);
        case ')': return curium_make_token(CURIUM_TOK_RPAREN, ")", 1, start_line, start_col);
        case '{': return curium_make_token(CURIUM_TOK_LBRACE, "{", 1, start_line, start_col);
        case '}': return curium_make_token(CURIUM_TOK_RBRACE, "}", 1, start_line, start_col);
        case '[': return curium_make_token(CURIUM_TOK_LBRACKET, "[", 1, start_line, start_col);
        case ']': return curium_make_token(CURIUM_TOK_RBRACKET, "]", 1, start_line, start_col);
        case ';': return curium_make_token(CURIUM_TOK_SEMI,   ";", 1, start_line, start_col);
        case ',': return curium_make_token(CURIUM_TOK_COMMA,  ",", 1, start_line, start_col);
        case ':': return curium_make_token(CURIUM_TOK_COLON,  ":", 1, start_line, start_col);
        case '=':
            if (curium_lexer_peek(lx) == '=') { curium_lexer_next(lx); return curium_make_token(CURIUM_TOK_EQUAL, "==", 2, start_line, start_col); }
            if (curium_lexer_peek(lx) == '>') { curium_lexer_next(lx); return curium_make_token(CURIUM_TOK_FAT_ARROW, "=>", 2, start_line, start_col); }
            return curium_make_token(CURIUM_TOK_EQUAL,  "=", 1, start_line, start_col);
        case '+': return curium_make_token(CURIUM_TOK_PLUS,   "+", 1, start_line, start_col);
        case '-':
            if (curium_lexer_peek(lx) == '>') { curium_lexer_next(lx); return curium_make_token(CURIUM_TOK_ARROW, "->", 2, start_line, start_col); }
            return curium_make_token(CURIUM_TOK_MINUS,  "-", 1, start_line, start_col);
        case '*': return curium_make_token(CURIUM_TOK_STAR,   "*", 1, start_line, start_col);
        case '/': return curium_make_token(CURIUM_TOK_SLASH,  "/", 1, start_line, start_col);
        case '.': return curium_make_token(CURIUM_TOK_DOT,    ".", 1, start_line, start_col);
        case '<': return curium_make_token(CURIUM_TOK_LT,     "<", 1, start_line, start_col);
        case '>': return curium_make_token(CURIUM_TOK_GT,     ">", 1, start_line, start_col);
        case '!': return curium_make_token(CURIUM_TOK_BANG,   "!", 1, start_line, start_col);
        case '&': return curium_make_token(CURIUM_TOK_AMPERSAND, "&", 1, start_line, start_col);
        case '|': return curium_make_token(CURIUM_TOK_PIPE,   "|", 1, start_line, start_col);
        case '"': {
            size_t str_start = lx->pos;
            while (1) {
                int ch = curium_lexer_next(lx);
                if (ch == EOF || ch == '\n') break;
                if (ch == '"') break;
                if (ch == '\\') {
                    curium_lexer_next(lx);
                }
            }
            size_t str_end = lx->pos;
            size_t len = (str_end > str_start) ? (str_end - str_start - 1) : 0;
            const char* base = lx->src + str_start;
            return curium_make_token(CURIUM_TOK_STRING_LITERAL, base, len, start_line, start_col);
        }
        default:
            break;
    }

    if (isdigit(c)) {
        while (isdigit(curium_lexer_peek(lx))) {
            curium_lexer_next(lx);
        }
        size_t end = lx->pos;
        return curium_make_token(CURIUM_TOK_NUMBER, lx->src + start_pos, end - start_pos,
                             start_line, start_col);
    }

    if (curium_is_ident_start(c)) {
        while (curium_is_ident_part(curium_lexer_peek(lx))) {
            curium_lexer_next(lx);
        }
        size_t end = lx->pos;
        size_t len = end - start_pos;
        const char* ident = lx->src + start_pos;

        if (len == 6 && strncmp(ident, "string", 6) == 0) {
            return curium_make_token(CURIUM_TOK_KW_STRING, ident, len, start_line, start_col);
        }
        if (len == 9 && strncmp(ident, "namespace", 9) == 0) {
            return curium_make_token(CURIUM_TOK_KW_NAMESPACE, ident, len, start_line, start_col);
        }
        if (len == 5 && strncmp(ident, "using", 5) == 0) {
            return curium_make_token(CURIUM_TOK_KW_USING, ident, len, start_line, start_col);
        }
        if (len == 6 && strncmp(ident, "public", 6) == 0) {
            return curium_make_token(CURIUM_TOK_KW_PUBLIC, ident, len, start_line, start_col);
        }
        if (len == 7 && strncmp(ident, "private", 7) == 0) {
            return curium_make_token(CURIUM_TOK_KW_PRIVATE, ident, len, start_line, start_col);
        }
        if (len == 6 && strncmp(ident, "static", 6) == 0) {
            return curium_make_token(CURIUM_TOK_KW_STATIC, ident, len, start_line, start_col);
        }
        if (len == 5 && strncmp(ident, "async", 5) == 0) {
            return curium_make_token(CURIUM_TOK_KW_ASYNC, ident, len, start_line, start_col);
        }
        if (len == 5 && strncmp(ident, "await", 5) == 0) {
            return curium_make_token(CURIUM_TOK_KW_AWAIT, ident, len, start_line, start_col);
        }
        if (len == 4 && strncmp(ident, "task", 4) == 0) {
            return curium_make_token(CURIUM_TOK_KW_TASK, ident, len, start_line, start_col);
        }
        if (len == 3 && strncmp(ident, "try", 3) == 0) {
            return curium_make_token(CURIUM_TOK_KW_TRY, ident, len, start_line, start_col);
        }
        if (len == 5 && strncmp(ident, "catch", 5) == 0) {
            return curium_make_token(CURIUM_TOK_KW_CATCH, ident, len, start_line, start_col);
        }
        if (len == 7 && strncmp(ident, "finally", 7) == 0) {
            return curium_make_token(CURIUM_TOK_KW_FINALLY, ident, len, start_line, start_col);
        }
        if (len == 5 && strncmp(ident, "throw", 5) == 0) {
            return curium_make_token(CURIUM_TOK_KW_THROW, ident, len, start_line, start_col);
        }
        if (len == 3 && strncmp(ident, "get", 3) == 0) {
            return curium_make_token(CURIUM_TOK_KW_GET, ident, len, start_line, start_col);
        }
        if (len == 3 && strncmp(ident, "set", 3) == 0) {
            return curium_make_token(CURIUM_TOK_KW_SET, ident, len, start_line, start_col);
        }
        if (len == 4 && strncmp(ident, "void", 4) == 0) {
            return curium_make_token(CURIUM_TOK_KW_VOID, ident, len, start_line, start_col);
        }
        if (len == 3 && strncmp(ident, "int", 3) == 0) {
            return curium_make_token(CURIUM_TOK_KW_INT, ident, len, start_line, start_col);
        }
        if (len == 4 && strncmp(ident, "bool", 4) == 0) {
            return curium_make_token(CURIUM_TOK_KW_BOOL, ident, len, start_line, start_col);
        }
        if (len == 5 && strncmp(ident, "float", 5) == 0) {
            return curium_make_token(CURIUM_TOK_KW_FLOAT, ident, len, start_line, start_col);
        }
        if (len == 6 && strncmp(ident, "double", 6) == 0) {
            return curium_make_token(CURIUM_TOK_KW_DOUBLE, ident, len, start_line, start_col);
        }
        if (len == 4 && strncmp(ident, "true", 4) == 0) {
            return curium_make_token(CURIUM_TOK_KW_TRUE, ident, len, start_line, start_col);
        }
        if (len == 5 && strncmp(ident, "false", 5) == 0) {
            return curium_make_token(CURIUM_TOK_KW_FALSE, ident, len, start_line, start_col);
        }
        if (len == 4 && strncmp(ident, "null", 4) == 0) {
            return curium_make_token(CURIUM_TOK_KW_NULL, ident, len, start_line, start_col);
        }
        if (len == 6 && strncmp(ident, "import", 6) == 0) {
            return curium_make_token(CURIUM_TOK_KW_IMPORT, ident, len, start_line, start_col);
        }
        if (len == 4 && strncmp(ident, "from", 4) == 0) {
            return curium_make_token(CURIUM_TOK_KW_FROM, ident, len, start_line, start_col);
        }
        if (len == 7 && strncmp(ident, "package", 7) == 0) {
            return curium_make_token(CURIUM_TOK_KW_PACKAGE, ident, len, start_line, start_col);
        }
        if (len == 9 && strncmp(ident, "interface", 9) == 0) {
            return curium_make_token(CURIUM_TOK_KW_INTERFACE, ident, len, start_line, start_col);
        }
        if (len == 10 && strncmp(ident, "implements", 10) == 0) {
            return curium_make_token(CURIUM_TOK_KW_IMPLEMENTS, ident, len, start_line, start_col);
        }
        if (len == 7 && strncmp(ident, "extends", 7) == 0) {
            return curium_make_token(CURIUM_TOK_KW_EXTENDS, ident, len, start_line, start_col);
        }
        if (len == 5 && strncmp(ident, "input", 5) == 0) {
            return curium_make_token(CURIUM_TOK_KW_INPUT, ident, len, start_line, start_col);
        }
        if (len == 7 && strncmp(ident, "require", 7) == 0) {
            return curium_make_token(CURIUM_TOK_KW_REQUIRE, ident, len, start_line, start_col);
        }
        if (len == 2 && strncmp(ident, "if", 2) == 0) {
            return curium_make_token(CURIUM_TOK_KW_IF, ident, len, start_line, start_col);
        }
        if (len == 4 && strncmp(ident, "else", 4) == 0) {
            return curium_make_token(CURIUM_TOK_KW_ELSE, ident, len, start_line, start_col);
        }
        if (len == 5 && strncmp(ident, "while", 5) == 0) {
            return curium_make_token(CURIUM_TOK_KW_WHILE, ident, len, start_line, start_col);
        }
        if (len == 6 && strncmp(ident, "return", 6) == 0) {
            return curium_make_token(CURIUM_TOK_KW_RETURN, ident, len, start_line, start_col);
        }
        if (len == 3 && strncmp(ident, "map", 3) == 0) {
            return curium_make_token(CURIUM_TOK_KW_MAP, ident, len, start_line, start_col);
        }
        if (len == 2 && strncmp(ident, "gc", 2) == 0) {
            return curium_make_token(CURIUM_TOK_KW_GC, ident, len, start_line, start_col);
        }
        if (len == 4 && strncmp(ident, "list", 4) == 0) {
            return curium_make_token(CURIUM_TOK_KW_LIST, ident, len, start_line, start_col);
        }
        if (len == 5 && strncmp(ident, "array", 5) == 0) {
            return curium_make_token(CURIUM_TOK_KW_ARRAY, ident, len, start_line, start_col);
        }
        if (len == 5 && strncmp(ident, "class", 5) == 0) {
            return curium_make_token(CURIUM_TOK_KW_CLASS, ident, len, start_line, start_col);
        }
        if (len == 3 && strncmp(ident, "new", 3) == 0) {
            return curium_make_token(CURIUM_TOK_KW_NEW, ident, len, start_line, start_col);
        }
        if (len == 3 && strncmp(ident, "ptr", 3) == 0) {
            return curium_make_token(CURIUM_TOK_KW_PTR, ident, len, start_line, start_col);
        }
        if (len == 3 && strncmp(ident, "str", 3) == 0) {
            return curium_make_token(CURIUM_TOK_KW_STR, ident, len, start_line, start_col);
        }
        if (len == 6 && strncmp(ident, "malloc", 6) == 0) {
            return curium_make_token(CURIUM_TOK_KW_MALLOC, ident, len, start_line, start_col);
        }
        if (len == 4 && strncmp(ident, "free", 4) == 0) {
            return curium_make_token(CURIUM_TOK_KW_FREE, ident, len, start_line, start_col);
        }
        if (len == 10 && strncmp(ident, "gc_collect", 10) == 0) {
            return curium_make_token(CURIUM_TOK_KW_GC_COLLECT, ident, len, start_line, start_col);
        }
        if (len == 3 && ident[0] == 'c' && ident[1] == 'p' && ident[2] == 'p') {
            curium_lexer_skip_ws_and_comments(lx);
            int brace = curium_lexer_next(lx);
            if (brace != '{') {
                return curium_make_token(CURIUM_TOK_IDENTIFIER, ident, len, start_line, start_col);
            }
            size_t code_start = lx->pos;
            int depth = 1;
            while (lx->pos < lx->length && depth > 0) {
                int ch = curium_lexer_next(lx);
                if (ch == '{') depth++;
                else if (ch == '}') depth--;
            }
            size_t code_end = (depth == 0) ? (lx->pos - 1) : lx->pos;
            if (code_end < code_start) code_end = code_start;
            size_t code_len = code_end - code_start;
            return curium_make_token(CURIUM_TOK_CPP_BLOCK, lx->src + code_start, code_len,
                                 start_line, start_col);
        }
        if (len == 1 && ident[0] == 'c') {
            curium_lexer_skip_ws_and_comments(lx);
            int brace = curium_lexer_next(lx);
            if (brace != '{') {
                return curium_make_token(CURIUM_TOK_IDENTIFIER, ident, len, start_line, start_col);
            }
            size_t code_start = lx->pos;
            int depth = 1;
            while (lx->pos < lx->length && depth > 0) {
                int ch = curium_lexer_next(lx);
                if (ch == '{') depth++;
                else if (ch == '}') depth--;
            }
            size_t code_end = (depth == 0) ? (lx->pos - 1) : lx->pos;
            if (code_end < code_start) code_end = code_start;
            size_t code_len = code_end - code_start;
            return curium_make_token(CURIUM_TOK_C_BLOCK, lx->src + code_start, code_len,
                                 start_line, start_col);
        }

        return curium_make_token(CURIUM_TOK_IDENTIFIER, ident, len, start_line, start_col);
    }

    char ch = (char)c;
    return curium_make_token(CURIUM_TOK_IDENTIFIER, &ch, 1, start_line, start_col);
}

/* ============================================================================
 * AST
 * ==========================================================================*/

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

typedef enum {
    CURIUM_POLY_C,
    CURIUM_POLY_CPP
} curium_poly_kind_t;

typedef struct curium_ast_node curium_ast_node_t;

struct curium_ast_node {
    curium_ast_kind_t kind;
    size_t        line;
    size_t        column;

    union {
        struct {
            curium_string_t* path;
        } require_stmt;

        struct {
            curium_string_t* type_name;
            curium_string_t* var_name;
            curium_string_t* init_expr;
        } var_decl;

        struct {
            curium_string_t* expr_text;
        } expr_stmt;

        struct {
            curium_poly_kind_t lang;
            curium_string_t*   code;
        } poly_block;

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

struct curium_ast_list {
    curium_ast_node_t* head;
    curium_ast_node_t* tail;
};

static curium_ast_node_t* curium_ast_new(curium_ast_kind_t kind, size_t line, size_t col) {
    curium_ast_node_t* n = (curium_ast_node_t*)curium_alloc(sizeof(curium_ast_node_t), "curium_ast_node");
    if (!n) return NULL;
    memset(n, 0, sizeof(*n));
    n->kind   = kind;
    n->line   = line;
    n->column = col;
    return n;
}

static void curium_ast_list_append(curium_ast_list_t* list, curium_ast_node_t* n) {
    if (!list || !n) return;
    if (!list->head) {
        list->head = list->tail = n;
    } else {
        list->tail->next = n;
        list->tail = n;
    }
}

static void curium_ast_free_list(curium_ast_list_t* list) {
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
                if (cur->as.namespace_decl.name) curium_string_free(cur->as.namespace_decl.name);
                break;
            case CURIUM_AST_CLASS:
                if (cur->as.class_decl.name) curium_string_free(cur->as.class_decl.name);
                if (cur->as.class_decl.parent_class) curium_string_free(cur->as.class_decl.parent_class);
                break;
            case CURIUM_AST_METHOD:
                if (cur->as.method_decl.name) curium_string_free(cur->as.method_decl.name);
                if (cur->as.method_decl.return_type) curium_string_free(cur->as.method_decl.return_type);
                break;
            case CURIUM_AST_PROPERTY:
                if (cur->as.property_decl.name) curium_string_free(cur->as.property_decl.name);
                if (cur->as.property_decl.type) curium_string_free(cur->as.property_decl.type);
                break;
            case CURIUM_AST_IMPORT:
                if (cur->as.import_stmt.module) curium_string_free(cur->as.import_stmt.module);
                if (cur->as.import_stmt.symbol) curium_string_free(cur->as.import_stmt.symbol);
                break;
            case CURIUM_AST_TRY_CATCH:
                if (cur->as.try_catch.catch_var) curium_string_free(cur->as.try_catch.catch_var);
                break;
            case CURIUM_AST_THROW:
                if (cur->as.throw_stmt.expr) curium_string_free(cur->as.throw_stmt.expr);
                break;
            case CURIUM_AST_ATTRIBUTE:
                if (cur->as.attribute_decl.name) curium_string_free(cur->as.attribute_decl.name);
                break;
            case CURIUM_AST_INTERFACE:
                if (cur->as.interface_decl.name) curium_string_free(cur->as.interface_decl.name);
                break;
            case CURIUM_AST_IMPLEMENTS:
                if (cur->as.implements_decl.base) curium_string_free(cur->as.implements_decl.base);
                break;
            case CURIUM_AST_GENERIC_TYPE:
                if (cur->as.generic_type.name) curium_string_free(cur->as.generic_type.name);
                break;
            default:
                break;
        }
        curium_free(cur);
        cur = next;
    }
    list->head = list->tail = NULL;
}

/* ============================================================================
 * Parser
 * ==========================================================================*/

typedef struct {
    curium_lexer_t   lexer;
    curium_token_t   current;
} curium_parser_t;

static void curium_parser_init(curium_parser_t* p, const char* src) {
    curium_lexer_init(&p->lexer, src);
    p->current  = curium_lexer_next_token(&p->lexer);
}

static void curium_parser_advance(curium_parser_t* p) {
    curium_token_free(&p->current);
    p->current = curium_lexer_next_token(&p->lexer);
}

static void curium_parser_destroy(curium_parser_t* p) {
    if (!p) return;
    curium_token_free(&p->current);
}

static void curium_expr_append_token(curium_string_t* out, const curium_token_t* tok) {
    if (!out || !tok || !tok->lexeme) return;
    const curium_token_kind_t k = tok->kind;
    const char* s = tok->lexeme->data ? tok->lexeme->data : "";

    /* Avoid injecting spaces around punctuation so desugaring can match. */
    const int is_word =
        (k == CURIUM_TOK_IDENTIFIER) || (k == CURIUM_TOK_NUMBER) || (k == CURIUM_TOK_STRING_LITERAL) ||
        (k == CURIUM_TOK_KW_REQUIRE) || (k == CURIUM_TOK_KW_INPUT) || (k == CURIUM_TOK_KW_STRING);

    if (is_word && out->length > 0) {
        char last = out->data[out->length - 1];
        if (isalnum((unsigned char)last) || last == '_' || last == '"' ) {
            curium_string_append(out, " ");
        }
    }

    if (k == CURIUM_TOK_STRING_LITERAL) {
        curium_string_append(out, "\"");
    }
    curium_string_append(out, s);
    if (k == CURIUM_TOK_STRING_LITERAL) {
        curium_string_append(out, "\"");
    }

    /* Post-fix spacing: keep '.' and '(' tight by not adding spaces at all here.
       Also avoid leaving a space before '.' due to previous word spacing. */
    if (strcmp(s, ".") == 0 && out->length >= 2 && out->data[out->length - 2] == ' ') {
        /* Remove the space before the dot. */
        out->data[out->length - 2] = '.';
        out->data[out->length - 1] = '\0';
        out->length -= 1;
    }
}

static int curium_parser_match(curium_parser_t* p, curium_token_kind_t kind) {
    if (p->current.kind == kind) {
        curium_parser_advance(p);
        return 1;
    }
    return 0;
}

static void curium_parser_expect(curium_parser_t* p, curium_token_kind_t kind, const char* what) {
    if (!curium_parser_match(p, kind)) {
        curium_error_set(CURIUM_ERROR_PARSE, what);
        CURIUM_THROW(CURIUM_ERROR_PARSE, what);
    }
}

static curium_ast_node_t* curium_parse_require(curium_parser_t* p) {
    size_t line = p->current.line;
    size_t col  = p->current.column;
    curium_parser_advance(p);
    curium_parser_expect(p, CURIUM_TOK_LPAREN, "expected '(' after require");
    if (p->current.kind != CURIUM_TOK_STRING_LITERAL) {
        curium_error_set(CURIUM_ERROR_PARSE, "expected string literal path in require()");
        CURIUM_THROW(CURIUM_ERROR_PARSE, "expected string literal path in require()");
    }
    curium_string_t* path = curium_string_new(p->current.lexeme->data);
    curium_parser_advance(p);
    curium_parser_expect(p, CURIUM_TOK_RPAREN, "expected ')' after require path");
    curium_parser_expect(p, CURIUM_TOK_SEMI, "expected ';' after require()");

    curium_ast_node_t* n = curium_ast_new(CURIUM_AST_REQUIRE, line, col);
    if (!n) return NULL;
    n->as.require_stmt.path = path;
    return n;
}

static curium_ast_node_t* curium_parse_simple_stmt(curium_parser_t* p) {
    /* Check for type keywords: string/str, ptr, list, array, map */
    curium_token_kind_t type_kind = p->current.kind;
    const char* type_name = NULL;
    
    if (type_kind == CURIUM_TOK_KW_STRING) type_name = "string";
    else if (type_kind == CURIUM_TOK_KW_STR) type_name = "str";
    else if (type_kind == CURIUM_TOK_KW_PTR) type_name = "ptr";
    else if (type_kind == CURIUM_TOK_KW_LIST) type_name = "list";
    else if (type_kind == CURIUM_TOK_KW_ARRAY) type_name = "array";
    else if (type_kind == CURIUM_TOK_KW_MAP) type_name = "map";
    
    if (type_name) {
        size_t line = p->current.line;
        size_t col  = p->current.column;
        curium_parser_advance(p);
        
        if (type_kind == CURIUM_TOK_KW_PTR) {
            curium_token_kind_t nk = p->current.kind;
            if (nk == CURIUM_TOK_KW_STRING || nk == CURIUM_TOK_KW_STR || nk == CURIUM_TOK_KW_LIST || nk == CURIUM_TOK_KW_ARRAY || nk == CURIUM_TOK_KW_MAP || nk == CURIUM_TOK_IDENTIFIER) {
                curium_parser_advance(p);
            }
        }

        if (p->current.kind != CURIUM_TOK_IDENTIFIER) {
            curium_error_set(CURIUM_ERROR_PARSE, "expected identifier after 'string'");
            CURIUM_THROW(CURIUM_ERROR_PARSE, "expected identifier after 'string'");
        }
        curium_string_t* var = curium_string_new(p->current.lexeme->data);
        curium_parser_advance(p);
        curium_parser_expect(p, CURIUM_TOK_EQUAL, "expected '=' after variable name");

        curium_string_t* expr = curium_string_new("");
        size_t expr_start_line = p->current.line;
        (void)expr_start_line;
        while (p->current.kind != CURIUM_TOK_SEMI && p->current.kind != CURIUM_TOK_EOF) {
            curium_expr_append_token(expr, &p->current);
            curium_parser_advance(p);
        }
        curium_parser_expect(p, CURIUM_TOK_SEMI, "expected ';' after expression");

        curium_ast_node_t* n = curium_ast_new(CURIUM_AST_VAR_DECL, line, col);
        if (!n) {
            curium_string_free(var);
            curium_string_free(expr);
            return NULL;
        }
        n->as.var_decl.type_name = curium_string_new(type_name);
        n->as.var_decl.var_name  = var;
        n->as.var_decl.init_expr = expr;
        return n;
    }

    if (p->current.kind == CURIUM_TOK_CPP_BLOCK) {
        curium_ast_node_t* n = curium_ast_new(CURIUM_AST_POLYGLOT, p->current.line, p->current.column);
        if (!n) return NULL;
        n->as.poly_block.lang = CURIUM_POLY_CPP;
        n->as.poly_block.code = curium_string_new(p->current.lexeme ? p->current.lexeme->data : "");
        curium_parser_advance(p);
        return n;
    }

    if (p->current.kind == CURIUM_TOK_C_BLOCK) {
        curium_ast_node_t* n = curium_ast_new(CURIUM_AST_POLYGLOT, p->current.line, p->current.column);
        if (!n) return NULL;
        n->as.poly_block.lang = CURIUM_POLY_C;
        n->as.poly_block.code = curium_string_new(p->current.lexeme ? p->current.lexeme->data : "");
        curium_parser_advance(p);
        return n;
    }

    size_t line = p->current.line;
    size_t col  = p->current.column;
    curium_string_t* expr = curium_string_new("");
    
    int brace_depth = 0;
    int is_block = 0;

    while (p->current.kind != CURIUM_TOK_EOF) {
        if (p->current.kind == CURIUM_TOK_LBRACE) {
            brace_depth++;
            is_block = 1;
        } else if (p->current.kind == CURIUM_TOK_RBRACE) {
            brace_depth--;
        }

        curium_expr_append_token(expr, &p->current);
        curium_parser_advance(p);

        if (brace_depth == 0) {
            if (is_block) {
                break;
            } else if (p->current.kind == CURIUM_TOK_SEMI) {
                break;
            }
        }
    }
    if (!is_block && p->current.kind == CURIUM_TOK_SEMI) {
        curium_parser_advance(p);
    }

    curium_ast_node_t* n = curium_ast_new(CURIUM_AST_EXPR_STMT, line, col);
    if (!n) {
        curium_string_free(expr);
        return NULL;
    }
    n->as.expr_stmt.expr_text = expr;

    return n;
}

static curium_ast_list_t curium_parser_parse(curium_parser_t* p) {
    curium_ast_list_t list = {0};
    while (p->current.kind != CURIUM_TOK_EOF) {
        if (p->current.kind == CURIUM_TOK_KW_REQUIRE) {
            curium_ast_node_t* req = curium_parse_require(p);
            curium_ast_list_append(&list, req);
            continue;
        }
        curium_ast_node_t* stmt = curium_parse_simple_stmt(p);
        if (stmt) curium_ast_list_append(&list, stmt);
    }
    return list;
}

/* ============================================================================
 * Directory / module loading helpers
 * ==========================================================================*/

static int curium_is_directory(const char* path) {
#ifndef _WIN32
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return S_ISDIR(st.st_mode);
#else
    struct _stat st;
    if (_stat(path, &st) != 0) return 0;
    return (st.st_mode & _S_IFDIR) != 0;
#endif
}

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

static void curium_scan_directory_for_cm(const char* dir, curium_module_list_t* out) {
    if (!dir || !out) return;
#ifndef _WIN32
    DIR* d = opendir(dir);
    if (!d) return;
    struct dirent* ent;
    while ((ent = readdir(d)) != NULL) {
        const char* name = ent->d_name;
        size_t len = strlen(name);
        if (len > 3 && strcmp(name + len - 3, ".cm") == 0) {
            char full[1024];
            snprintf(full, sizeof(full), "%s/%s", dir, name);
            curium_module_list_append(out, full);
        }
    }
    closedir(d);
#else
    char pattern[1024];
    snprintf(pattern, sizeof(pattern), "%s\\*.cm", dir);
    struct _finddata_t data;
    intptr_t handle = _findfirst(pattern, &data);
    if (handle == -1L) return;
    do {
        char full[1024];
        snprintf(full, sizeof(full), "%s\\%s", dir, data.name);
        curium_module_list_append(out, full);
    } while (_findnext(handle, &data) == 0);
    _findclose(handle);
#endif
}

/* ============================================================================
 * Transpiler / Codegen: CM AST -> Hardened C
 * ==========================================================================*/

/* Builtin blacklist for unsafe C APIs in CM code (not polyglot blocks). */
static const char* CURIUM_BLACKLISTED_FUNCS[] = {
    "strcpy", "strcat", "sprintf", "snprintf",
    "scanf", "gets", "fgets",
    /* Note: malloc/free are allowed - they get converted to curium_alloc/curium_free */
    "printf",
    NULL
};

/* Check if 'word' appears as a complete word in 'text' (not as substring) */
static int curium_contains_word(const char* text, const char* word) {
    if (!text || !word) return 0;
    size_t word_len = strlen(word);
    if (word_len == 0) return 0;
    
    const char* p = text;
    while ((p = strstr(p, word)) != NULL) {
        /* Check if preceded by word boundary (start or non-alnum/_ char) */
        int prev_ok = (p == text) || !isalnum((unsigned char)p[-1]) && p[-1] != '_';
        /* Check if followed by word boundary (end or non-alnum/_ char or '(') */
        int next_ok = !isalnum((unsigned char)p[word_len]) && p[word_len] != '_';
        
        if (prev_ok && next_ok) {
            return 1;
        }
        p += word_len;
    }
    return 0;
}

static void curium_check_blacklist_string(const curium_string_t* s) {
    if (!s || !s->data) return;
    for (const char* const* p = CURIUM_BLACKLISTED_FUNCS; *p; ++p) {
        if (curium_contains_word(s->data, *p)) {
            curium_string_t* msg = curium_string_format(
                "[CM Compiler] use of banned function '%s' detected. "
                "Use CM safe alternatives (curium_string_*, curium_alloc, curium_map_*, print/input) instead.",
                *p);
            curium_error_set(CURIUM_ERROR_TYPE, msg ? msg->data : "banned function use");
            if (msg) curium_string_free(msg);
            CURIUM_THROW(CURIUM_ERROR_TYPE, "banned function use");
        }
    }
}

/* Walk AST and apply blacklist checks to CM expressions (not polyglot blocks). */
static void curium_check_blacklist_ast(curium_ast_list_t* ast) {
    if (!ast) return;
    for (curium_ast_node_t* n = ast->head; n; n = n->next) {
        switch (n->kind) {
            case CURIUM_AST_VAR_DECL:
                curium_check_blacklist_string(n->as.var_decl.init_expr);
                break;
            case CURIUM_AST_EXPR_STMT:
                curium_check_blacklist_string(n->as.expr_stmt.expr_text);
                break;
            case CURIUM_AST_REQUIRE:
                curium_check_blacklist_string(n->as.require_stmt.path);
                break;
            case CURIUM_AST_POLYGLOT:
                /* Polyglot blocks are passed through verbatim by design. */
                break;
        }
    }
}

static curium_string_t* curium_transpile_ast_to_c(curium_ast_list_t* ast) {
    return curium_codegen_to_c((const curium_ast_list_t*)ast);
}

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
        buf = NULL;
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

static int curium_collect_require_modules(curium_ast_list_t* ast, curium_module_list_t* modules) {
    if (!ast || !modules) return 0;
    for (curium_ast_node_t* n = ast->head; n; n = n->next) {
        if (n->kind != CURIUM_AST_REQUIRE) continue;
        const char* path = n->as.require_stmt.path->data;
        if (curium_is_directory(path)) {
            curium_scan_directory_for_cm(path, modules);
            continue;
        }

        /* Try the exact path first. */
        curium_module_list_append(modules, path);

        /* Also attempt resolution in standard search locations: src/ and include/cm/. */
        char buf[1024];
        snprintf(buf, sizeof(buf), "src/%s", path);
        curium_module_list_append(modules, buf);
        snprintf(buf, sizeof(buf), "include/cm/%s", path);
        curium_module_list_append(modules, buf);
    }
    return 0;
}

static int curium_invoke_system_compiler(const char* c_path, const char* output_exe) {
    if (!c_path || !output_exe) return -1;

    /* Enterprise default: compile generated C together with the CM runtime sources,
       so `cm main.cm app` works even without a pre-built libcm present. */
    const char* candidates[] = { "tcc", "gcc", "clang", "cc", NULL };
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
    
    extern int curium_opt_release;
    extern int curium_opt_debug;
    
    if (curium_opt_release) {
        curium_cmd_arg(cmd, "-O3");
    } else if (curium_opt_debug) {
        curium_cmd_arg(cmd, "-g");
        #ifndef _WIN32
        curium_cmd_arg(cmd, "-fsanitize=address");
        #endif
    }

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

int curium_compile_file(const char* entry_path, const char* output_exe) {
    if (!entry_path || !output_exe) return -1;

    char* src = curium_read_file_all(entry_path);
    if (!src) {
        curium_error_set(CURIUM_ERROR_IO, "failed to read entry .cm file");
        return -1;
    }

    curium_parser_t parser;
    curium_parser_init(&parser, src);

    curium_ast_list_t ast = {0};
    CURIUM_TRY() {
        ast = curium_parser_parse(&parser);
        /* Enforce safety & blacklist policy after parsing. */
        curium_check_blacklist_ast(&ast);
    } CURIUM_CATCH() {
        curium_parser_destroy(&parser);
        free(src);
        curium_ast_free_list(&ast);
        return -1;
    }
    curium_parser_destroy(&parser);

    curium_module_list_t modules;
    curium_module_list_init(&modules);
    curium_collect_require_modules(&ast, &modules);

    curium_string_t* c_code = curium_transpile_ast_to_c(&ast);

    const char* c_path = "curium_out.c";
    if (curium_write_text_file(c_path, c_code->data) != 0) {
        curium_string_free(c_code);
        curium_module_list_free(&modules);
        curium_ast_free_list(&ast);
        free(src);
        curium_error_set(CURIUM_ERROR_IO, "failed to write intermediate C file");
        return -1;
    }

    curium_try_remove_output(output_exe);
    int rc = curium_invoke_system_compiler(c_path, output_exe);

    curium_string_free(c_code);
    curium_module_list_free(&modules);
    curium_ast_free_list(&ast);
    free(src);

    return rc;
}

int curium_emit_c_file(const char* entry_path, const char* output_c_path) {
    if (!entry_path || !output_c_path) return -1;

    char* src = curium_read_file_all(entry_path);
    if (!src) {
        curium_error_set(CURIUM_ERROR_IO, "failed to read entry .cm file");
        return -1;
    }

    curium_parser_t parser;
    curium_parser_init(&parser, src);

    curium_ast_list_t ast = {0};
    CURIUM_TRY() {
        ast = curium_parser_parse(&parser);
        curium_check_blacklist_ast(&ast);
    } CURIUM_CATCH() {
        curium_parser_destroy(&parser);
        free(src);
        curium_ast_free_list(&ast);
        return -1;
    }
    curium_parser_destroy(&parser);

    curium_string_t* c_code = curium_transpile_ast_to_c(&ast);
    if (curium_write_text_file(output_c_path, c_code->data) != 0) {
        curium_string_free(c_code);
        curium_ast_free_list(&ast);
        free(src);
        curium_error_set(CURIUM_ERROR_IO, "failed to write generated C file");
        return -1;
    }

    curium_string_free(c_code);
    curium_ast_free_list(&ast);
    free(src);
    return 0;
}

