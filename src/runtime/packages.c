#include "curium/packages.h"
#include "curium/file.h"
#include "curium/json.h"
#include "curium/map.h"
#include "curium/memory.h"
#include "curium/error.h"
#include "curium/cmd.h"
#include "curium/http.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#define mkdir(path, mode) _mkdir(path)
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

/* ============================================================================
 * Utility Functions
 * ========================================================================== */

curium_string_t* curium_packages_expand_path(const char* path) {
    if (!path) return NULL;
    
    /* Handle tilde expansion for home directory */
    if (path[0] == '~') {
        const char* home = getenv("HOME");
        if (!home) home = getenv("USERPROFILE");
        if (home) {
            return curium_string_format("%s%s", home, path + 1);
        }
    }
    return curium_string_new(path);
}

int curium_packages_ensure_dir(const char* path) {
    if (!path) return -1;
    
#ifdef _WIN32
    return _mkdir(path) == 0 || errno == EEXIST ? 0 : -1;
#else
    return mkdir(path, 0755) == 0 || errno == EEXIST ? 0 : -1;
#endif
}

curium_string_t* curium_packages_get_cache_dir(void) {
    const char* home = getenv("HOME");
    if (!home) home = getenv("USERPROFILE");
    if (!home) home = ".";
    
    return curium_string_format("%s/.cm/packages", home);
}

/* ============================================================================
 * Manifest Operations
 * ========================================================================== */

curium_package_manifest_t* curium_packages_manifest_new(void) {
    curium_package_manifest_t* manifest = (curium_package_manifest_t*)curium_alloc(sizeof(curium_package_manifest_t), "curium_package_manifest");
    if (!manifest) return NULL;
    
    memset(manifest, 0, sizeof(*manifest));
    manifest->dependencies = curium_map_new();
    manifest->dev_dependencies = curium_map_new();
    
    return manifest;
}

void curium_packages_manifest_free(curium_package_manifest_t* manifest) {
    if (!manifest) return;
    
    if (manifest->name) curium_string_free(manifest->name);
    if (manifest->version) curium_string_free(manifest->version);
    if (manifest->description) curium_string_free(manifest->description);
    if (manifest->main) curium_string_free(manifest->main);
    if (manifest->registry) curium_string_free(manifest->registry);
    if (manifest->dependencies) curium_map_free(manifest->dependencies);
    if (manifest->dev_dependencies) curium_map_free(manifest->dev_dependencies);
    
    curium_free(manifest);
}

curium_package_manifest_t* curium_packages_load_manifest(const char* path) {
    if (!path) return NULL;
    
    curium_string_t* content = curium_file_read(path);
    if (!content) {
        curium_error_set(CURIUM_ERROR_IO, "Failed to read package manifest");
        return NULL;
    }
    
    struct CuriumJsonNode* root = curium_json_parse(content->data);
    curium_string_free(content);
    
    if (!root || root->type != CURIUM_JSON_OBJECT) {
        curium_error_set(CURIUM_ERROR_PARSE, "Invalid package manifest format");
        if (root) CuriumJsonNode_delete(root);
        return NULL;
    }
    
    curium_package_manifest_t* manifest = curium_packages_manifest_new();
    if (!manifest) {
        CuriumJsonNode_delete(root);
        return NULL;
    }
    
    curium_map_t* obj = root->value.object_val;
    
    /* Parse basic fields */
    struct CuriumJsonNode** node;
    
    node = (struct CuriumJsonNode**)curium_map_get(obj, "name");
    if (node && (*node)->type == CURIUM_JSON_STRING) {
        manifest->name = curium_string_new((*node)->value.string_val->data);
    }
    
    node = (struct CuriumJsonNode**)curium_map_get(obj, "version");
    if (node && (*node)->type == CURIUM_JSON_STRING) {
        manifest->version = curium_string_new((*node)->value.string_val->data);
    }
    
    node = (struct CuriumJsonNode**)curium_map_get(obj, "description");
    if (node && (*node)->type == CURIUM_JSON_STRING) {
        manifest->description = curium_string_new((*node)->value.string_val->data);
    }
    
    node = (struct CuriumJsonNode**)curium_map_get(obj, "main");
    if (node && (*node)->type == CURIUM_JSON_STRING) {
        manifest->main = curium_string_new((*node)->value.string_val->data);
    }
    
    node = (struct CuriumJsonNode**)curium_map_get(obj, "registry");
    if (node && (*node)->type == CURIUM_JSON_STRING) {
        manifest->registry = curium_string_new((*node)->value.string_val->data);
    }
    
    /* Parse dependencies */
    node = (struct CuriumJsonNode**)curium_map_get(obj, "dependencies");
    if (node && (*node)->type == CURIUM_JSON_OBJECT) {
        curium_map_t* deps = (*node)->value.object_val;
        (void)deps; /* TODO: Implement dependency parsing */
        /* Iterate through dependencies */
        /* Note: curium_map iteration would need to be implemented */
    }
    
    CuriumJsonNode_delete(root);
    return manifest;
}

