#include "curium/core.h"
#include "curium/memory.h"
#include "curium/error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#else
#include <unistd.h>
#include <libgen.h>
#include <locale.h>
#endif

void curium_init(void) {
#ifdef _WIN32
    /* Set console code page to UTF-8 */
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
    
    /* Enable ANSI escape sequences for colors */
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hOut, dwMode);
        }
    }

    /* Change working directory to exe path */
    char exe_path[MAX_PATH];
    if (GetModuleFileNameA(NULL, exe_path, MAX_PATH)) {
        char* last_backslash = strrchr(exe_path, '\\');
        if (last_backslash) {
            *last_backslash = '\0';
            _chdir(exe_path);
        }
    }
#else
    /* Set locale to UTF-8 on Unix-like systems */
    setlocale(LC_ALL, "");

    /* Change working directory to exe path on Linux/macOS */
    char exe_path[4096];
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len > 0) {
        exe_path[len] = '\0';
        char* dir = dirname(exe_path);
        chdir(dir);
    }
#endif

    /* Initialize memory management */
    curium_gc_init();

    /* Disable buffering for stdout/stderr to ensure logs are visible immediately */
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
}

void curium_shutdown(void) {
    /* Cleanup memory management */
    curium_gc_shutdown();
}

/* Random string implementation (moving from main or wherever it was if it's core) */
void curium_random_seed(unsigned int seed) {
    srand(seed);
}

void curium_random_string(char* buffer, size_t length) {
    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    for (size_t n = 0; n < length; n++) {
        int key = rand() % (int)(sizeof(charset) - 1);
        buffer[n] = charset[key];
    }
    buffer[length] = '\0';
}
