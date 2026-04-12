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
#include "curium/cmd.h"
#include "curium/memory.h"
#include "curium/error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Internal structure                                                  */
/* ------------------------------------------------------------------ */

#define CURIUM_CMD_MAX_ARGS 256

struct curium_cmd {
    const char* argv[CURIUM_CMD_MAX_ARGS + 1]; /* NULL-terminated arg array  */
    int         argc;                       /* current argument count     */
};

/* ------------------------------------------------------------------ */
/*  Common helpers                                                      */
/* ------------------------------------------------------------------ */

curium_cmd_t* curium_cmd_new(const char* program) {
    if (!program) return NULL;
    curium_cmd_t* cmd = (curium_cmd_t*)curium_alloc(sizeof(curium_cmd_t), "curium_cmd");
    if (!cmd) return NULL;
    memset(cmd, 0, sizeof(curium_cmd_t));
    cmd->argv[0] = program;
    cmd->argc    = 1;
    return cmd;
}

void curium_cmd_arg(curium_cmd_t* cmd, const char* arg) {
    if (!cmd || !arg) return;
    if (cmd->argc >= CURIUM_CMD_MAX_ARGS) {
        curium_error_set(CURIUM_ERROR_OVERFLOW, "curium_cmd: too many arguments (max 256)");
        return;
    }
    cmd->argv[cmd->argc++] = arg;
    cmd->argv[cmd->argc]   = NULL; /* keep array NULL-terminated */
}

void curium_cmd_free(curium_cmd_t* cmd) {
    if (cmd) curium_free(cmd);
}

void curium_cmd_result_free(curium_cmd_result_t* r) {
    if (!r) return;
    if (r->stdout_output) curium_string_free(r->stdout_output);
    if (r->stderr_output) curium_string_free(r->stderr_output);
    curium_free(r);
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

typedef struct {
    HANDLE pipe;
    char** output;
} win_read_thread_params;

static DWORD WINAPI win_read_thread_proc(LPVOID param) {
    win_read_thread_params* p = (win_read_thread_params*)param;
    if (p) *p->output = win_read_pipe(p->pipe);
    return 0;
}

curium_cmd_result_t* curium_cmd_run(curium_cmd_t* cmd) {
    if (!cmd || cmd->argc == 0) return NULL;

    curium_cmd_result_t* result = (curium_cmd_result_t*)curium_alloc(sizeof(curium_cmd_result_t), "curium_cmd_result");
    if (!result) return NULL;
    result->exit_code     = -1;
    result->stdout_output = curium_string_new("");
    result->stderr_output = curium_string_new("");

    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
    HANDLE stdout_r, stdout_w;
    HANDLE stderr_r, stderr_w;

    if (!CreatePipe(&stdout_r, &stdout_w, &sa, 0) ||
        !CreatePipe(&stderr_r, &stderr_w, &sa, 0)) {
        curium_error_set(CURIUM_ERROR_IO, "curium_cmd: CreatePipe failed");
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

    char* cmdline = win_build_cmdline(cmd->argv);
    BOOL ok = CreateProcessA(NULL, cmdline, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi);
    free(cmdline);

    /* Close write ends in parent */
    CloseHandle(stdout_w);
    CloseHandle(stderr_w);

    if (!ok) {
        curium_error_set(CURIUM_ERROR_IO, "curium_cmd: CreateProcess failed");
        CloseHandle(stdout_r);
        CloseHandle(stderr_r);
        return result;
    }

    /* Start threads to read pipes concurrently to prevent deadlocks */
    char *out = NULL, *err = NULL;
    win_read_thread_params params_out = { stdout_r, &out };
    win_read_thread_params params_err = { stderr_r, &err };
    
    HANDLE threads[2];
    threads[0] = CreateThread(NULL, 0, win_read_thread_proc, &params_out, 0, NULL);
    threads[1] = CreateThread(NULL, 0, win_read_thread_proc, &params_err, 0, NULL);

    /* Wait for both reading threads to finish */
    WaitForMultipleObjects(2, threads, TRUE, INFINITE);
    
    CloseHandle(threads[0]);
    CloseHandle(threads[1]);
    CloseHandle(stdout_r);
    CloseHandle(stderr_r);

    /* Wait for child to exit */
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exit_code = 0;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    result->exit_code = (int)exit_code;

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    if (out) { curium_string_set(result->stdout_output, out); free(out); }
    if (err) { curium_string_set(result->stderr_output, err); free(err); }

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

curium_cmd_result_t* curium_cmd_run(curium_cmd_t* cmd) {
    if (!cmd || cmd->argc == 0) return NULL;

    curium_cmd_result_t* result = (curium_cmd_result_t*)curium_alloc(sizeof(curium_cmd_result_t), "curium_cmd_result");
    if (!result) return NULL;
    result->exit_code     = -1;
    result->stdout_output = curium_string_new("");
    result->stderr_output = curium_string_new("");

    int stdout_pipe[2], stderr_pipe[2];
    if (pipe(stdout_pipe) < 0 || pipe(stderr_pipe) < 0) {
        curium_error_set(CURIUM_ERROR_IO, "curium_cmd: pipe() failed");
        return result;
    }

    pid_t pid = fork();

    if (pid < 0) {
        curium_error_set(CURIUM_ERROR_IO, "curium_cmd: fork() failed");
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

    if (out) { curium_string_set(result->stdout_output, out); free(out); }
    if (err) { curium_string_set(result->stderr_output, err); free(err); }

    return result;
}

#endif /* _WIN32 */
