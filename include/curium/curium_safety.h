/**
 * @file curium_safety.h
 * @brief C code safety filtering and sandboxing.
 *
 * Provides comprehensive safety checks for C code blocks:
 * - Dangerous function blacklisting
 * - Static analysis for unsafe patterns
 * - Sandbox execution restrictions
 */
#ifndef CURIUM_SAFETY_H
#define CURIUM_SAFETY_H

#include "core.h"
#include "string.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Safety check result codes.
 */
typedef enum {
    CURIUM_SAFETY_OK = 0,
    CURIUM_SAFETY_UNSAFE_FUNC,
    CURIUM_SAFETY_UNSAFE_PTR_ARITH,
    CURIUM_SAFETY_UNSAFE_CAST,
    CURIUM_SAFETY_BUFFER_OVERFLOW_RISK,
    CURIUM_SAFETY_USE_AFTER_FREE,
    CURIUM_SAFETY_UNSAFE_INCLUDE,
    CURIUM_SAFETY_SYSTEM_CALL,
    CURIUM_SAFETY_NETWORK_OP,
    CURIUM_SAFETY_FILE_OP
} curium_safety_result_t;

/**
 * @brief Safety violation information.
 */
typedef struct {
    curium_safety_result_t code;
    size_t line;
    size_t column;
    char* message;
    char* suggestion;
} curium_safety_violation_t;

/**
 * @brief Safety check options.
 */
typedef struct {
    int check_blacklist;      /* Check dangerous functions */
    int check_static;           /* Run static analysis */
    int sandbox_level;          /* 0=none, 1=moderate, 2=strict */
    int allow_file_ops;         /* Allow file operations */
    int allow_network;          /* Allow network operations */
    int allow_system_calls;     /* Allow system() calls */
} curium_safety_opts_t;

/**
 * @brief Default safety options (strict).
 */
extern curium_safety_opts_t CURIUM_SAFETY_STRICT;

/**
 * @brief Moderate safety options.
 */
extern curium_safety_opts_t CURIUM_SAFETY_MODERATE;

/**
 * @brief Check C code for safety violations.
 * @param code The C code to check.
 * @param opts Safety options.
 * @param violations Output array of violations (caller frees).
 * @param violation_count Output count.
 * @return 0 if safe, -1 if violations found.
 */
int curium_safety_check(const char* code, curium_safety_opts_t* opts,
                    curium_safety_violation_t** violations, size_t* violation_count);

/**
 * @brief Check if function is blacklisted.
 * @param func_name Function name.
 * @return 1 if blacklisted, 0 if safe.
 */
int curium_safety_is_blacklisted(const char* func_name);

/**
 * @brief Get blacklist suggestion for function.
 * @param func_name Function name.
 * @return Safe alternative or NULL.
 */
const char* curium_safety_blacklist_suggestion(const char* func_name);

/**
 * @brief Free violation array.
 * @param violations Array to free.
 * @param count Number of violations.
 */
void curium_safety_free_violations(curium_safety_violation_t* violations, size_t count);

/**
 * @brief Print safety report.
 * @param code Original code.
 * @param violations Violation array.
 * @param count Number of violations.
 */
void curium_safety_print_report(const char* code, curium_safety_violation_t* violations, size_t count);

/**
 * @brief Initialize sandbox for C execution.
 * @param level Sandbox level (0-2).
 * @return 0 on success, -1 on error.
 */
int curium_sandbox_init(int level);

/**
 * @brief Execute C code in sandbox (if enabled).
 * @param compiled_path Path to compiled binary.
 * @param level Sandbox level.
 * @return Exit code from sandboxed process.
 */
int curium_sandbox_exec(const char* compiled_path, int level);

/**
 * @brief Cleanup sandbox.
 */
void curium_sandbox_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif /* CURIUM_SAFETY_H */
