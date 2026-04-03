#ifndef CURIUM_LEXER_H
#define CURIUM_LEXER_H
#include "curium/compiler/tokens.h"

typedef struct {
    const char* src;
    size_t length;
    size_t pos;
    size_t line;
    size_t column;
    curium_string_t* current_lexeme;
} curium_lexer_t;

void curium_lexer_init(curium_lexer_t* lx, const char* src);
curium_token_t curium_lexer_next_token(curium_lexer_t* lx);

/* v2 lexer functions */
void curium_lexer_v2_init(curium_lexer_t* lx, const char* src);
curium_token_t curium_lexer_v2_next_token(curium_lexer_t* lx);

#endif
