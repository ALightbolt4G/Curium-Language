#ifndef CURIUM_TOKENS_H
#define CURIUM_TOKENS_H

#include "curium/core.h"
#include "curium/string.h"

typedef enum {
    CURIUM_TOK_EOF = 0,
    CURIUM_TOK_IDENTIFIER,
    CURIUM_TOK_STRING_LITERAL,
    CURIUM_TOK_NUMBER,
    CURIUM_TOK_LPAREN,
    CURIUM_TOK_RPAREN,
    CURIUM_TOK_LBRACE,
    CURIUM_TOK_RBRACE,
    CURIUM_TOK_LBRACKET,
    CURIUM_TOK_RBRACKET,
    CURIUM_TOK_SEMI,
    CURIUM_TOK_COMMA,
    CURIUM_TOK_COLON,
    CURIUM_TOK_EQUAL,

    /* Operators */
    CURIUM_TOK_PLUS,
    CURIUM_TOK_MINUS,
    CURIUM_TOK_STAR,
    CURIUM_TOK_SLASH,
    CURIUM_TOK_DOT,
    CURIUM_TOK_LT,
    CURIUM_TOK_GT,
    CURIUM_TOK_BANG,
    CURIUM_TOK_AMPERSAND,
    CURIUM_TOK_PIPE,
    /* New CM v2 operators */
    CURIUM_TOK_COLON_EQUAL,  /* := inference assignment */
    CURIUM_TOK_DEREF,        /* ^ postfix dereference */
    CURIUM_TOK_ADDR_OF,      /* ^ prefix address-of */
    CURIUM_TOK_QUESTION,     /* ? option type */
    CURIUM_TOK_DOUBLE_QUESTION, /* ?? null coalescing */
    CURIUM_TOK_AT,           /* @ attribute */
    CURIUM_TOK_DOLLAR,       /* $ fallback in dyn */
    CURIUM_TOK_ARROW,        /* -> function return type */
    CURIUM_TOK_FAT_ARROW,    /* => lambda/fat arrow */
    
    /* Comparison operators (multi-char) */
    CURIUM_TOK_EQUAL_EQUAL,  /* == */
    CURIUM_TOK_NOT_EQUAL,    /* != */
    CURIUM_TOK_LT_EQUAL,     /* <= */
    CURIUM_TOK_GT_EQUAL,     /* >= */
    CURIUM_TOK_AND_AND,      /* && */
    CURIUM_TOK_PIPE_PIPE,    /* || */

    /* New CM v2 keywords */
    CURIUM_TOK_KW_LET,          /* let immutable declaration */
    CURIUM_TOK_KW_MUT,          /* mut mutable declaration */
    CURIUM_TOK_KW_FN,           /* fn function keyword */
    CURIUM_TOK_KW_STRUCT,       /* struct type definition */
    CURIUM_TOK_KW_UNION,        /* union sum type */
    CURIUM_TOK_KW_ENUM,         /* enum algebraic data type */
    CURIUM_TOK_KW_IMPL,         /* impl implementation block */
    CURIUM_TOK_KW_TRAIT,        /* trait definition block */
    CURIUM_TOK_KW_MATCH,        /* match expression */
    CURIUM_TOK_KW_SOME,         /* Some variant of Option */
    CURIUM_TOK_KW_NONE,         /* None variant of Option */
    CURIUM_TOK_KW_OK,           /* Ok variant of Result */
    CURIUM_TOK_KW_ERR,          /* Err variant of Result */
    CURIUM_TOK_KW_RESULT,       /* Result type */
    CURIUM_TOK_KW_OPTION,       /* Option type */
    CURIUM_TOK_KW_ARRAY,        /* array<T> type */
    CURIUM_TOK_KW_SLICE,        /* slice<T> type */
    CURIUM_TOK_KW_MAP,          /* map<K,V> type */
    CURIUM_TOK_KW_BOOL,         /* bool type */
    CURIUM_TOK_KW_INT,          /* int type */
    CURIUM_TOK_KW_FLOAT,        /* float type */
    CURIUM_TOK_KW_STRING,       /* string type */
    CURIUM_TOK_KW_VOID,         /* void type */
    CURIUM_TOK_KW_TRUE,         /* true literal */
    CURIUM_TOK_KW_FALSE,        /* false literal */
    CURIUM_TOK_KW_IF,           /* if statement */
    CURIUM_TOK_KW_ELSE,         /* else statement */
    CURIUM_TOK_KW_WHILE,        /* while loop */
    CURIUM_TOK_KW_FOR,          /* for loop */
    CURIUM_TOK_KW_RETURN,       /* return statement */
    CURIUM_TOK_KW_BREAK,        /* break statement */
    CURIUM_TOK_KW_CONTINUE,     /* continue statement */
    CURIUM_TOK_KW_MUTABLE,      /* mutable parameter marker */
    CURIUM_TOK_KW_IMPORT,       /* import module */
    CURIUM_TOK_KW_DYN,          /* dyn dynamic operator */
    CURIUM_TOK_KW_CALL,         /* call dynamic function */
    CURIUM_TOK_KW_SPAWN,        /* spawn thread */
    CURIUM_TOK_KW_C,            /* c block keyword */
    CURIUM_TOK_KW_GC,           /* gc namespace */
    CURIUM_TOK_KW_PRINT,        /* print function */
    CURIUM_TOK_KW_MALLOC,       /* malloc function */
    CURIUM_TOK_KW_FREE,         /* free function */
    CURIUM_TOK_KW_STRNUM,       /* strnum primitive type */

    /* v4.0: Sized numeric types */
    CURIUM_TOK_KW_I8,           /* i8  → int8_t */
    CURIUM_TOK_KW_I16,          /* i16 → int16_t */
    CURIUM_TOK_KW_I32,          /* i32 → int32_t */
    CURIUM_TOK_KW_I64,          /* i64 → int64_t */
    CURIUM_TOK_KW_U8,           /* u8  → uint8_t */
    CURIUM_TOK_KW_U16,          /* u16 → uint16_t */
    CURIUM_TOK_KW_U32,          /* u32 → uint32_t */
    CURIUM_TOK_KW_U64,          /* u64 → uint64_t */
    CURIUM_TOK_KW_F32,          /* f32 → float */
    CURIUM_TOK_KW_F64,          /* f64 → double */
    CURIUM_TOK_KW_USIZE,        /* usize → size_t */

    /* v4.0: Reactor memory model */
    CURIUM_TOK_KW_REACTOR,      /* reactor keyword */
    CURIUM_TOK_KW_ARENA,        /* arena mode */
    CURIUM_TOK_KW_MANUAL,       /* manual mode */

    /* C#-like professional keywords (legacy) */
    CURIUM_TOK_KW_NAMESPACE,
    CURIUM_TOK_KW_USING,
    CURIUM_TOK_KW_CLASS,
    CURIUM_TOK_KW_PUBLIC,
    CURIUM_TOK_KW_PRIVATE,
    CURIUM_TOK_KW_STATIC,
    CURIUM_TOK_KW_ASYNC,
    CURIUM_TOK_KW_AWAIT,
    CURIUM_TOK_KW_TASK,
    CURIUM_TOK_KW_TRY,
    CURIUM_TOK_KW_CATCH,
    CURIUM_TOK_KW_FINALLY,
    CURIUM_TOK_KW_THROW,
    CURIUM_TOK_KW_GET,
    CURIUM_TOK_KW_SET,
    CURIUM_TOK_KW_DOUBLE,
    CURIUM_TOK_KW_NULL,
    CURIUM_TOK_KW_INTERFACE,
    CURIUM_TOK_KW_IMPLEMENTS,
    CURIUM_TOK_KW_EXTENDS,
    CURIUM_TOK_KW_FROM,
    CURIUM_TOK_KW_PACKAGE,

    /* Legacy keywords */
    CURIUM_TOK_KW_INPUT,
    CURIUM_TOK_KW_REQUIRE,

    /* Dynamic data structures (legacy) */
    CURIUM_TOK_KW_LIST,
    CURIUM_TOK_KW_NEW,
    CURIUM_TOK_KW_PTR,

    /* Native API keywords (legacy) */
    CURIUM_TOK_KW_STR,
    CURIUM_TOK_KW_GC_COLLECT,

    /* Classified identifiers */
    CURIUM_TOK_FUNCTION_NAME,
    CURIUM_TOK_METHOD_NAME,

    /* Special */
    CURIUM_TOK_COMMENT,
    CURIUM_TOK_CPP_BLOCK,
    CURIUM_TOK_C_BLOCK,
    
    /* New v2 specific */
    CURIUM_TOK_INTERPOLATED_STRING,  /* "hello {name}" */
    CURIUM_TOK_RAW_STRING,           /* r"raw string"  */

    /* v5.0: Developer Cache Control — #[attribute] syntax.
     * Emitted when lexer sees #[name]; lexeme contains the inner name only.
     * Example: #[hot]  →  CURIUM_TOK_HASH_ATTR, lexeme = "hot"           */
    CURIUM_TOK_HASH_ATTR
} curium_token_kind_t;

typedef struct {
    curium_token_kind_t kind;
    curium_string_t*    lexeme;
    size_t          line;
    size_t          column;
} curium_token_t;

/* Token functions */
void curium_token_free(curium_token_t* tok);

#endif
