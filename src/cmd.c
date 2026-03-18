/**
 * @file cmd.c
 * @brief CM CMD SDK implementation.
 *
 * Cross-platform, shell-bypass command execution:
 *   - Windows: CreateProcess() + anonymous pipes
 *   - Unix:    fork() + execvp() + pipes
 *
 * No shell is ever invoked, so user-supplied arguments cannot
 * contain shell injection payloads.
 */
#include "cm/cmd.h"
#include "cm/memory.h"
#include "cm/error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Internal structure                                                  */
/* ------------------------------------------------------------------ */

#define CM_CMD_MAX_ARGS 256

struct cm_cmd {
    const char* argv[CM_CMD_MAX_ARGS + 1]; /* NULL-terminated arg array  */
    int         argc;                       /* current argument count     */
};

/* ------------------------------------------------------------------ */
/*  Common helpers                                                      */
/* ------------------------------------------------------------------ */

cm_cmd_t* cm_cmd_new(const char* program) {
    if (!program) return NULL;
    cm_cmd_t* cmd = (cm_cmd_t*)cm_alloc(sizeof(cm_cmd_t), "cm_cmd");
    if (!cmd) return NULL;
    memset(cmd, 0, sizeof(cm_cmd_t));
    cmd->argv[0] = program;
    cmd->argc    = 1;
    return cmd;
}

void cm_cmd_arg(cm_cmd_t* cmd, const char* arg) {
    if (!cmd || !arg) return;
    if (cmd->argc >= CM_CMD_MAX_ARGS) {
        cm_error_set(CM_ERROR_OVERFLOW, "cm_cmd: too many arguments (max 256)");
        return;
    }
    cmd->argv[cmd->argc++] = arg;
    cmd->argv[cmd->argc]   = NULL; /* keep array NULL-terminated */
}

void cm_cmd_free(cm_cmd_t* cmd) {
    if (cmd) cm_free(cmd);
}

void cm_cmd_result_free(cm_cmd_result_t* r) {
    if (!r) return;
    if (r->stdout_output) cm_string_free(r->stdout_output);
    if (r->stderr_output) cm_string_free(r->stderr_output);
    cm_free(r);
}

/* ------------------------------------------------------------------ */
/*  Platform implementations                                            */
/* ------------------------------------------------------------------ */

#ifdef _WIN32
/* ================================================================== */
/*  WINDOWS — CreateProcess + anonymous pipes                          */
/* ================================================================== */
#include <windows.h>

/** Read everything from a Windows pipe handle into a heap string. */
static char* win_read_pipe(HANDLE pipe) {
    size_t cap  = 4096;
    size_t used = 0;
    char*  buf  = (char*)malloc(cap);
    if (!buf) return NULL;

    DWORD bytes_read;
    char  chunk[1024];
    while (ReadFile(pipe, chunk, sizeof(chunk) - 1, &bytes_read, NULL) && bytes_read > 0) {
        if (used + bytes_read + 1 > cap) {
            cap  = (used + bytes_read + 1) * 2;
            buf  = (char*)realloc(buf, cap);
            if (!buf) return NULL;
        }
        memcpy(buf + used, chunk, bytes_read);
        used += bytes_read;
    }
    buf[used] = '\0';
    return buf;
}

/** Build the full command-line string (argv[0] + args) for CreateProcess.
 *  We pass NULL for lpApplicationName so Windows can PATH-resolve names. */
static char* win_build_cmdline(const char* const* argv) {
    size_t total = 2;
    for (int i = 0; argv[i]; i++) {
        total += strlen(argv[i]) + 4; /* space + two quotes + possible escape */
    }
    char* line = (char*)malloc(total);
    if (!line) return NULL;
    line[0] = '\0';

    for (int i = 0; argv[i]; i++) {
        if (i > 0) strcat(line, " ");
        strcat(line, "\"");
        strcat(line, argv[i]);
        strcat(line, "\"");
    }
    return line;
}

