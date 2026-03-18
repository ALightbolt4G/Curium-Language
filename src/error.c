#include "cm/error.h"
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
#define cm_write _write
#else
#include <unistd.h>
#define cm_write write
#endif

__thread cm_exception_frame_t* cm_current_frame = NULL;

static cm_error_code_t cm_last_error = CM_SUCCESS;
static char cm_error_message[1024] = {0};

void cm_error_set(cm_error_code_t error, const char* message) {
    cm_last_error = error;
    if (message) {
        strncpy(cm_error_message, message, sizeof(cm_error_message) - 1);
        cm_error_message[sizeof(cm_error_message) - 1] = '\0';
    } else {
        cm_error_message[0] = '\0';
    }
}

cm_error_code_t cm_error_get_last(void) {
    return cm_last_error;
}

const char* cm_error_get_message(void) {
    return cm_error_message;
}

void cm_error_clear(void) {
    cm_last_error = CM_SUCCESS;
    cm_error_message[0] = '\0';
}

static void cm_signal_handler(int sig) {
    const char* sig_name = "Unknown Signal";
    switch(sig) {
        case SIGSEGV: sig_name = "SIGSEGV (Segmentation Fault - Null Dereference / Bad Pointer)"; break;
        case SIGABRT: sig_name = "SIGABRT (Abort)"; break;
        case SIGFPE:  sig_name = "SIGFPE (Floating Point Exception - Division by zero)"; break;
        case SIGILL:  sig_name = "SIGILL (Illegal Instruction)"; break;
    }
    
    /* In a real signal handler, printf is not async-signal-safe. We use write instead for critical crashes. */
    const char* header = "\n\nCRITICAL FATAL ERROR INTERCEPTED: ";
    cm_write(STDERR_FILENO, header, strlen(header));
    cm_write(STDERR_FILENO, sig_name, strlen(sig_name));
    
    const char* footer = "\nProcess terminated cleanly by CM Realtime Error Detector.\n\n";
    cm_write(STDERR_FILENO, footer, strlen(footer));
    
    _exit(1);
}

void cm_init_error_detector(void) {
    signal(SIGSEGV, cm_signal_handler);
    signal(SIGABRT, cm_signal_handler);
    signal(SIGFPE,  cm_signal_handler);
    signal(SIGILL,  cm_signal_handler);
}
