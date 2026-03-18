/**
 * @file error.h
 * @brief Error handling, exception structures, and fatal crash detection.
 */
#ifndef CM_ERROR_H
#define CM_ERROR_H

#include "core.h"
#include <setjmp.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Set the global thread-local error state actively manually.
 */
void cm_error_set(cm_error_code_t error, const char* message);

/**
 * @brief Return the last encountered error code organically safely.
 */
cm_error_code_t cm_error_get_last(void);

/**
 * @brief Return the message corresponding to the recent error trace securely.
 */
const char* cm_error_get_message(void);

/**
 * @brief Wipe existing global exceptions rendering trackers blank completely.
 */
void cm_error_clear(void);

/**
 * @brief Bind system handlers for SIGSEGV, SIGABRT, SIGFPE, handling panics reliably globally.
 */
void cm_init_error_detector(void);

/* Structured Exception Handling */
typedef struct {
    jmp_buf buffer;
    int active;
    const char* file;
    int line;
} cm_exception_frame_t;

extern __thread cm_exception_frame_t* cm_current_frame;

#define CM_TRY() \
    for (cm_exception_frame_t __cm_frame = {0}; !__cm_frame.active; __cm_frame.active = 1) \
        for (cm_exception_frame_t* __cm_prev = cm_current_frame; \
             !__cm_frame.active && (cm_current_frame = &__cm_frame, __cm_frame.active = 1); \
             cm_current_frame = __cm_prev) \
            if (setjmp(__cm_frame.buffer) == 0)

#define CM_CATCH() else

#define CM_THROW(err, msg) \
    do { \
        cm_error_set((err), (msg)); \
        if (cm_current_frame && cm_current_frame->active) { \
            longjmp(cm_current_frame->buffer, (err)); \
        } else { \
            fprintf(stderr, "FATAL: Uncaught exception: %s\n", (msg)); \
            exit((err) ? (err) : 1); \
        } \
    } while(0)

#ifdef __cplusplus
}
#endif

#endif /* CM_ERROR_H */
