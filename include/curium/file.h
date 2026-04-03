/**
 * @file file.h
 * @brief CM File System Operations natively tracked by the Garbage Collector.
 */

#ifndef CURIUM_FILE_H
#define CURIUM_FILE_H

#include "curium/string.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Checks if a file exists on disk efficiently.
 * 
 * @param filepath The absolute or relative system path
 * @return 1 if found, 0 if missing.
 */
int curium_file_exists(const char* filepath);

/**
 * @brief Safely reads a complete file from disk into a CM tracked string.
 * 
 * @param filepath The absolute or relative system path
 * @return curium_string_t* containing the byte data. NULL on read error.
 */
curium_string_t* curium_file_read(const char* filepath);

/**
 * @brief Directly writes a native string buffer payloads to physical disk natively.
 * 
 * @param filepath The absolute or relative system path
 * @param content The char* payload
 * @return 1 on success, 0 on failure.
 */
int curium_file_write(const char* filepath, const char* content);

#ifdef __cplusplus
}
#endif

#endif // CURIUM_FILE_H
