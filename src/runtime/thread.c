    #include "curium/thread.h"
    #include <stdlib.h>

    #ifdef _WIN32
    #include <windows.h>
    #else
    #include <pthread.h>
    #endif

    CMMutex curium_mutex_init(void) {
    #ifdef _WIN32
        CRITICAL_SECTION* cs = (CRITICAL_SECTION*)malloc(sizeof(CRITICAL_SECTION));
        if (cs) InitializeCriticalSection(cs);
        return (CMMutex)cs;
    #else
        pthread_mutex_t* lock = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
        if (lock) pthread_mutex_init(lock, NULL);
        return (CMMutex)lock;
    #endif
    }

    void curium_mutex_lock(CMMutex mutex) {
        if (!mutex) return;
    #ifdef _WIN32
        EnterCriticalSection((CRITICAL_SECTION*)mutex);
    #else
        pthread_mutex_lock((pthread_mutex_t*)mutex);
    #endif
    }

    void curium_mutex_unlock(CMMutex mutex) {
        if (!mutex) return;
    #ifdef _WIN32
        LeaveCriticalSection((CRITICAL_SECTION*)mutex);
    #else
        pthread_mutex_unlock((pthread_mutex_t*)mutex);
    #endif
    }

    void curium_mutex_destroy(CMMutex mutex) {
        if (!mutex) return;
    #ifdef _WIN32
        DeleteCriticalSection((CRITICAL_SECTION*)mutex);
        free(mutex);
    #else
        pthread_mutex_destroy((pthread_mutex_t*)mutex);
        free(mutex);
    #endif
    }

    #ifdef _WIN32
    typedef struct {
        void* (*func)(void*);
        void* arg;
    } ThreadArgs;

    static DWORD WINAPI ThreadWrapper(LPVOID lpParam) {
        ThreadArgs* ta = (ThreadArgs*)lpParam;
        if (ta && ta->func) {
            ta->func(ta->arg);
        }
        if (ta) free(ta);
        return 0;
    }
    #endif

    CMThread curium_thread_create(void* (*func)(void*), void* arg) {
        if (!func) return NULL;
    #ifdef _WIN32
        ThreadArgs* ta = (ThreadArgs*)malloc(sizeof(ThreadArgs));
        if (!ta) return NULL;
        ta->func = func;
        ta->arg = arg;
        HANDLE hThread = CreateThread(NULL, 0, ThreadWrapper, ta, 0, NULL);
        return (CMThread)hThread;
    #else
        pthread_t* pt = (pthread_t*)malloc(sizeof(pthread_t));
        if (!pt) return NULL;
        if (pthread_create(pt, NULL, func, arg) != 0) {
            free(pt);
            return NULL;
        }
        return (CMThread)pt;
    #endif
    }

    int curium_thread_join(CMThread thread) {
        if (!thread) return 0;
    #ifdef _WIN32
        WaitForSingleObject((HANDLE)thread, INFINITE);
        CloseHandle((HANDLE)thread);
        return 1;
    #else
        pthread_t* pt = (pthread_t*)thread;
        int res = (pthread_join(*pt, NULL) == 0) ? 1 : 0;
        free(pt);
        return res;
    #endif
    }
