#include "curium/compiler/tokens.h"
// Tokens are mostly enums.
void curium_token_free(curium_token_t* t) {
    if (t && t->lexeme) {
        curium_string_free(t->lexeme);
        t->lexeme = NULL;
    }
}
