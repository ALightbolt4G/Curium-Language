#ifndef CURIUM_CODEGEN_H
#define CURIUM_CODEGEN_H

#include "curium/core.h"
#include "curium/string.h"

/* Forward declarations of frontend AST types (kept internal for now). */
#include "curium/compiler/ast.h"

/* Generate hardened C for a parsed CM program. */
curium_string_t* curium_codegen_to_c(const curium_ast_list_t* ast);

#endif /* CURIUM_CODEGEN_H */

