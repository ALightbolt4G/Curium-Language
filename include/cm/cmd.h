/**
 * @file cmd.h
 * @brief CM CMD SDK — Cross-platform, injection-safe command execution.
 *
 * Uses OS process APIs (CreateProcess on Windows, fork+execvp on Unix)
 * directly, bypassing the shell entirely. Arguments are passed as an array,
 * so shell metacharacters like ; | && $() ` are never interpreted.
 *
 * Usage:
 *   cm_cmd_t* cmd = cm_cmd_new("python");
 *   cm_cmd_arg(cmd, "-m");
 *   cm_cmd_arg(cmd, "yt_dlp");
 *   cm_cmd_arg(cmd, user_url);   // safe — no injection possible
 *   cm_cmd_result_t* r = cm_cmd_run(cmd);
 *   printf("exit=%d stdout=%s\n", r->exit_code, r->stdout_output->data);
 *   cm_cmd_result_free(r);
 *   cm_cmd_free(cmd);
 */
#ifndef CM_CMD_H
#define CM_CMD_H

#include "core.h"
#include "string.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/*  Structures                                                          */
/* ------------------------------------------------------------------ */

/**
 * @brief Opaque command builder. Add arguments with cm_cmd_arg().
 */
typedef struct cm_cmd cm_cmd_t;

/**
 * @brief Result of a finished command execution.
 */
typedef struct {
    int          exit_code;     /**< Process exit/return code            */
    cm_string_t* stdout_output; /**< Captured standard output            */
    cm_string_t* stderr_output; /**< Captured standard error             */
} cm_cmd_result_t;

/* ------------------------------------------------------------------ */
/*  API                                                                 */
/* ------------------------------------------------------------------ */

/**
 * @brief Create a new command, setting the program to execute.
 * @param program The executable name or full path (e.g. "python", "/usr/bin/ls")
 * @return A new cm_cmd_t* that must be freed with cm_cmd_free().
 */
cm_cmd_t* cm_cmd_new(const char* program);

/**
 * @brief Append a single argument to the command.
 *        Any character — including shell metacharacters — is safe here.
 * @param cmd The command builder.
 * @param arg The argument string to add.
 */
void cm_cmd_arg(cm_cmd_t* cmd, const char* arg);

/**
 * @brief Execute the command and capture its output.
 * @param cmd The fully built command.
 * @return A cm_cmd_result_t* with exit code, stdout and stderr.
 *         Must be freed with cm_cmd_result_free().
 */
cm_cmd_result_t* cm_cmd_run(cm_cmd_t* cmd);

/**
 * @brief Free a command builder created by cm_cmd_new().
 */
void cm_cmd_free(cm_cmd_t* cmd);

/**
 * @brief Free a result returned by cm_cmd_run().
 */
void cm_cmd_result_free(cm_cmd_result_t* result);

#ifdef __cplusplus
}
#endif

#endif /* CM_CMD_H */
