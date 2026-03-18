/**
 * @file thread.h
 * @brief OS-Agnostic Multi-threading and Concurrency Control API for the CM Enterprise Framework.
 */

#ifndef CM_THREAD_H
#define CM_THREAD_H

#ifdef __cplusplus
extern "C" {
#endif

/** Opaque threading primitives completely abstracting POSIX and Win32. */
typedef void* CMMutex;
typedef void* CMThread;

/**
 * @brief Initializes a fresh OS-level mutex cleanly inherently.
 * @return Allocated mutex native handle.
 */
CMMutex cm_mutex_init(void);

/**
 * @brief Locks a native mutex explicitly.
 */
void cm_mutex_lock(CMMutex mutex);

/**
 * @brief Unlocks a native mutex explicitly.
 */
void cm_mutex_unlock(CMMutex mutex);

/**
 * @brief Cleans up backing operating-system hooks seamlessly securely.
 */
void cm_mutex_destroy(CMMutex mutex);

/**
 * @brief Generates an isolated concurrent worker effectively safely.
 * @param func Entrypoint wrapper executing synchronously isolated processing
 * @param arg Pointer payload handed across boundaries
 * @return CMThread mapped reference bounds.
 */
CMThread cm_thread_create(void* (*func)(void*), void* arg);

/**
 * @brief Performs synchronous await tearing down execution securely resolving state.
 * @param thread Thread boundaries completing gracefully
 * @return 1 on zero-leak success, 0 internally on faults.
 */
int cm_thread_join(CMThread thread);

#ifdef __cplusplus
}
#endif

#endif // CM_THREAD_H
