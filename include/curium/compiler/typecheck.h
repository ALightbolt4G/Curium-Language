#ifndef CURIUM_TYPECHECK_H
#define CURIUM_TYPECHECK_H

#include "curium/compiler/ast_v2.h"

/* Forward declaration */
typedef struct curium_typecheck_ctx curium_typecheck_ctx_t;

/* Create/destroy type checker context */
curium_typecheck_ctx_t* curium_typecheck_new(const char* source_text, const char* file_path);
void curium_typecheck_free(curium_typecheck_ctx_t* ctx);

/* Type check an entire module */
int curium_typecheck_module(curium_typecheck_ctx_t* ctx, curium_ast_v2_list_t* ast);

/* Get error/warning counts */
int curium_typecheck_get_error_count(curium_typecheck_ctx_t* ctx);
int curium_typecheck_get_warning_count(curium_typecheck_ctx_t* ctx);

#endif
