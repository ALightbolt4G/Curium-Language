/**
 * @file json.h
 * @brief JSON parser mapping recursive node sequences.
 */
#ifndef CURIUM_JSON_H
#define CURIUM_JSON_H

#include "core.h"
#include "string.h"
#include "array.h"
#include "map.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CURIUM_JSON_NULL,
    CURIUM_JSON_BOOLEAN,
    CURIUM_JSON_NUMBER,
    CURIUM_JSON_STRING,
    CURIUM_JSON_ARRAY,
    CURIUM_JSON_OBJECT
} CMJsonType;

struct CuriumJsonNode {
    CMJsonType type;
    union {
        curium_string_t* string_val;
        double number_val;
        int boolean_val;
        curium_array_t* array_val;
        curium_map_t* object_val;
    } value;
};

/**
 * @brief iterates recursively fetching nodes processing natively intuitively.
 */
struct CuriumJsonNode* curium_json_parse(const char* json_str);

/**
 * @brief collapses object metrics mapping cleanly inherently securely.
 */
curium_string_t* curium_json_stringify(struct CuriumJsonNode* node);

/**
 * @brief explicitly traverses destructing dynamic endpoints systematically globally.
 */
void CuriumJsonNode_delete(struct CuriumJsonNode* node);

#ifdef __cplusplus
}
#endif

#endif /* CURIUM_JSON_H */