int curium_packages_save_manifest(const char* path, curium_package_manifest_t* manifest) {
    if (!path || !manifest) return -1;
    
    curium_string_t* json = curium_string_new("{\n");
    
    /* Build JSON manually since we don't have curium_string_append_format */
    curium_string_append(json, "  \"name\": \"");
    curium_string_append(json, manifest->name ? manifest->name->data : "unknown");
    curium_string_append(json, "\",\n");
    
    curium_string_append(json, "  \"version\": \"");
    curium_string_append(json, manifest->version ? manifest->version->data : "1.0.0");
    curium_string_append(json, "\",\n");
    
    curium_string_append(json, "  \"description\": \"");
    curium_string_append(json, manifest->description ? manifest->description->data : "");
    curium_string_append(json, "\",\n");
    
    curium_string_append(json, "  \"main\": \"");
    curium_string_append(json, manifest->main ? manifest->main->data : "src/main.cm");
    curium_string_append(json, "\",\n");
    
    curium_string_append(json, "  \"registry\": \"");
    curium_string_append(json, manifest->registry ? manifest->registry->data : CURIUM_REGISTRY_URL);
    curium_string_append(json, "\",\n");
    
    /* Dependencies */
    curium_string_append(json, "  \"dependencies\": {\n");
    curium_string_append(json, "  },\n");
    
    /* Dev dependencies */
    curium_string_append(json, "  \"devDependencies\": {\n");
    curium_string_append(json, "  }\n");
    
    curium_string_append(json, "}\n");
    
    int result = curium_file_write(path, json->data);
    curium_string_free(json);
    
    return result;
}

/* ============================================================================
 * Package Manager Context
 * ========================================================================== */

curium_package_manager_t* curium_packages_init(const char* project_root) {
    if (!project_root) return NULL;
    
    curium_package_manager_t* pm = (curium_package_manager_t*)curium_alloc(sizeof(curium_package_manager_t), "curium_package_manager");
    if (!pm) return NULL;
    
    memset(pm, 0, sizeof(*pm));
    pm->project_root = curium_string_new(project_root);
    pm->packages_dir = curium_packages_get_cache_dir();
    pm->registry_url = curium_string_new(CURIUM_REGISTRY_URL);
    
    /* Ensure packages directory exists */
    curium_packages_ensure_dir(pm->packages_dir->data);
    
    /* Load manifest if exists */
    char manifest_path[1024];
    snprintf(manifest_path, sizeof(manifest_path), "%s/%s", project_root, CURIUM_PACKAGE_FILE);
    pm->manifest = curium_packages_load_manifest(manifest_path);
    
    return pm;
}

void curium_packages_free(curium_package_manager_t* pm) {
    if (!pm) return;
    
    if (pm->project_root) curium_string_free(pm->project_root);
    if (pm->packages_dir) curium_string_free(pm->packages_dir);
    if (pm->registry_url) curium_string_free(pm->registry_url);
    if (pm->manifest) curium_packages_manifest_free(pm->manifest);
    
    curium_free(pm);
}

/* ============================================================================
 * CLI Commands
 * ========================================================================== */

