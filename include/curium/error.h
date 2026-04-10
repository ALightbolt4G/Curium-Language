/**
 * @file error.h
 * @brief Error handling, exception structures, and fatal crash detection.
 */
#ifndef CURIUM_ERROR_H
#define CURIUM_ERROR_H

#include "core.h"
#include <setjmp.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Set the global thread-local error state actively manually.
 */
void curium_error_set(curium_error_code_t error, const char* message);

/**
 * @brief Return the last encountered error code organically safely.
 */
curium_error_code_t curium_error_get_last(void);

/**
 * @brief Return the message corresponding to the recent error trace securely.
 */
const char* curium_error_get_message(void);

/**
 * @brief Wipe existing global exceptions rendering trackers blank completely.
 */
void curium_error_clear(void);

/**
 * @brief Format and set a rich error message with a caret pointing to the source location.
 * @param src The full source code string (can be read-only).
 * @param filename The name of the file (e.g., "src/main.cm").
 * @param line The 1-indexed line number.
 * @param col The 1-indexed column number.
 * @param error The error code to set.
 * @param message The primary error message.
 * @param hint An optional hint (e.g., "add a semicolon").
 */
void curium_error_report_caret(const char* src, const char* filename, size_t line, size_t col, curium_error_code_t error, const char* message, const char* hint);


/**
 * @brief Bind system handlers for SIGSEGV, SIGABRT, SIGFPE, handling panics reliably globally.
 */
void curium_init_error_detector(void);

/* Structured Exception Handling */
typedef struct {
    jmp_buf buffer;
    int active;
    const char* file;
    int line;
} curium_exception_frame_t;

extern CURIUM_TLS curium_exception_frame_t* curium_current_frame;

#define CURIUM_TRY() \
    for (curium_exception_frame_t __curium_frame = {0}; !__curium_frame.active; __curium_frame.active = 1) \
        for (curium_exception_frame_t* __curium_prev = curium_current_frame; \
             !__curium_frame.active && (curium_current_frame = &__curium_frame, __curium_frame.active = 1); \
             curium_current_frame = __curium_prev) \
            if (setjmp(__curium_frame.buffer) == 0)

#define CURIUM_CATCH() else

#define CURIUM_THROW(err, msg) \
    do { \
        curium_error_set((err), (msg)); \
        if (curium_current_frame && curium_current_frame->active) { \
            longjmp(curium_current_frame->buffer, (err)); \
        } else { \
            fprintf(stderr, "FATAL: Uncaught exception: %s\n", (msg)); \
            exit((err) ? (err) : 1); \
        } \
    } while(0)

#ifdef __cplusplus
}
#endif

#endif /* CURIUM_ERROR_H */
