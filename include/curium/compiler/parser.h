#ifndef CURIUM_PARSER_H
#define CURIUM_PARSER_H
#include "curium/compiler/lexer.h"
#include "curium/compiler/ast.h"
#include "curium/compiler/ast_v2.h"

typedef struct {
    curium_lexer_t lexer;
    curium_token_t current;
} curium_parser_t;

int curium_compile_file(const char* entry_path, const char* output_exe);
int curium_emit_c_file(const char* entry_path, const char* output_c_path);

/* v2 high-level parse interface */
curium_ast_v2_list_t curium_parse_v2(const char* src);

#endif
