#include "curium/debug.h"
#include "curium/error_detail.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static curium_var_track_t* var_list = NULL;
static int debug_initialized = 0;

void curium_debug_init(void) {
    if (debug_initialized) return;
    var_list = NULL;
    debug_initialized = 1;
}

void curium_debug_shutdown(void) {
    if (!debug_initialized) return;
    
    curium_var_track_t* current = var_list;
    while (current) {
        curium_var_track_t* next = current->next;
        free(current);
        current = next;
    }
    var_list = NULL;
    debug_initialized = 0;
}

void curium_debug_var_declare(const char* name, const char* type, void* addr,
                          const char* file, int line) {
    if (!debug_initialized || !addr) return;
    
    curium_var_track_t* track = (curium_var_track_t*)malloc(sizeof(curium_var_track_t));
    if (!track) return;
    
    memset(track, 0, sizeof(*track));
    if (name) strncpy(track->name, name, sizeof(track->name) - 1);
    if (type) strncpy(track->type, type, sizeof(track->type) - 1);
    track->address = addr;
    track->state = CURIUM_VAR_UNINITIALIZED;
    track->line_declared = line;
    if (file) strncpy(track->file_declared, file, sizeof(track->file_declared) - 1);
    
    track->next = var_list;
    var_list = track;
}

void curium_debug_var_init(void* addr) {
    if (!debug_initialized || !addr) return;
    
    curium_var_track_t* current = var_list;
    while (current) {
        if (current->address == addr) {
            current->state = CURIUM_VAR_INITIALIZED;
            return;
        }
        current = current->next;
    }
}

void curium_debug_var_free(void* addr) {
    if (!debug_initialized || !addr) return;
    
    curium_var_track_t* current = var_list;
    while (current) {
        if (current->address == addr) {
            current->state = CURIUM_VAR_FREED;
            return;
        }
        current = current->next;
    }
}

int curium_debug_var_is_initialized(void* addr) {
    if (!debug_initialized || !addr) return 0;
    
    curium_var_track_t* current = var_list;
    while (current) {
        if (current->address == addr) {
            return current->state == CURIUM_VAR_INITIALIZED;
        }
        current = current->next;
    }
    return 0;
}

const char* curium_debug_var_get_name(void* addr) {
    if (!debug_initialized || !addr) return "unknown";
    
    curium_var_track_t* current = var_list;
    while (current) {
        if (current->address == addr) {
            return current->name[0] ? current->name : "unnamed";
        }
        current = current->next;
    }
    return "unknown";
}

const char* curium_debug_var_get_type(void* addr) {
    if (!debug_initialized || !addr) return "unknown";
    
    curium_var_track_t* current = var_list;
    while (current) {
        if (current->address == addr) {
            return current->type[0] ? current->type : "unknown";
        }
        current = current->next;
    }
    return "unknown";
}

void curium_debug_check_null(const void* ptr, const char* name,
                         const char* file, int line) {
    if (ptr != NULL) return;
    
    curium_error_detail_t detail;
    curium_error_detail_init(&detail);
    detail.code = CURIUM_ERROR_NULL_POINTER;
    detail.severity = CURIUM_SEVERITY_FATAL;
    curium_error_detail_set_location(&detail, file, line, 0);
    curium_error_detail_set_object(&detail, "pointer", name ? name : "unknown");
    curium_error_detail_set_message(&detail, "Accessed while NULL");
    curium_error_detail_set_suggestion(&detail, "Ensure variable is initialized before use");
    
    curium_error_detail_print_runtime(&detail);
    
    /* In debug mode, also print stack trace */
    curium_debug_print_stack_trace();
    
    exit(1);
}

void curium_debug_check_initialized(void* addr, const char* name,
                                const char* file, int line) {
    (void)name; /* May be used in future for better error messages */
    if (!debug_initialized) return;
    
    curium_var_track_t* current = var_list;
    while (current) {
        if (current->address == addr) {
            if (current->state == CURIUM_VAR_INITIALIZED) return;
            
            curium_error_detail_t detail;
            curium_error_detail_init(&detail);
            detail.code = CURIUM_ERROR_UNINITIALIZED;
            detail.severity = CURIUM_SEVERITY_ERROR;
            curium_error_detail_set_location(&detail, file, line, 0);
            curium_error_detail_set_object(&detail, current->type, current->name);
            curium_error_detail_set_message(&detail, "Variable used before initialization");
            curium_error_detail_set_suggestion(&detail, "Initialize variable before using");
            
            curium_error_detail_print_runtime(&detail);
            return;
        }
        current = current->next;
    }
}

void curium_debug_check_bounds(size_t index, size_t size, const char* name,
                           const char* file, int line) {
    if (index < size) return;
    
    curium_error_detail_t detail;
    curium_error_detail_init(&detail);
    detail.code = CURIUM_ERROR_OUT_OF_BOUNDS;
    detail.severity = CURIUM_SEVERITY_FATAL;
    curium_error_detail_set_location(&detail, file, line, 0);
    curium_error_detail_set_object(&detail, "array", name ? name : "unknown");
    curium_error_detail_set_message(&detail, "Index %zu out of bounds (size: %zu)", index, size);
    curium_error_detail_set_suggestion(&detail, "Check array bounds before accessing");
    
    curium_error_detail_print_runtime(&detail);
    curium_debug_print_stack_trace();
    exit(1);
}

void curium_debug_track_alloc(void* ptr, size_t size, const char* file, int line) {
    (void)size;
    if (!debug_initialized || !ptr) return;
    
    curium_var_track_t* track = (curium_var_track_t*)malloc(sizeof(curium_var_track_t));
    if (!track) return;
    
    memset(track, 0, sizeof(*track));
    strncpy(track->name, "allocated", sizeof(track->name) - 1);
    strncpy(track->type, "memory", sizeof(track->type) - 1);
    track->address = ptr;
    track->state = CURIUM_VAR_INITIALIZED;
    track->line_declared = line;
    if (file) strncpy(track->file_declared, file, sizeof(track->file_declared) - 1);
    
    track->next = var_list;
    var_list = track;
}

void curium_debug_track_free(void* ptr) {
    if (!debug_initialized || !ptr) return;
    
    curium_var_track_t* current = var_list;
    while (current) {
        if (current->address == ptr) {
            current->state = CURIUM_VAR_FREED;
            return;
        }
        current = current->next;
    }
}

int curium_debug_is_valid_ptr(void* ptr) {
    if (!debug_initialized || !ptr) return 0;
    
    curium_var_track_t* current = var_list;
    while (current) {
        if (current->address == ptr) {
            return current->state != CURIUM_VAR_FREED;
        }
        current = current->next;
    }
    return 1; /* Assume valid if not tracked */
}

void curium_debug_print_stack_trace(void) {
    fprintf(stderr, "Stack trace:\n");
    fprintf(stderr, "  (stack trace requires debug symbols)\n");
}

void curium_debug_capture_stack_trace(char** buffer, int max_depth) {
    (void)buffer;
    (void)max_depth;
    /* Stack trace capture would require platform-specific code */
}
