#include "curium/compiler/lexer.h"
#include "curium/memory.h"
#include "curium/error.h"
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

/* ============================================================================
 * CM v2 Lexer - Supports new simplified syntax
 * ==========================================================================*/

static int curium_lexer_v2_peek(curium_lexer_t* lx) {
    if (lx->pos >= lx->length) return EOF;
    return (unsigned char)lx->src[lx->pos];
}

static int curium_lexer_v2_next(curium_lexer_t* lx) {
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

void curium_lexer_v2_init(curium_lexer_t* lx, const char* src) {
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

static void curium_lexer_v2_skip_ws_and_comments(curium_lexer_t* lx) {
    int c;
    for (;;) {
        c = curium_lexer_v2_peek(lx);
        if (isspace(c)) {
            curium_lexer_v2_next(lx);
            continue;
        }
        if (c == '/' && lx->pos + 1 < lx->length && lx->src[lx->pos + 1] == '/') {
            while ((c = curium_lexer_v2_next(lx)) != EOF && c != '\n') {}
            continue;
        }
        if (c == '/' && lx->pos + 1 < lx->length && lx->src[lx->pos + 1] == '*') {
            curium_lexer_v2_next(lx); // consume '/'
            curium_lexer_v2_next(lx); // consume '*'
            while (lx->pos + 1 < lx->length) {
                if (lx->src[lx->pos] == '*' && lx->src[lx->pos + 1] == '/') {
                    curium_lexer_v2_next(lx); // consume '*'
                    curium_lexer_v2_next(lx); // consume '/'
                    break;
                }
                curium_lexer_v2_next(lx);
            }
            continue;
        }
        break;
    }
}

/* Parse interpolated string "hello {name} age: {age}" */
static curium_token_t curium_lexer_v2_parse_interpolated_string(curium_lexer_t* lx, size_t start_line, size_t start_col) {
    curium_lexer_v2_next(lx); /* consume opening quote */

    curium_string_t* content = curium_string_new("");
    int in_interpolation = 0;
    int brace_depth = 0;

    while (lx->pos < lx->length) {
        int c = curium_lexer_v2_peek(lx);
        if (c == EOF) break;

        if (!in_interpolation && c == '"') {
            curium_lexer_v2_next(lx); /* consume closing quote */
            break;
        }

        if (!in_interpolation && c == '{') {
            in_interpolation = 1;
            brace_depth = 1;
            curium_string_append(content, "{");
            curium_lexer_v2_next(lx);
            continue;
        }

        if (in_interpolation) {
            if (c == '{') brace_depth++;
            else if (c == '}') {
                brace_depth--;
                if (brace_depth == 0) {
                    in_interpolation = 0;
                }
            }
        }

        {
            char buf[2];
            buf[0] = (char)c;
            buf[1] = '\0';
            curium_string_append(content, buf);
        }
        curium_lexer_v2_next(lx);
    }

    {
        curium_token_t tok = curium_make_token(CURIUM_TOK_INTERPOLATED_STRING, content->data, content->length, start_line, start_col);
        curium_string_free(content); /* fix memory leak */
        return tok;
    }
}

/* Parse raw string r"content" — called when we have already consumed 'r' as an
 * identifier char but then see the next char is '"'. We receive start_pos pointing
 * to the first char after 'r' was identified, i.e. the '"'. */
static curium_token_t curium_lexer_v2_parse_raw_string(curium_lexer_t* lx, size_t start_line, size_t start_col) {
    curium_lexer_v2_next(lx); /* consume opening '"' */

    curium_string_t* content = curium_string_new("");

    while (lx->pos < lx->length) {
        int c = curium_lexer_v2_peek(lx);
        if (c == EOF) break;

        if (c == '"') {
            curium_lexer_v2_next(lx); /* consume closing quote */
            break;
        }

        {
            char buf[2];
            buf[0] = (char)c;
            buf[1] = '\0';
            curium_string_append(content, buf);
        }
        curium_lexer_v2_next(lx);
    }

    {
        curium_token_t tok = curium_make_token(CURIUM_TOK_RAW_STRING, content->data, content->length, start_line, start_col);
        curium_string_free(content); /* fix memory leak */
        return tok;
    }
}

curium_token_t curium_lexer_v2_next_token(curium_lexer_t* lx) {
    curium_lexer_v2_skip_ws_and_comments(lx);
    size_t start_pos = lx->pos;
    size_t start_line = lx->line;
    size_t start_col  = lx->column;

    int c = curium_lexer_v2_next(lx);
    if (c == EOF) {
        return curium_make_token(CURIUM_TOK_EOF, NULL, 0, lx->line, lx->column);
    }

    /* Single character tokens */
    switch (c) {
        case '(': return curium_make_token(CURIUM_TOK_LPAREN,    "(",  1, start_line, start_col);
        case ')': return curium_make_token(CURIUM_TOK_RPAREN,    ")",  1, start_line, start_col);
        case '{': return curium_make_token(CURIUM_TOK_LBRACE,    "{",  1, start_line, start_col);
        case '}': return curium_make_token(CURIUM_TOK_RBRACE,    "}",  1, start_line, start_col);
        case '[': return curium_make_token(CURIUM_TOK_LBRACKET,  "[",  1, start_line, start_col);
        case ']': return curium_make_token(CURIUM_TOK_RBRACKET,  "]",  1, start_line, start_col);
        case ';': return curium_make_token(CURIUM_TOK_SEMI,      ";",  1, start_line, start_col);
        case ',': return curium_make_token(CURIUM_TOK_COMMA,     ",",  1, start_line, start_col);
        /* Arithmetic operators — were missing and fell through to identifier default */
        case '+': return curium_make_token(CURIUM_TOK_PLUS,      "+",  1, start_line, start_col);
        case '*': return curium_make_token(CURIUM_TOK_STAR,      "*",  1, start_line, start_col);
        case '/': return curium_make_token(CURIUM_TOK_SLASH,     "/",  1, start_line, start_col);
        case ':': {
            if (curium_lexer_v2_peek(lx) == '=') {
                curium_lexer_v2_next(lx);
                return curium_make_token(CURIUM_TOK_COLON_EQUAL, ":=", 2, start_line, start_col);
            }
            return curium_make_token(CURIUM_TOK_COLON, ":", 1, start_line, start_col);
        }
        case '.': return curium_make_token(CURIUM_TOK_DOT,       ".",  1, start_line, start_col);
        case '@': return curium_make_token(CURIUM_TOK_AT,        "@",  1, start_line, start_col);
        case '$': return curium_make_token(CURIUM_TOK_DOLLAR,     "$",  1, start_line, start_col);
        case '?': {
            if (curium_lexer_v2_peek(lx) == '?') {
                curium_lexer_v2_next(lx);
                return curium_make_token(CURIUM_TOK_DOUBLE_QUESTION, "??", 2, start_line, start_col);
            }
            return curium_make_token(CURIUM_TOK_QUESTION, "?", 1, start_line, start_col);
        }
        default: break;
    }

    // Multi-character operators
    if (c == '=') {
        if (curium_lexer_v2_peek(lx) == '=') { 
            curium_lexer_v2_next(lx); 
            return curium_make_token(CURIUM_TOK_EQUAL_EQUAL, "==", 2, start_line, start_col); 
        }
        if (curium_lexer_v2_peek(lx) == '>') { 
            curium_lexer_v2_next(lx); 
            return curium_make_token(CURIUM_TOK_FAT_ARROW, "=>", 2, start_line, start_col); 
        }
        return curium_make_token(CURIUM_TOK_EQUAL, "=", 1, start_line, start_col);
    }
    
    if (c == '!') {
        if (curium_lexer_v2_peek(lx) == '=') { 
            curium_lexer_v2_next(lx); 
            return curium_make_token(CURIUM_TOK_NOT_EQUAL, "!=", 2, start_line, start_col); 
        }
        return curium_make_token(CURIUM_TOK_BANG, "!", 1, start_line, start_col);
    }
    
    if (c == '<') {
        if (curium_lexer_v2_peek(lx) == '=') { 
            curium_lexer_v2_next(lx); 
            return curium_make_token(CURIUM_TOK_LT_EQUAL, "<=", 2, start_line, start_col); 
        }
        return curium_make_token(CURIUM_TOK_LT, "<", 1, start_line, start_col);
    }
    
    if (c == '>') {
        if (curium_lexer_v2_peek(lx) == '=') { 
            curium_lexer_v2_next(lx); 
            return curium_make_token(CURIUM_TOK_GT_EQUAL, ">=", 2, start_line, start_col); 
        }
        return curium_make_token(CURIUM_TOK_GT, ">", 1, start_line, start_col);
    }
    
    if (c == '&') {
        if (curium_lexer_v2_peek(lx) == '&') { 
            curium_lexer_v2_next(lx); 
            return curium_make_token(CURIUM_TOK_AND_AND, "&&", 2, start_line, start_col); 
        }
        return curium_make_token(CURIUM_TOK_AMPERSAND, "&", 1, start_line, start_col);
    }
    
    if (c == '|') {
        if (curium_lexer_v2_peek(lx) == '|') { 
            curium_lexer_v2_next(lx); 
            return curium_make_token(CURIUM_TOK_PIPE_PIPE, "||", 2, start_line, start_col); 
        }
        return curium_make_token(CURIUM_TOK_PIPE, "|", 1, start_line, start_col);
    }
    
    if (c == '-') {
        if (curium_lexer_v2_peek(lx) == '>') { 
            curium_lexer_v2_next(lx); 
            return curium_make_token(CURIUM_TOK_ARROW, "->", 2, start_line, start_col); 
        }
        return curium_make_token(CURIUM_TOK_MINUS, "-", 1, start_line, start_col);
    }
    
    if (c == '^') {
        // Context will determine if this is prefix or postfix
        return curium_make_token(CURIUM_TOK_ADDR_OF, "^", 1, start_line, start_col);
    }

    /* String literals */
    if (c == '"') {
        /* Check for interpolated string by looking ahead for { */
        size_t save_pos = lx->pos;
        int has_interpolation = 0;
        while (save_pos < lx->length && lx->src[save_pos] != '"') {
            if (lx->src[save_pos] == '{') {
                has_interpolation = 1;
                break;
            }
            save_pos++;
        }

        if (has_interpolation) {
            /* Reset to just before the opening quote and parse as interpolated */
            lx->pos    = start_pos;
            lx->line   = start_line;
            lx->column = start_col;
            return curium_lexer_v2_parse_interpolated_string(lx, start_line, start_col);
        }

        /* Regular string literal */
        {
            curium_string_t* content = curium_string_new("");
            for (;;) {
                int ch = curium_lexer_v2_next(lx);
                if (ch == EOF || ch == '\n') break;
                if (ch == '"') break;
                if (ch == '\\') {
                    int next_ch = curium_lexer_v2_next(lx);
                    char buf[2] = {0, 0};
                    switch (next_ch) {
                        case 'n': buf[0] = '\n'; break;
                        case 't': buf[0] = '\t'; break;
                        case 'r': buf[0] = '\r'; break;
                        case '\\': buf[0] = '\\'; break;
                        case '"': buf[0] = '"'; break;
                        case EOF: break;
                        default: buf[0] = (char)next_ch; break;
                    }
                    if (buf[0]) curium_string_append(content, buf);
                } else {
                    char buf[2] = {(char)ch, 0};
                    curium_string_append(content, buf);
                }
            }
            curium_token_t tok = curium_make_token(CURIUM_TOK_STRING_LITERAL, content->data, content->length, start_line, start_col);
            curium_string_free(content);
            return tok;
        }
    }

    // Number literals
    if (isdigit(c)) {
        while (isdigit(curium_lexer_v2_peek(lx))) {
            curium_lexer_v2_next(lx);
        }
        // Handle float literals
        if (curium_lexer_v2_peek(lx) == '.') {
            curium_lexer_v2_next(lx);
            while (isdigit(curium_lexer_v2_peek(lx))) {
                curium_lexer_v2_next(lx);
            }
        }
        size_t end = lx->pos;
        return curium_make_token(CURIUM_TOK_NUMBER, lx->src + start_pos, end - start_pos,
                             start_line, start_col);
    }

    /* Identifiers and keywords */
    if (curium_is_ident_start(c)) {
        while (curium_is_ident_part(curium_lexer_v2_peek(lx))) {
            curium_lexer_v2_next(lx);
        }
        {
            size_t end  = lx->pos;
            size_t len  = end - start_pos;
            const char* ident = lx->src + start_pos;

            /* BUG FIX: raw string literal r"..." — detected here after 'r' is
             * consumed as an identifier char and the next char is '"'. */
            if (len == 1 && ident[0] == 'r' && curium_lexer_v2_peek(lx) == '"') {
                return curium_lexer_v2_parse_raw_string(lx, start_line, start_col);
            }

            /* Keywords — ordered by frequency for speed */
            if (len == 1  && ident[0] == 'c')           return curium_make_token(CURIUM_TOK_KW_C,        ident, len, start_line, start_col);
            if (len == 2  && strncmp(ident, "fn",       2)  == 0) return curium_make_token(CURIUM_TOK_KW_FN,       ident, len, start_line, start_col);
            if (len == 2  && strncmp(ident, "if",       2)  == 0) return curium_make_token(CURIUM_TOK_KW_IF,       ident, len, start_line, start_col);
            if (len == 2  && strncmp(ident, "Ok",       2)  == 0) return curium_make_token(CURIUM_TOK_KW_OK,       ident, len, start_line, start_col);
            if (len == 2  && strncmp(ident, "gc",       2)  == 0) return curium_make_token(CURIUM_TOK_KW_GC,       ident, len, start_line, start_col);
            if (len == 3  && strncmp(ident, "try",      3)  == 0) return curium_make_token(CURIUM_TOK_KW_TRY,      ident, len, start_line, start_col);
            if (len == 3  && strncmp(ident, "let",      3)  == 0) return curium_make_token(CURIUM_TOK_KW_LET,      ident, len, start_line, start_col);
            if (len == 3  && strncmp(ident, "mut",      3)  == 0) return curium_make_token(CURIUM_TOK_KW_MUT,      ident, len, start_line, start_col);
            if (len == 3  && strncmp(ident, "for",      3)  == 0) return curium_make_token(CURIUM_TOK_KW_FOR,      ident, len, start_line, start_col);
            if (len == 3  && strncmp(ident, "dyn",      3)  == 0) return curium_make_token(CURIUM_TOK_KW_DYN,      ident, len, start_line, start_col);
            if (len == 3  && strncmp(ident, "int",      3)  == 0) return curium_make_token(CURIUM_TOK_KW_INT,      ident, len, start_line, start_col);
            if (len == 3  && strncmp(ident, "map",      3)  == 0) return curium_make_token(CURIUM_TOK_KW_MAP,      ident, len, start_line, start_col);
            if (len == 3  && strncmp(ident, "Err",      3)  == 0) return curium_make_token(CURIUM_TOK_KW_ERR,      ident, len, start_line, start_col);
            if (len == 3  && strncmp(ident, "pub",      3)  == 0) return curium_make_token(CURIUM_TOK_KW_PUBLIC,   ident, len, start_line, start_col);
            if (len == 4  && strncmp(ident, "impl",     4)  == 0) return curium_make_token(CURIUM_TOK_KW_IMPL,     ident, len, start_line, start_col);
            if (len == 4  && strncmp(ident, "Some",     4)  == 0) return curium_make_token(CURIUM_TOK_KW_SOME,     ident, len, start_line, start_col);
            if (len == 4  && strncmp(ident, "None",     4)  == 0) return curium_make_token(CURIUM_TOK_KW_NONE,     ident, len, start_line, start_col);
            if (len == 4  && strncmp(ident, "bool",     4)  == 0) return curium_make_token(CURIUM_TOK_KW_BOOL,     ident, len, start_line, start_col);
            if (len == 4  && strncmp(ident, "enum",     4)  == 0) return curium_make_token(CURIUM_TOK_KW_ENUM,     ident, len, start_line, start_col);
            if (len == 4  && strncmp(ident, "void",     4)  == 0) return curium_make_token(CURIUM_TOK_KW_VOID,     ident, len, start_line, start_col);
            if (len == 4  && strncmp(ident, "true",     4)  == 0) return curium_make_token(CURIUM_TOK_KW_TRUE,     ident, len, start_line, start_col);
            if (len == 4  && strncmp(ident, "free",     4)  == 0) return curium_make_token(CURIUM_TOK_KW_FREE,     ident, len, start_line, start_col);
            if (len == 4  && strncmp(ident, "else",     4)  == 0) return curium_make_token(CURIUM_TOK_KW_ELSE,     ident, len, start_line, start_col);
            if (len == 5  && strncmp(ident, "match",    5)  == 0) return curium_make_token(CURIUM_TOK_KW_MATCH,    ident, len, start_line, start_col);
            if (len == 5  && strncmp(ident, "catch",    5)  == 0) return curium_make_token(CURIUM_TOK_KW_CATCH,    ident, len, start_line, start_col);
            if (len == 5  && strncmp(ident, "throw",    5)  == 0) return curium_make_token(CURIUM_TOK_KW_THROW,    ident, len, start_line, start_col);
            if (len == 5  && strncmp(ident, "trait",    5)  == 0) return curium_make_token(CURIUM_TOK_KW_TRAIT,    ident, len, start_line, start_col);
            if (len == 5  && strncmp(ident, "array",    5)  == 0) return curium_make_token(CURIUM_TOK_KW_ARRAY,    ident, len, start_line, start_col);
            if (len == 5  && strncmp(ident, "slice",    5)  == 0) return curium_make_token(CURIUM_TOK_KW_SLICE,    ident, len, start_line, start_col);
            if (len == 5  && strncmp(ident, "float",    5)  == 0) return curium_make_token(CURIUM_TOK_KW_FLOAT,    ident, len, start_line, start_col);
            if (len == 5  && strncmp(ident, "false",    5)  == 0) return curium_make_token(CURIUM_TOK_KW_FALSE,    ident, len, start_line, start_col);
            if (len == 5  && strncmp(ident, "while",    5)  == 0) return curium_make_token(CURIUM_TOK_KW_WHILE,    ident, len, start_line, start_col);
            if (len == 5  && strncmp(ident, "break",    5)  == 0) return curium_make_token(CURIUM_TOK_KW_BREAK,    ident, len, start_line, start_col);
            if (len == 5  && strncmp(ident, "spawn",    5)  == 0) return curium_make_token(CURIUM_TOK_KW_SPAWN,    ident, len, start_line, start_col);
            if (len == 5  && strncmp(ident, "print",    5)  == 0) return curium_make_token(CURIUM_TOK_KW_PRINT,    ident, len, start_line, start_col);
            if (len == 5  && strncmp(ident, "input",    5)  == 0) return curium_make_token(CURIUM_TOK_KW_INPUT,    ident, len, start_line, start_col);
            if (len == 5  && strncmp(ident, "union",    5)  == 0) return curium_make_token(CURIUM_TOK_KW_UNION,    ident, len, start_line, start_col);
            if (len == 6  && strncmp(ident, "struct",   6)  == 0) return curium_make_token(CURIUM_TOK_KW_STRUCT,   ident, len, start_line, start_col);
            if (len == 6  && strncmp(ident, "string",   6)  == 0) return curium_make_token(CURIUM_TOK_KW_STRING,   ident, len, start_line, start_col); /* single check — duplicate removed */
            if (len == 6  && strncmp(ident, "strnum",   6)  == 0) return curium_make_token(CURIUM_TOK_KW_STRNUM,   ident, len, start_line, start_col);
            if (len == 6  && strncmp(ident, "import",   6)  == 0) return curium_make_token(CURIUM_TOK_KW_IMPORT,   ident, len, start_line, start_col);
            if (len == 6  && strncmp(ident, "return",   6)  == 0) return curium_make_token(CURIUM_TOK_KW_RETURN,   ident, len, start_line, start_col);
            if (len == 6  && strncmp(ident, "Result",   6)  == 0) return curium_make_token(CURIUM_TOK_KW_RESULT,   ident, len, start_line, start_col);
            if (len == 6  && strncmp(ident, "Option",   6)  == 0) return curium_make_token(CURIUM_TOK_KW_OPTION,   ident, len, start_line, start_col);
            if (len == 6  && strncmp(ident, "malloc",   6)  == 0) return curium_make_token(CURIUM_TOK_KW_MALLOC,   ident, len, start_line, start_col);
            if (len == 7  && strncmp(ident, "mutable",  7)  == 0) return curium_make_token(CURIUM_TOK_KW_MUTABLE,  ident, len, start_line, start_col); /* BUG FIX: was 8 */
            if (len == 7  && strncmp(ident, "require",  7)  == 0) return curium_make_token(CURIUM_TOK_KW_REQUIRE,  ident, len, start_line, start_col);
            if (len == 8  && strncmp(ident, "continue", 8)  == 0) return curium_make_token(CURIUM_TOK_KW_CONTINUE, ident, len, start_line, start_col);

            /* v4.0: Sized numeric types */
            if (len == 2  && strncmp(ident, "i8",       2)  == 0) return curium_make_token(CURIUM_TOK_KW_I8,       ident, len, start_line, start_col);
            if (len == 2  && strncmp(ident, "u8",       2)  == 0) return curium_make_token(CURIUM_TOK_KW_U8,       ident, len, start_line, start_col);
            if (len == 3  && strncmp(ident, "i16",      3)  == 0) return curium_make_token(CURIUM_TOK_KW_I16,      ident, len, start_line, start_col);
            if (len == 3  && strncmp(ident, "i32",      3)  == 0) return curium_make_token(CURIUM_TOK_KW_I32,      ident, len, start_line, start_col);
            if (len == 3  && strncmp(ident, "i64",      3)  == 0) return curium_make_token(CURIUM_TOK_KW_I64,      ident, len, start_line, start_col);
            if (len == 3  && strncmp(ident, "u16",      3)  == 0) return curium_make_token(CURIUM_TOK_KW_U16,      ident, len, start_line, start_col);
            if (len == 3  && strncmp(ident, "u32",      3)  == 0) return curium_make_token(CURIUM_TOK_KW_U32,      ident, len, start_line, start_col);
            if (len == 3  && strncmp(ident, "u64",      3)  == 0) return curium_make_token(CURIUM_TOK_KW_U64,      ident, len, start_line, start_col);
            if (len == 3  && strncmp(ident, "f32",      3)  == 0) return curium_make_token(CURIUM_TOK_KW_F32,      ident, len, start_line, start_col);
            if (len == 3  && strncmp(ident, "f64",      3)  == 0) return curium_make_token(CURIUM_TOK_KW_F64,      ident, len, start_line, start_col);
            if (len == 5  && strncmp(ident, "usize",    5)  == 0) return curium_make_token(CURIUM_TOK_KW_USIZE,    ident, len, start_line, start_col);

            /* v4.0: Reactor memory model */
            if (len == 7  && strncmp(ident, "reactor",  7)  == 0) return curium_make_token(CURIUM_TOK_KW_REACTOR,  ident, len, start_line, start_col);
            if (len == 5  && strncmp(ident, "arena",    5)  == 0) return curium_make_token(CURIUM_TOK_KW_ARENA,    ident, len, start_line, start_col);
            if (len == 6  && strncmp(ident, "manual",   6)  == 0) return curium_make_token(CURIUM_TOK_KW_MANUAL,   ident, len, start_line, start_col);

            return curium_make_token(CURIUM_TOK_IDENTIFIER, ident, len, start_line, start_col);
        }
    }

    /* Unknown character — report as an error token (single char identifier) */
    {
        char ch = (char)c;
        return curium_make_token(CURIUM_TOK_IDENTIFIER, &ch, 1, start_line, start_col);
    }
}
