#include "curium/oop.h"
#include "curium/string.h"
#include "curium/memory.h"
#include <string.h>
#include <stdlib.h>

/* Helper for curium_strdup if not in string.h */
static char* curium_strdup_local(const char* s) {
    if (!s) return NULL;
    size_t len = strlen(s) + 1;
    char* d = (char*)curium_alloc(len, "string");
    if (d) memcpy(d, s, len);
    return d;
}

curium_class_t* curium_class_new(const char* name, curium_class_t* parent) {
    if (!name) return NULL;
    
    curium_class_t* class = (curium_class_t*)curium_alloc(sizeof(curium_class_t), "class");
    if (!class) return NULL;
    
    class->name = curium_strdup_local(name);
    class->parent = parent;
    class->methods = curium_map_new();
    class->fields = curium_map_new();
    class->gc_handle.index = 0;
    class->gc_handle.generation = 0;
    
    if (!class->name || !class->methods || !class->fields) {
        curium_class_free(class);
        return NULL;
    }
    
    /* Inherit parent's fields and methods */
    if (parent) {
        /* Copy parent fields */
        curium_map_foreach(parent->fields, key, value) {
            curium_map_set(class->fields, key, value, 0);
        }
        /* Copy parent methods */
        curium_map_foreach(parent->methods, key, value) {
            curium_map_set(class->methods, key, value, 0);
        }
    }
    
    return class;
}

void curium_class_free(curium_class_t* class) {
    if (!class) return;
    if (class->name) curium_free(class->name);
    if (class->methods) curium_map_free(class->methods);
    if (class->fields) curium_map_free(class->fields);
    curium_free(class);
}

int curium_class_add_method(curium_class_t* class, const char* name, curium_method_fn fn) {
    if (!class || !name || !fn) return -1;
    
    /* Safely convert function pointer to void* using a union to satisfy -Wpedantic */
    union {
        curium_method_fn fn;
        const void* ptr;
    } cast;
    cast.fn = fn;
    
    curium_map_set(class->methods, name, cast.ptr, 0);
    return 0;
}

int curium_class_add_field(curium_class_t* class, const char* name, void* default_value) {
    if (!class || !name) return -1;
    curium_map_set(class->fields, name, default_value, 0);
    return 0;
}

int curium_class_has_method(curium_class_t* class, const char* name) {
    if (!class || !name) return 0;
    return curium_map_get(class->methods, name) != NULL;
}

int curium_class_has_field(curium_class_t* class, const char* name) {
    if (!class || !name) return 0;
    return curium_map_get(class->fields, name) != NULL;
}

curium_object_t* curium_object_new(curium_class_t* class) {
    if (!class) return NULL;
    
    curium_object_t* obj = (curium_object_t*)curium_alloc(sizeof(struct curium_object), "object");
    if (!obj) return NULL;
    
    obj->class = class;
    obj->fields = curium_map_new();
    obj->gc_handle.index = 0;
    obj->gc_handle.generation = 0;
    
    if (!obj->fields) {
        curium_object_free(obj);
        return NULL;
    }
    
    /* Initialize fields with defaults from class */
    curium_map_foreach(class->fields, key, value) {
        curium_map_set(obj->fields, key, value, 0);
    }
    
    return obj;
}

void curium_object_free(curium_object_t* obj) {
    if (!obj) return;
    if (obj->fields) curium_map_free(obj->fields);
    curium_free(obj);
}

void* curium_object_call(curium_object_t* obj, const char* name, void** args, size_t arg_count) {
    if (!obj || !name) return NULL;
    
    void* ptr = curium_map_get(obj->class->methods, name);
    if (!ptr) return NULL;
    
    /* Safely convert void* back to function pointer */
    union {
        void* ptr;
        curium_method_fn fn;
    } cast;
    cast.ptr = ptr;
    
    return cast.fn(obj, args, arg_count);
}

void* curium_object_get_field(curium_object_t* obj, const char* name) {
    if (!obj || !name) return NULL;
    return curium_map_get(obj->fields, name);
}

int curium_object_set_field(curium_object_t* obj, const char* name, void* value) {
    if (!obj || !name) return -1;
    /* Check if field exists in class */
    if (!curium_class_has_field(obj->class, name)) return -1;
    curium_map_set(obj->fields, name, value, 0);
    return 0;
}

int curium_object_is_instance(curium_object_t* obj, curium_class_t* class) {
    if (!obj || !class) return 0;
    
    curium_class_t* current = obj->class;
    while (current) {
        if (current == class) return 1;
        current = current->parent;
    }
    return 0;
}