int curium_packages_cmd_init(const char* project_name) {
    if (!project_name) {
        fprintf(stderr, "Error: Project name required\n");
        return -1;
    }
    
    /* Create project directory */
    if (curium_packages_ensure_dir(project_name) != 0 && errno != EEXIST) {
        fprintf(stderr, "Error: Failed to create project directory\n");
        return -1;
    }
    
    /* Create subdirectories */
    char path[1024];
    snprintf(path, sizeof(path), "%s/src", project_name);
    curium_packages_ensure_dir(path);
    
    snprintf(path, sizeof(path), "%s/packages", project_name);
    curium_packages_ensure_dir(path);
    
    /* Create default manifest */
    curium_package_manifest_t* manifest = curium_packages_manifest_new();
    if (!manifest) return -1;
    
    manifest->name = curium_string_new(project_name);
    manifest->version = curium_string_new("1.0.0");
    manifest->description = curium_string_format("CM project: %s", project_name);
    manifest->main = curium_string_new("src/main.cm");
    manifest->registry = curium_string_new(CURIUM_REGISTRY_URL);
    
    snprintf(path, sizeof(path), "%s/%s", project_name, CURIUM_PACKAGE_FILE);
    int result = curium_packages_save_manifest(path, manifest);
    
    curium_packages_manifest_free(manifest);
    
    /* Create default main.cm */
    snprintf(path, sizeof(path), "%s/src/main.cm", project_name);
    curium_string_t* main_content = curium_string_new("// Curium Project: ");
    curium_string_append(main_content, project_name);
    curium_string_append(main_content, "\n"
        "\n"
        "fn main() {\n"
        "    print(\"Hello from ");
    curium_string_append(main_content, project_name);
    curium_string_append(main_content, "!\");\n"
        "}\n");
    
    curium_file_write(path, main_content->data);
    curium_string_free(main_content);
    
    if (result == 0) {
        printf("Initialized CM project '%s'\n", project_name);
        printf("  - curium.json created\n");
        printf("  - src/main.cm created\n");
        printf("  - packages/ directory ready\n");
    }
    
    return result;
}

int curium_packages_cmd_install(const char* package_name, const char* version) {
    if (!package_name) {
        /* Install all dependencies from manifest */
        curium_package_manager_t* pm = curium_packages_init(".");
        if (!pm) return -1;
        
        int result = curium_packages_install_all(pm);
        curium_packages_free(pm);
        return result;
    }
    
    /* Install specific package */
    curium_package_manager_t* pm = curium_packages_init(".");
    if (!pm) return -1;
    
    int result = curium_packages_install(pm, package_name, version);
    curium_packages_free(pm);
    return result;
}

/* ============================================================================
 * Package Installation
 * ========================================================================== */

int curium_packages_install(curium_package_manager_t* pm, const char* package_name, const char* version) {
    if (!pm || !package_name) return -1;
    
    printf("Installing %s", package_name);
    if (version) printf("@%s", version);
    printf("...\n");
    
    char url[512];
    snprintf(url, sizeof(url), "https://registry.curium-lang.org/api/pkg/%s", package_name);
    
    CHttpResponse* res = curium_http_get(url);
    if (!res || res->status_code != 200) {
        printf("Failed to locate package API for: %s\n", package_name);
        if (res) CHttpResponse_delete(res);
        return -1;
    }
    
    printf("Fetched package info successfully, unpacking not fully implemented.\n");
    
    CHttpResponse_delete(res);
    return 1;
}

int curium_packages_install_all(curium_package_manager_t* pm) {
    if (!pm || !pm->manifest) {
        fprintf(stderr, "No manifest found. Run 'cm packages init' first.\n");
        return -1;
    }
    
    printf("Installing dependencies for %s...\n", 
           pm->manifest->name ? pm->manifest->name->data : "project");
    
    /* TODO: Iterate through manifest dependencies and install each */
    
    return 0;
}

int curium_packages_cmd_remove(const char* package_name) {
    if (!package_name) {
        fprintf(stderr, "Error: Package name required\n");
        return -1;
    }
    
    printf("Removing %s...\n", package_name);
    
    /* TODO: Implement package removal */
    /* 1. Remove from manifest
     * 2. Remove from packages directory
     * 3. Check for orphaned dependencies
     */
    
    return 0;
}

int curium_packages_cmd_update(const char* package_name) {
    if (!package_name) {
        /* Update all packages */
        printf("Updating all packages...\n");
    } else {
        printf("Updating %s...\n", package_name);
    }
    
    /* TODO: Implement package update */
    /* 1. Check for updates from registry
     * 2. Update manifest versions
     * 3. Reinstall packages
     */
    
    return 0;
}

int curium_packages_cmd_list(void) {
    curium_package_manager_t* pm = curium_packages_init(".");
    if (!pm || !pm->manifest) {
        fprintf(stderr, "No manifest found.\n");
        return -1;
    }
    
    printf("Project: %s@%s\n",
           pm->manifest->name ? pm->manifest->name->data : "unknown",
           pm->manifest->version ? pm->manifest->version->data : "1.0.0");
    
    printf("\nDependencies:\n");
    /* TODO: List installed dependencies */
    
    curium_packages_free(pm);
    return 0;
}

int curium_packages_cmd_search(const char* query) {
    if (!query) {
        fprintf(stderr, "Error: Search query required\n");
        return -1;
    }
    
    printf("Searching for '%s'...\n", query);
    
    /* TODO: Implement package search via registry API */
    
    return 0;
}
