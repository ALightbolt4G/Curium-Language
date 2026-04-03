/**
 * @file cmd.h
 * @brief CM CMD SDK — Cross-platform, injection-safe command execution.
 *
 * Uses OS process APIs (CreateProcess on Windows, fork+execvp on Unix)
 * directly, bypassing the shell entirely. Arguments are passed as an array,
 * so shell metacharacters like ; | && $() ` are never interpreted.
 *
 * Usage:
 *   curium_cmd_t* cmd = curium_cmd_new("python");
 *   curium_cmd_arg(cmd, "-m");
 *   curium_cmd_arg(cmd, "yt_dlp");
 *   curium_cmd_arg(cmd, user_url);   // safe — no injection possible
 *   curium_cmd_result_t* r = curium_cmd_run(cmd);
 *   printf("exit=%d stdout=%s\n", r->exit_code, r->stdout_output->data);
 *   curium_cmd_result_free(r);
 *   curium_cmd_free(cmd);
 */
#ifndef CURIUM_CMD_H
#define CURIUM_CMD_H

#include "core.h"
#include "string.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/*  Structures                                                          */
/* ------------------------------------------------------------------ */

/**
 * @brief Opaque command builder. Add arguments with curium_cmd_arg().
 */
typedef struct curium_cmd curium_cmd_t;

/**
 * @brief Result of a finished command execution.
 */
typedef struct {
    int          exit_code;     /**< Process exit/return code            */
    curium_string_t* stdout_output; /**< Captured standard output            */
    curium_string_t* stderr_output; /**< Captured standard error             */
} curium_cmd_result_t;

/* ------------------------------------------------------------------ */
/*  API                                                                 */
/* ------------------------------------------------------------------ */

/**
 * @brief Create a new command, setting the program to execute.
 * @param program The executable name or full path (e.g. "python", "/usr/bin/ls")
 * @return A new curium_cmd_t* that must be freed with curium_cmd_free().
 */
curium_cmd_t* curium_cmd_new(const char* program);

/**
 * @brief Append a single argument to the command.
 *        Any character — including shell metacharacters — is safe here.
 * @param cmd The command builder.
 * @param arg The argument string to add.
 */
void curium_cmd_arg(curium_cmd_t* cmd, const char* arg);

/**
 * @brief Execute the command and capture its output.
 * @param cmd The fully built command.
 * @return A curium_cmd_result_t* with exit code, stdout and stderr.
 *         Must be freed with curium_cmd_result_free().
 */
curium_cmd_result_t* curium_cmd_run(curium_cmd_t* cmd);

/**
 * @brief Free a command builder created by curium_cmd_new().
 */
void curium_cmd_free(curium_cmd_t* cmd);

/**
 * @brief Free a result returned by curium_cmd_run().
 */
void curium_cmd_result_free(curium_cmd_result_t* result);

#ifdef __cplusplus
}
#endif

#endif /* CURIUM_CMD_H */
