#include "curium/file.h"
#include "curium/memory.h"
#include "curium/error.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

int curium_file_exists(const char* filepath) {
    if (!filepath) return 0;
    struct stat buffer;
    return (stat(filepath, &buffer) == 0);
}

curium_string_t* curium_file_read(const char* filepath) {
    if (!filepath) {
        curium_error_set(-1, "Filepath to curium_file_read cannot be NULL.");
        return NULL;
    }

    FILE* fp = fopen(filepath, "rb");
    if (!fp) {
        curium_error_set(-1, "File missing or permissions denied in curium_file_read.");
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (fsize < 0) {
        fclose(fp);
        curium_error_set(-1, "Unable to deduce file size in curium_file_read.");
        return NULL;
    }

    char* buffer = (char*)malloc(fsize + 1);
    if (!buffer) {
        fclose(fp);
        curium_error_set(-1, "Insufficient native memory allocated for file read.");
        return NULL;
    }

    size_t bytes_read = fread(buffer, 1, (size_t)fsize, fp);
    if (bytes_read != (size_t)fsize) {
        free(buffer);
        buffer = NULL;
        fclose(fp);
        curium_error_set(-1, "Failed to read full file content in curium_file_read.");
        return NULL;
    }
    buffer[bytes_read] = '\0';
    
    fclose(fp);

    // Create a tracked CM string instance safely escaping the read bounds
    curium_string_t* str = curium_string_new(buffer);
    free(buffer);
    
    return str;
}

int curium_file_write(const char* filepath, const char* content) {
    if (!filepath || !content) return 0;

    FILE* fp = fopen(filepath, "wb");
    if (!fp) {
        curium_error_set(-1, "Unable to open physical file pointer for curium_file_write.");
        return 0;
    }

    size_t len = strlen(content);
    size_t written = fwrite(content, 1, len, fp);
    fclose(fp);

    return (written == len) ? 1 : 0;
}

curium_string_t* curium_path_normalize(const char* path) {
    if (!path) return NULL;
    char buffer[4096];
    char* resolved = NULL;

#ifdef _WIN32
    resolved = _fullpath(buffer, path, sizeof(buffer));
#else
    resolved = realpath(path, buffer);
#endif

    if (!resolved) {
        /* Fallback: if file doesn't exist, realpath might fail.
         * For the import resolver, we only normalize paths that we know exist. */
        return curium_string_new(path);
    }

    /* Normalize slashes to forward slashes for consistent comparison */
    for (int i = 0; resolved[i]; i++) {
        if (resolved[i] == '\\') resolved[i] = '/';
    }

    return curium_string_new(resolved);
}
