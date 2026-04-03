#include "curium/error_obj.h"
#include "curium/string.h"
#include "curium/memory.h"
#include <stdlib.h>
#include <string.h>

struct curium_error_obj {
    curium_error_detail_t detail;
    int refcount;
};

struct curium_error_chain {
    curium_error_obj_t** errors;
    int count;
    int capacity;
};

curium_error_obj_t* curium_error_obj_create(const curium_error_detail_t* detail) {
    if (!detail) return NULL;
    
    curium_error_obj_t* err = (curium_error_obj_t*)malloc(sizeof(curium_error_obj_t));
    if (!err) return NULL;
    
    memcpy(&err->detail, detail, sizeof(curium_error_detail_t));
    err->refcount = 1;
    return err;
}

curium_error_obj_t* curium_error_obj_create_simple(curium_error_code_t code, const char* message) {
    curium_error_detail_t detail;
    curium_error_detail_init(&detail);
    detail.code = code;
    curium_error_detail_set_message(&detail, "%s", message ? message : "Unknown error");
    return curium_error_obj_create(&detail);
}

curium_error_code_t curium_error_obj_get_code(const curium_error_obj_t* err) {
    return err ? err->detail.code : CURIUM_ERROR_UNKNOWN;
}

const char* curium_error_obj_get_message(const curium_error_obj_t* err) {
    return err ? err->detail.message : "";
}

const char* curium_error_obj_get_file(const curium_error_obj_t* err) {
    return err ? err->detail.file : "";
}

int curium_error_obj_get_line(const curium_error_obj_t* err) {
    return err ? err->detail.line : -1;
}

int curium_error_obj_get_column(const curium_error_obj_t* err) {
    return err ? err->detail.column : -1;
}

const char* curium_error_obj_get_object_name(const curium_error_obj_t* err) {
    return err ? err->detail.object_name : "";
}

const char* curium_error_obj_get_object_type(const curium_error_obj_t* err) {
    return err ? err->detail.object_type : "";
}

const char* curium_error_obj_get_suggestion(const curium_error_obj_t* err) {
    return err ? err->detail.suggestion : "";
}

curium_error_severity_t curium_error_obj_get_severity(const curium_error_obj_t* err) {
    return err ? err->detail.severity : CURIUM_SEVERITY_ERROR;
}

int curium_error_obj_is(const curium_error_obj_t* err, curium_error_code_t code) {
    return err && err->detail.code == code;
}

void curium_error_obj_print(const curium_error_obj_t* err) {
    if (!err) return;
    curium_error_detail_print(&err->detail);
}

curium_string_t* curium_error_obj_to_json(const curium_error_obj_t* err) {
    if (!err) return curium_string_new("null");
    return curium_error_detail_to_json(&err->detail);
}

void curium_error_obj_free(curium_error_obj_t* err) {
    if (!err) return;
    err->refcount--;
    if (err->refcount <= 0) {
        free(err);
    }
}

curium_error_chain_t* curium_error_chain_create(void) {
    curium_error_chain_t* chain = (curium_error_chain_t*)malloc(sizeof(curium_error_chain_t));
    if (!chain) return NULL;
    
    chain->capacity = 8;
    chain->count = 0;
    chain->errors = (curium_error_obj_t**)malloc(sizeof(curium_error_obj_t*) * chain->capacity);
    
    if (!chain->errors) {
        free(chain);
        return NULL;
    }
    
    return chain;
}

void curium_error_chain_add(curium_error_chain_t* chain, curium_error_obj_t* err) {
    if (!chain || !err) return;
    
    if (chain->count >= chain->capacity) {
        chain->capacity *= 2;
        curium_error_obj_t** new_errors = (curium_error_obj_t**)realloc(chain->errors, 
                                                                  sizeof(curium_error_obj_t*) * chain->capacity);
        if (!new_errors) return;
        chain->errors = new_errors;
    }
    
    chain->errors[chain->count++] = err;
}

int curium_error_chain_count(const curium_error_chain_t* chain) {
    return chain ? chain->count : 0;
}

curium_error_obj_t* curium_error_chain_get(const curium_error_chain_t* chain, int index) {
    if (!chain || index < 0 || index >= chain->count) return NULL;
    return chain->errors[index];
}

void curium_error_chain_print_all(const curium_error_chain_t* chain) {
    if (!chain) return;
    
    for (int i = 0; i < chain->count; i++) {
        fprintf(stderr, "\n[%d/%d] ", i + 1, chain->count);
        curium_error_obj_print(chain->errors[i]);
    }
}

void curium_error_chain_free(curium_error_chain_t* chain) {
    if (!chain) return;
    
    for (int i = 0; i < chain->count; i++) {
        curium_error_obj_free(chain->errors[i]);
    }
    
    free(chain->errors);
    free(chain);
}
