#ifndef CURIUM_PACKAGES_H
#define CURIUM_PACKAGES_H

#include "curium/core.h"
#include "curium/string.h"

/* ============================================================================
 * CM Package Manager
 * A modern package manager for CM language (similar to npm/cargo)
 * ========================================================================== */

#define CURIUM_PACKAGES_DIR "~/.curium/packages"
#define CURIUM_REGISTRY_URL "https://packages.curium-lang.org"
#define CURIUM_PACKAGE_FILE "curium.json"

/* Package manifest structure */
typedef struct {
    curium_string_t* name;
    curium_string_t* version;
    curium_string_t* description;
    curium_string_t* main;
    curium_map_t* dependencies;      /* name -> version constraint */
    curium_map_t* dev_dependencies;   /* name -> version constraint */
    curium_string_t* registry;
} curium_package_manifest_t;

/* Package installation result */
typedef struct {
    int success;
    curium_string_t* package_name;
    curium_string_t* version;
    curium_string_t* install_path;
    curium_string_t* error_message;
} curium_package_result_t;

/* Package manager context */
typedef struct {
    curium_string_t* project_root;
    curium_string_t* packages_dir;
    curium_string_t* registry_url;
    curium_package_manifest_t* manifest;
} curium_package_manager_t;

/* Initialize package manager */
curium_package_manager_t* curium_packages_init(const char* project_root);
void curium_packages_free(curium_package_manager_t* pm);

/* Manifest operations */
curium_package_manifest_t* curium_packages_load_manifest(const char* path);
int curium_packages_save_manifest(const char* path, curium_package_manifest_t* manifest);
void curium_packages_manifest_free(curium_package_manifest_t* manifest);

/* Package operations */
int curium_packages_install(curium_package_manager_t* pm, const char* package_name, const char* version);
int curium_packages_install_all(curium_package_manager_t* pm);
int curium_packages_remove(curium_package_manager_t* pm, const char* package_name);
int curium_packages_update(curium_package_manager_t* pm, const char* package_name);

/* Registry operations */
curium_string_t* curium_packages_fetch_package_info(const char* registry_url, const char* package_name);
curium_string_t* curium_packages_download_package(const char* registry_url, const char* package_name, 
                                           const char* version, const char* dest_path);

/* Dependency resolution */
curium_map_t* curium_packages_resolve_dependencies(curium_package_manager_t* pm);
int curium_packages_check_conflicts(curium_map_t* resolved_deps);

/* CLI commands */
int curium_packages_cmd_init(const char* project_name);
int curium_packages_cmd_install(const char* package_name, const char* version);
int curium_packages_cmd_remove(const char* package_name);
int curium_packages_cmd_update(const char* package_name);
int curium_packages_cmd_list(void);
int curium_packages_cmd_search(const char* query);

/* Utility functions */
curium_string_t* curium_packages_get_cache_dir(void);
curium_string_t* curium_packages_expand_path(const char* path);
int curium_packages_ensure_dir(const char* path);
curium_string_t* curium_packages_hash_file(const char* path);

#endif /* CURIUM_PACKAGES_H */
