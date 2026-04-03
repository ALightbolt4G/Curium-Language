#ifndef CURIUM_PROJECT_H
#define CURIUM_PROJECT_H

#include "curium/core.h"
#include "curium/string.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Project Configuration API
 * Handles curium.json and package.json parsing for project metadata
 * ========================================================================== */

typedef struct {
    char name[128];
    char version[32];
    char description[256];
    char entry[256];
    char output[256];
    char curium_root[512];
} curium_project_config_t;

/* Load project configuration from curium.json or package.json
 * Returns 0 on success, -1 if no config found
 */
int curium_project_load_config(curium_project_config_t* config, const char* project_dir);

/* Get project name from config or directory name
 * Returns pointer to name (owned by config), never NULL
 */
const char* curium_project_get_name(curium_project_config_t* config);

/* Get entry point from config or default
 * Returns pointer to entry path, defaults to "src/main.curium"
 */
const char* curium_project_get_entry(curium_project_config_t* config);

/* Get output path from config or default
 * Returns pointer to output path, defaults to "a.out"
 */
const char* curium_project_get_output(curium_project_config_t* config);

/* Check if running in a project directory
 * Returns 1 if curium.json or package.json found, 0 otherwise
 */
int curium_project_detect(const char* path);

/* Generate default curium.json content
 * Returns newly allocated string (caller must free)
 */
curium_string_t* curium_project_generate_curium_json(const char* name, const char* description);

/* Generate default .gitignore content for CM projects
 * Returns newly allocated string (caller must free)
 */
curium_string_t* curium_project_generate_gitignore(void);

/* Generate README.md template
 * Returns newly allocated string (caller must free)
 */
curium_string_t* curium_project_generate_readme(const char* name);

#ifdef __cplusplus
}
#endif

#endif /* CURIUM_PROJECT_H */
