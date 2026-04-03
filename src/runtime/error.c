#include "curium/error.h"
#include "curium/error_detail.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#ifdef _WIN32
#include <io.h>
#include <process.h>
#ifndef STDERR_FILENO
#define STDERR_FILENO 2
#endif
#define curium_write _write
#else
#include <unistd.h>
#define curium_write write
#endif

__thread curium_exception_frame_t* curium_current_frame = NULL;

static curium_error_code_t curium_last_error = CURIUM_SUCCESS;
static char curium_error_message[1024] = {0};

void curium_error_set(curium_error_code_t error, const char* message) {
    curium_last_error = error;
    if (message) {
        strncpy(curium_error_message, message, sizeof(curium_error_message) - 1);
        curium_error_message[sizeof(curium_error_message) - 1] = '\0';
    } else {
        curium_error_message[0] = '\0';
    }
}

curium_error_code_t curium_error_get_last(void) {
    return curium_last_error;
}

const char* curium_error_get_message(void) {
    return curium_error_message;
}

void curium_error_clear(void) {
    curium_last_error = CURIUM_SUCCESS;
    curium_error_message[0] = '\0';
}

void curium_error_report_caret(const char* src, const char* filename, size_t line, size_t col, curium_error_code_t error, const char* message, const char* hint) {
    curium_last_error = error;
    
    if (!src) {
        snprintf(curium_error_message, sizeof(curium_error_message), "%s", message);
        return;
    }

    /* Extract the specific line from the source code */
    const char* line_start = src;
    size_t current_line = 1;
    while (current_line < line && *line_start != '\0') {
        if (*line_start == '\n') current_line++;
        line_start++;
    }
    
    const char* line_end = line_start;
    while (*line_end != '\n' && *line_end != '\0') {
        line_end++;
    }
    
    size_t line_len = line_end - line_start;
    char source_line[256] = {0};
    if (line_len > sizeof(source_line) - 1) line_len = sizeof(source_line) - 1;
    strncpy(source_line, line_start, line_len);
    source_line[line_len] = '\0';

    /* Build the caret string */
    char caret_line[256] = {0};
    size_t caret_pos = (col > 0) ? col - 1 : 0;
    if (caret_pos > sizeof(caret_line) - 2) caret_pos = sizeof(caret_line) - 2;
    
    for (size_t i = 0; i < caret_pos; i++) {
        caret_line[i] = ' ';
    }
    caret_line[caret_pos] = '^';
    caret_line[caret_pos + 1] = '\0';

    /* Format the final message */
    if (hint) {
        snprintf(curium_error_message, sizeof(curium_error_message),
            "error[E%04d]: %s\n"
            "  --> %s:%zu:%zu\n"
            "   |\n"
            "%2zu | %s\n"
            "   | %s %s\n"
            "   |\n"
            "   = help: %s",
            error, message, filename ? filename : "<source>", line, col, line, source_line, caret_line, message, hint);
    } else {
        snprintf(curium_error_message, sizeof(curium_error_message),
            "error[E%04d]: %s\n"
            "  --> %s:%zu:%zu\n"
            "   |\n"
            "%2zu | %s\n"
            "   | %s %s\n"
            "   |",
            error, message, filename ? filename : "<source>", line, col, line, source_line, caret_line, message);
    }
}

static void curium_signal_handler(int sig) {
    const char* sig_name = "Unknown Signal";
    curium_error_code_t code = CURIUM_ERROR_UNKNOWN;
    const char* obj_type = "unknown";
    const char* status = "Unknown error";
    const char* fix = "Check error logs for details";
    
    switch(sig) {
        case SIGSEGV: 
            sig_name = "SIGSEGV (Segmentation Fault)";
            code = CURIUM_ERROR_NULL_POINTER;
            obj_type = "pointer";
            status = "Accessed invalid memory address";
            fix = "Check for null pointer dereference or use-after-free";
            break;
        case SIGABRT: 
            sig_name = "SIGABRT (Abort)";
            code = CURIUM_ERROR_RUNTIME;
            status = "Program aborted";
            fix = "Check assertion failures or abort() calls";
            break;
        case SIGFPE:  
            sig_name = "SIGFPE (Floating Point Exception)";
            code = CURIUM_ERROR_DIVISION_BY_ZERO;
            obj_type = "number";
            status = "Division by zero or invalid math operation";
            fix = "Check denominator is not zero before dividing";
            break;
        case SIGILL:  
            sig_name = "SIGILL (Illegal Instruction)";
            code = CURIUM_ERROR_RUNTIME;
            status = "Illegal instruction executed";
            fix = "Check for corrupted binary or invalid code generation";
            break;
    }
    
    /* Build enhanced error message using error_detail system */
    curium_error_detail_t detail;
    curium_error_detail_init(&detail);
    detail.code = code;
    detail.severity = CURIUM_SEVERITY_FATAL;
    curium_error_detail_set_location(&detail, "unknown", 0, 0);
    curium_error_detail_set_object(&detail, obj_type, "unknown");
    curium_error_detail_set_message(&detail, "%s", status);
    curium_error_detail_set_suggestion(&detail, "%s", fix);
    
    /* For critical crashes, we use async-signal-safe write */
    if (sig == SIGSEGV) {
        const char* neon_err = 
            "\n\n\xE2\x9A\xA0\xEF\xB8\x8F  CM Runtime Error:\n"
            "   Location: unknown -> Line 0\n"
            "   Object: ptr pointer unknown\n"
            "   Status: Accessed invalid memory address\n"
            "   Fix: Check for null pointer dereference or use-after-free\n\n";
        curium_write(STDERR_FILENO, neon_err, strlen(neon_err));
    } else {
        const char* header = "\n\n\xF0\x9F\x92\xA5 CRITICAL FATAL ERROR: ";
        curium_write(STDERR_FILENO, header, strlen(header));
        curium_write(STDERR_FILENO, sig_name, strlen(sig_name));
        curium_write(STDERR_FILENO, "\n", 1);
        
        const char* status_msg = "   Status: ";
        curium_write(STDERR_FILENO, status_msg, strlen(status_msg));
        curium_write(STDERR_FILENO, status, strlen(status));
        curium_write(STDERR_FILENO, "\n", 1);
        
        const char* fix_msg = "   Fix: ";
        curium_write(STDERR_FILENO, fix_msg, strlen(fix_msg));
        curium_write(STDERR_FILENO, fix, strlen(fix));
        curium_write(STDERR_FILENO, "\n\n", 2);
    }
    
    _exit(1);
}

void curium_init_error_detector(void) {
    signal(SIGSEGV, curium_signal_handler);
    signal(SIGABRT, curium_signal_handler);
    signal(SIGFPE,  curium_signal_handler);
    signal(SIGILL,  curium_signal_handler);
}