cm_cmd_result_t* cm_cmd_run(cm_cmd_t* cmd) {
    if (!cmd || cmd->argc == 0) return NULL;

    cm_cmd_result_t* result = (cm_cmd_result_t*)cm_alloc(sizeof(cm_cmd_result_t), "cm_cmd_result");
    if (!result) return NULL;
    result->exit_code     = -1;
    result->stdout_output = cm_string_new("");
    result->stderr_output = cm_string_new("");

    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
    HANDLE stdout_r, stdout_w;
    HANDLE stderr_r, stderr_w;

    if (!CreatePipe(&stdout_r, &stdout_w, &sa, 0) ||
        !CreatePipe(&stderr_r, &stderr_w, &sa, 0)) {
        cm_error_set(CM_ERROR_IO, "cm_cmd: CreatePipe failed");
        return result;
    }

    SetHandleInformation(stdout_r, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(stderr_r, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si;
    ZeroMemory(&si, sizeof(si));
    si.cb         = sizeof(si);
    si.hStdOutput = stdout_w;
    si.hStdError  = stderr_w;
    si.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);
    si.dwFlags    = STARTF_USESTDHANDLES;

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    /* Build full command line including argv[0]; pass NULL lpApplicationName
       so Windows searches PATH for the executable automatically */
    char* cmdline = win_build_cmdline(cmd->argv);

    BOOL ok = CreateProcessA(
        NULL,     /* lpApplicationName — NULL so PATH lookup works */
        cmdline,  /* lpCommandLine — full quoted command + args    */
        NULL, NULL, TRUE, 0, NULL, NULL,
        &si, &pi
    );

    free(cmdline);

    /* Close write ends in the parent after launching child */
    CloseHandle(stdout_w);
    CloseHandle(stderr_w);

    if (!ok) {
        cm_error_set(CM_ERROR_IO, "cm_cmd: CreateProcess failed");
        CloseHandle(stdout_r);
        CloseHandle(stderr_r);
        return result;
    }

    /* Read output */
    char* out = win_read_pipe(stdout_r);
    char* err = win_read_pipe(stderr_r);

    CloseHandle(stdout_r);
    CloseHandle(stderr_r);

    /* Wait for child */
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exit_code = 0;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    result->exit_code = (int)exit_code;

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    if (out) { cm_string_set(result->stdout_output, out); free(out); }
    if (err) { cm_string_set(result->stderr_output, err); free(err); }

    return result;
}

#else
/* ================================================================== */
/*  UNIX (Linux / macOS) — fork + execvp + pipes                      */
/* ================================================================== */
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

/** Read everything from a file descriptor into a heap string. */
static char* unix_read_fd(int fd) {
    size_t cap  = 4096;
    size_t used = 0;
    char*  buf  = (char*)malloc(cap);
    if (!buf) return NULL;

    char   chunk[1024];
    ssize_t n;
    while ((n = read(fd, chunk, sizeof(chunk))) > 0) {
        if (used + (size_t)n + 1 > cap) {
            cap = (used + (size_t)n + 1) * 2;
            buf = (char*)realloc(buf, cap);
            if (!buf) return NULL;
        }
        memcpy(buf + used, chunk, (size_t)n);
        used += (size_t)n;
    }
    buf[used] = '\0';
    return buf;
}

cm_cmd_result_t* cm_cmd_run(cm_cmd_t* cmd) {
    if (!cmd || cmd->argc == 0) return NULL;

    cm_cmd_result_t* result = (cm_cmd_result_t*)cm_alloc(sizeof(cm_cmd_result_t), "cm_cmd_result");
    if (!result) return NULL;
    result->exit_code     = -1;
    result->stdout_output = cm_string_new("");
    result->stderr_output = cm_string_new("");

    int stdout_pipe[2], stderr_pipe[2];
    if (pipe(stdout_pipe) < 0 || pipe(stderr_pipe) < 0) {
        cm_error_set(CM_ERROR_IO, "cm_cmd: pipe() failed");
        return result;
    }

    pid_t pid = fork();

    if (pid < 0) {
        cm_error_set(CM_ERROR_IO, "cm_cmd: fork() failed");
        close(stdout_pipe[0]); close(stdout_pipe[1]);
        close(stderr_pipe[0]); close(stderr_pipe[1]);
        return result;
    }

    if (pid == 0) {
        /* ---- Child process ---- */
        /* Redirect stdout/stderr to pipe write ends */
        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stderr_pipe[1], STDERR_FILENO);

        /* Close all pipe fds */
        close(stdout_pipe[0]); close(stdout_pipe[1]);
        close(stderr_pipe[0]); close(stderr_pipe[1]);

        /* execvp replaces this process — argv[0] = program, never hits shell */
        execvp(cmd->argv[0], (char* const*)cmd->argv);

        /* If we get here, execvp failed */
        _exit(127);
    }

    /* ---- Parent process ---- */
    close(stdout_pipe[1]);
    close(stderr_pipe[1]);

    char* out = unix_read_fd(stdout_pipe[0]);
    char* err = unix_read_fd(stderr_pipe[0]);

    close(stdout_pipe[0]);
    close(stderr_pipe[0]);

    int status = 0;
    waitpid(pid, &status, 0);
    result->exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;

    if (out) { cm_string_set(result->stdout_output, out); free(out); }
    if (err) { cm_string_set(result->stderr_output, err); free(err); }

    return result;
}

#endif /* _WIN32 */
