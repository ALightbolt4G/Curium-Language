/**
 * @file json.h
 * @brief JSON parser mapping recursive node sequences.
 */
#ifndef CM_JSON_H
#define CM_JSON_H

#include "core.h"
#include "string.h"
#include "array.h"
#include "map.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CM_JSON_NULL,
    CM_JSON_BOOLEAN,
    CM_JSON_NUMBER,
    CM_JSON_STRING,
    CM_JSON_ARRAY,
    CM_JSON_OBJECT
} CMJsonType;

struct CMJsonNode {
    CMJsonType type;
    union {
        cm_string_t* string_val;
        double number_val;
        int boolean_val;
        cm_array_t* array_val;
        cm_map_t* object_val;
    } value;
};

/**
 * @brief iterates recursively fetching nodes processing natively intuitively.
 */
struct CMJsonNode* cm_json_parse(const char* json_str);

/**
 * @brief collapses object metrics mapping cleanly inherently securely.
 */
cm_string_t* cm_json_stringify(struct CMJsonNode* node);

/**
 * @brief explicitly traverses destructing dynamic endpoints systematically globally.
 */
void CMJsonNode_delete(struct CMJsonNode* node);

#ifdef __cplusplus
}
#endif

#endif /* CM_JSON_H */
