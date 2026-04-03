#include "curium/curium_safety.h"
#include "curium/memory.h"
#include <string.h>
#include <ctype.h>
#include <stdio.h>

/* Simple strdup wrapper */
static char* curium_safety_strdup(const char* s) {
    if (!s) return NULL;
    size_t len = strlen(s) + 1;
    char* copy = (char*)curium_alloc(len, "safety_string");
    if (copy) memcpy(copy, s, len);
    return copy;
}

/* ============================================================================
 * Dangerous Function Blacklist
 * ========================================================================== */

typedef struct {
    const char* func;
    const char* alternative;
    const char* reason;
} curium_blacklist_entry_t;

static const curium_blacklist_entry_t CURIUM_BLACKLIST[] = {
    /* String operations - unsafe */
    {"strcpy", "curium_string_copy", "Unbounded string copy"},
    {"strcat", "curium_string_append", "Unbounded string concatenation"},
    {"strncpy", "curium_string_ncopy", "Potentially non-null-terminated copy"},
    {"strncat", "curium_string_nappend", "Potentially non-null-terminated concat"},
    {"sprintf", "curium_string_format", "Format string vulnerability"},
    {"vsprintf", "curium_string_vformat", "Format string vulnerability"},
    {"gets", "curium_input", "Buffer overflow guaranteed"},
    {"fgets", "curium_input_line", "Use CM safe input instead"},
    
    /* Memory operations - unsafe */
    {"alloca", "curium_alloc", "Stack allocation unsafe"},
    {"memcpy", "curium_safe_memcpy", "Bounds-checked copy required"},
    {"memset", "curium_safe_memset", "Bounds-checked set required"},
    
    /* Note: malloc/free are allowed in CM - they get converted to curium_alloc/curium_free */
    
    /* File operations - restrict */
    {"fopen", "curium_file_open", "Use CM file abstraction"},
    {"freopen", NULL, "File reopen not allowed"},
    {"fclose", "curium_file_close", "Use CM file abstraction"},
    {"fread", "curium_file_read", "Use CM file abstraction"},
    {"fwrite", "curium_file_write", "Use CM file abstraction"},
    {"fscanf", NULL, "Format string vulnerability"},
    {"fprintf", "curium_file_write", "Use CM file abstraction"},
    
    /* System operations - dangerous */
    {"system", NULL, "Arbitrary command execution"},
    {"popen", NULL, "Arbitrary command execution"},
    {"pclose", NULL, "Shell execution"},
    {"execl", NULL, "Process execution"},
    {"execv", NULL, "Process execution"},
    {"execle", NULL, "Process execution"},
    {"execve", NULL, "Process execution"},
    
    /* Network - restrict */
    {"socket", "curium_socket_create", "Use CM network abstraction"},
    {"connect", "curium_socket_connect", "Use CM network abstraction"},
    {"bind", "curium_socket_bind", "Use CM network abstraction"},
    {"listen", "curium_socket_listen", "Use CM network abstraction"},
    {"accept", "curium_socket_accept", "Use CM network abstraction"},
    
    /* Pointer arithmetic patterns */
    {"offsetof", NULL, "Low-level memory manipulation"},
    
    {NULL, NULL, NULL}
};

curium_safety_opts_t CURIUM_SAFETY_STRICT = {
    .check_blacklist = 1,
    .check_static = 1,
    .sandbox_level = 2,
    .allow_file_ops = 0,
    .allow_network = 0,
    .allow_system_calls = 0
};

curium_safety_opts_t CURIUM_SAFETY_MODERATE = {
    .check_blacklist = 1,
    .check_static = 1,
    .sandbox_level = 1,
    .allow_file_ops = 1,
    .allow_network = 1,
    .allow_system_calls = 0
};

int curium_safety_is_blacklisted(const char* func_name) {
    if (!func_name) return 0;
    for (const curium_blacklist_entry_t* entry = CURIUM_BLACKLIST; entry->func; entry++) {
        if (strcmp(func_name, entry->func) == 0) return 1;
    }
    return 0;
}

const char* curium_safety_blacklist_suggestion(const char* func_name) {
    if (!func_name) return NULL;
    for (const curium_blacklist_entry_t* entry = CURIUM_BLACKLIST; entry->func; entry++) {
        if (strcmp(func_name, entry->func) == 0) return entry->alternative;
    }
    return NULL;
}

/* Static Analysis - Simple Pattern Matching - Patterns currently unused but reserved for future */
#if 0
typedef struct {
    const char* pattern;
    curium_safety_result_t type;
    const char* message;
} curium_pattern_t;

static const curium_pattern_t CURIUM_PATTERNS[] = {
    {"*p++", CURIUM_SAFETY_UNSAFE_PTR_ARITH, "Unchecked pointer increment"},
    {"*p--", CURIUM_SAFETY_UNSAFE_PTR_ARITH, "Unchecked pointer decrement"},
    {"p[i]", CURIUM_SAFETY_BUFFER_OVERFLOW_RISK, "Array access without bounds check"},
    {"(char*)", CURIUM_SAFETY_UNSAFE_CAST, "Dangerous pointer cast"},
    {"(void*)", CURIUM_SAFETY_UNSAFE_CAST, "Dangerous pointer cast"},
    {"free(", CURIUM_SAFETY_USE_AFTER_FREE, "Manual memory management"},
    {"#include <", CURIUM_SAFETY_UNSAFE_INCLUDE, "Custom includes may be unsafe"},
    {"system(", CURIUM_SAFETY_SYSTEM_CALL, "System command execution"},
    {"socket(", CURIUM_SAFETY_NETWORK_OP, "Raw network access"},
    {NULL, 0, NULL}
};
#endif

