/**
 * @file oop.h
 * @brief Object-oriented programming support for CM.
 *
 * Simple OOP system with classes, objects, methods, and inheritance.
 * All memory is managed by the CM garbage collector.
 */
#ifndef CURIUM_OOP_H
#define CURIUM_OOP_H

#include "core.h"
#include "memory.h"
#include "map.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Method function type.
 */
typedef void* (*curium_method_fn)(void* self, void** args, size_t arg_count);

/**
 * @brief Class definition.
 */
typedef struct curium_class {
    char* name;
    struct curium_class* parent;
    curium_map_t* methods;      /* name -> curium_method_fn */
    curium_map_t* fields;       /* name -> default value */
    curium_ptr_t gc_handle;
} curium_class_t;

/**
 * @brief Object instance.
 * Mapping to the opaque curium_object_t defined in core.h
 */
struct curium_object {
    curium_class_t* class;
    curium_map_t* fields;       /* instance field values */
    curium_ptr_t gc_handle;
};

/**
 * @brief Create a new class.
 * @param name Class name.
 * @param parent Parent class (NULL for none).
 * @return New class or NULL on error.
 */
curium_class_t* curium_class_new(const char* name, curium_class_t* parent);

/**
 * @brief Free a class.
 * @param class The class to free.
 */
void curium_class_free(curium_class_t* class);

/**
 * @brief Add method to class.
 * @param class The class.
 * @param name Method name.
 * @param fn Method function.
 * @return 0 on success, -1 on error.
 */
int curium_class_add_method(curium_class_t* class, const char* name, curium_method_fn fn);

/**
 * @brief Add field to class.
 * @param class The class.
 * @param name Field name.
 * @param default_value Default value (can be NULL).
 * @return 0 on success, -1 on error.
 */
int curium_class_add_field(curium_class_t* class, const char* name, void* default_value);

/**
 * @brief Check if class has method.
 * @param class The class.
 * @param name Method name.
 * @return 1 if found, 0 otherwise.
 */
int curium_class_has_method(curium_class_t* class, const char* name);

/**
 * @brief Check if class has field.
 * @param class The class.
 * @param name Field name.
 * @return 1 if found, 0 otherwise.
 */
int curium_class_has_field(curium_class_t* class, const char* name);

/**
 * @brief Create new object instance.
 * @param class The class to instantiate.
 * @return New object or NULL on error.
 */
curium_object_t* curium_object_new(curium_class_t* class);

/**
 * @brief Free an object.
 * @param obj The object.
 */
void curium_object_free(curium_object_t* obj);

/**
 * @brief Call method on object.
 * @param obj The object.
 * @param name Method name.
 * @param args Arguments array.
 * @param arg_count Number of arguments.
 * @return Method return value or NULL.
 */
void* curium_object_call(curium_object_t* obj, const char* name, void** args, size_t arg_count);

/**
 * @brief Get field value.
 * @param obj The object.
 * @param name Field name.
 * @return Field value or NULL.
 */
void* curium_object_get_field(curium_object_t* obj, const char* name);

/**
 * @brief Set field value.
 * @param obj The object.
 * @param name Field name.
 * @param value New value.
 * @return 0 on success, -1 on error.
 */
int curium_object_set_field(curium_object_t* obj, const char* name, void* value);

/**
 * @brief Check if object is instance of class.
 * @param obj The object.
 * @param class The class to check.
 * @return 1 if instance, 0 otherwise.
 */
int curium_object_is_instance(curium_object_t* obj, curium_class_t* class);

#ifdef __cplusplus
}
#endif

#endif /* CURIUM_OOP_H */