static int curium_safety_check_blacklist(const char* code, curium_safety_violation_t** violations, size_t* count) {
    size_t capacity = 8;
    *violations = (curium_safety_violation_t*)curium_alloc(capacity * sizeof(curium_safety_violation_t), "safety_violations");
    if (!violations) return -1;
    *count = 0;
    
    /* Simple token-based search for blacklisted functions */
    const char* p = code;
    size_t line = 1;
    size_t col = 1;
    
    while (*p) {
        if (isalpha((unsigned char)*p) || *p == '_') {
            /* Start of identifier */
            const char* start = p;
            while (isalnum((unsigned char)*p) || *p == '_') p++;
            
            size_t len = (size_t)(p - start);
            char ident[64] = {0};
            if (len < sizeof(ident)) {
                memcpy(ident, start, len);
                
                /* Check if followed by '(' - it's a function call */
                const char* after = p;
                while (*after == ' ' || *after == '\t') after++;
                
                if (*after == '(' && curium_safety_is_blacklisted(ident)) {
                    /* Expand violations if needed */
                    if (*count >= capacity) {
                        capacity *= 2;
                        curium_safety_violation_t* new_v = (curium_safety_violation_t*)curium_alloc(capacity * sizeof(curium_safety_violation_t), "safety_violations");
                        if (!new_v) return -1;
                        memcpy(new_v, *violations, *count * sizeof(curium_safety_violation_t));
                        curium_free(*violations);
                        *violations = new_v;
                    }
                    
                    curium_safety_violation_t* v = &(*violations)[*count];
                    v->code = CURIUM_SAFETY_UNSAFE_FUNC;
                    v->line = line;
                    v->column = col;
                    
                    const char* alt = curium_safety_blacklist_suggestion(ident);
                    char msg[256];
                    if (alt) {
                        snprintf(msg, sizeof(msg), "Dangerous function '%s' - use '%s' instead", ident, alt);
                    } else {
                        snprintf(msg, sizeof(msg), "Dangerous function '%s' - not allowed in CM", ident);
                    }
                    v->message = curium_safety_strdup(msg);
                    v->suggestion = alt ? curium_safety_strdup(alt) : NULL;
                    
                    (*count)++;
                }
            }
            
            col += (int)len;
        } else {
            if (*p == '\n') {
                line++;
                col = 1;
            } else {
                col++;
            }
            p++;
        }
    }
    
    return (*count > 0) ? -1 : 0;
}

/* Pattern checking - currently integrated into blacklist check */
static int curium_safety_check_patterns(const char* code, curium_safety_violation_t** violations, size_t* count) {
    (void)code;
    (void)violations;
    (void)count;
    /* Full static analysis would use a proper C parser */
    return 0;
}

/* ============================================================================
 * Public API
 * ========================================================================== */

int curium_safety_check(const char* code, curium_safety_opts_t* opts,
                    curium_safety_violation_t** violations, size_t* violation_count) {
    if (!code || !violations || !violation_count) return -1;
    
    curium_safety_opts_t actual_opts = opts ? *opts : CURIUM_SAFETY_STRICT;
    
    *violations = NULL;
    *violation_count = 0;
    
    /* Check blacklist if enabled */
    if (actual_opts.check_blacklist) {
        if (curium_safety_check_blacklist(code, violations, violation_count) != 0) {
            /* Violations found, but we continue to collect all */
        }
    }
    
    /* Check patterns if enabled */
    if (actual_opts.check_static) {
        curium_safety_check_patterns(code, violations, violation_count);
    }
    
    return (*violation_count > 0) ? -1 : 0;
}

void curium_safety_free_violations(curium_safety_violation_t* violations, size_t count) {
    if (!violations) return;
    for (size_t i = 0; i < count; i++) {
        if (violations[i].message) curium_free(violations[i].message);
        if (violations[i].suggestion) curium_free(violations[i].suggestion);
    }
    curium_free(violations);
}

void curium_safety_print_report(const char* code, curium_safety_violation_t* violations, size_t count) {
    (void)code; /* Unused but kept for API compatibility */
    if (!violations || count == 0) {
        printf("[CM Safety] No violations found. Code is safe.\n");
        return;
    }
    
    fprintf(stderr, "[CM Safety] Found %zu violation(s):\n", count);
    for (size_t i = 0; i < count; i++) {
        curium_safety_violation_t* v = &violations[i];
        fprintf(stderr, "  Line %zu, Col %zu: %s\n", v->line, v->column, v->message);
        if (v->suggestion) {
            fprintf(stderr, "    Suggestion: Use '%s'\n", v->suggestion);
        }
    }
}

/* ============================================================================
 * Sandbox (Platform-specific)
 * ========================================================================== */

int curium_sandbox_init(int level) {
    if (level == 0) return 0; /* No sandbox */
    
    /* Platform-specific sandbox setup would go here:
     * - Linux: seccomp-bpf, namespaces, chroot
     * - Windows: AppContainer, job objects
     * - macOS: seatbelt
     */
    
    (void)level;
    return 0; /* Placeholder - full implementation would be OS-specific */
}

int curium_sandbox_exec(const char* compiled_path, int level) {
    if (level == 0) {
        /* No sandbox - direct execution */
        /* Would use system() or execve() here */
        (void)compiled_path;
        return 0;
    }
    
    /* Sandboxed execution would go here */
    (void)compiled_path;
    (void)level;
    return 0;
}

void curium_sandbox_cleanup(void) {
    /* Cleanup sandbox resources */
}
