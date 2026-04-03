#include "curium/json.h"
#include "curium/memory.h"
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static struct CuriumJsonNode* parse_json_value(const char** ptr);

static void skip_whitespace(const char** ptr) {
    while (isspace(**ptr)) {
        (*ptr)++;
    }
}

static struct CuriumJsonNode* create_json_node(CMJsonType type) {
    struct CuriumJsonNode* node = (struct CuriumJsonNode*)curium_alloc(sizeof(struct CuriumJsonNode), "CuriumJsonNode");
    if (node) node->type = type;
    return node;
}

static struct CuriumJsonNode* parse_json_string(const char** ptr) {
    (*ptr)++;
    const char* start = *ptr;
    while (**ptr && **ptr != '"') {
        if (**ptr == '\\') (*ptr)++;
        (*ptr)++;
    }
    
    size_t len = (size_t)(*ptr - start);
    char* buf = (char*)malloc(len + 1);
    if (!buf) return NULL;
    memcpy(buf, start, len);
    buf[len] = '\0';
    
    (*ptr)++;
    
    struct CuriumJsonNode* node = create_json_node(CURIUM_JSON_STRING);
    node->value.string_val = curium_string_new(buf);
    free(buf);
    return node;
}

static struct CuriumJsonNode* parse_json_number(const char** ptr) {
    char* end;
    double val = strtod(*ptr, &end);
    *ptr = end;
    struct CuriumJsonNode* node = create_json_node(CURIUM_JSON_NUMBER);
    node->value.number_val = val;
    return node;
}

static struct CuriumJsonNode* parse_json_boolean(const char** ptr) {
    struct CuriumJsonNode* node = create_json_node(CURIUM_JSON_BOOLEAN);
    if (strncmp(*ptr, "true", 4) == 0) {
        node->value.boolean_val = 1;
        *ptr += 4;
    } else {
        node->value.boolean_val = 0;
        *ptr += 5;
    }
    return node;
}

static struct CuriumJsonNode* parse_json_null(const char** ptr) {
    struct CuriumJsonNode* node = create_json_node(CURIUM_JSON_NULL);
    *ptr += 4;
    return node;
}

static struct CuriumJsonNode* parse_json_array(const char** ptr) {
    struct CuriumJsonNode* node = create_json_node(CURIUM_JSON_ARRAY);
    node->value.array_val = curium_array_new(sizeof(struct CuriumJsonNode*), 4);
    
    (*ptr)++;
    skip_whitespace(ptr);
    
    while (**ptr && **ptr != ']') {
        struct CuriumJsonNode* elem = parse_json_value(ptr);
        if (elem) {
            curium_array_push(node->value.array_val, &elem);
        }
        skip_whitespace(ptr);
        if (**ptr == ',') {
            (*ptr)++;
            skip_whitespace(ptr);
        }
    }
    if (**ptr == ']') (*ptr)++;
    return node;
}

static struct CuriumJsonNode* parse_json_object(const char** ptr) {
    struct CuriumJsonNode* node = create_json_node(CURIUM_JSON_OBJECT);
    node->value.object_val = curium_map_new();
    
    (*ptr)++;
    skip_whitespace(ptr);
    
    while (**ptr && **ptr != '}') {
        struct CuriumJsonNode* key_node = parse_json_string(ptr);
        skip_whitespace(ptr);
        if (**ptr == ':') {
            (*ptr)++;
            skip_whitespace(ptr);
            struct CuriumJsonNode* val_node = parse_json_value(ptr);
            if (val_node) {
                curium_map_set(node->value.object_val, key_node->value.string_val->data, &val_node, sizeof(struct CuriumJsonNode*));
            }
        }
        CuriumJsonNode_delete(key_node);
        
        skip_whitespace(ptr);
        if (**ptr == ',') {
            (*ptr)++;
            skip_whitespace(ptr);
        }
    }
    if (**ptr == '}') (*ptr)++;
    return node;
}

static struct CuriumJsonNode* parse_json_value(const char** ptr) {
    skip_whitespace(ptr);
    char c = **ptr;
    if (c == '{') return parse_json_object(ptr);
    if (c == '[') return parse_json_array(ptr);
    if (c == '"') return parse_json_string(ptr);
    if (isdigit((unsigned char)c) || c == '-') return parse_json_number(ptr);
    if (strncmp(*ptr, "true", 4) == 0 || strncmp(*ptr, "false", 5) == 0) return parse_json_boolean(ptr);
    if (strncmp(*ptr, "null", 4) == 0) return parse_json_null(ptr);
    return NULL;
}

struct CuriumJsonNode* curium_json_parse(const char* json_str) {
    if (!json_str) return NULL;
    const char* ptr = json_str;
    return parse_json_value(&ptr);
}

void CuriumJsonNode_delete(struct CuriumJsonNode* node) {
    if (!node) return;
    switch (node->type) {
        case CURIUM_JSON_STRING:
            if (node->value.string_val) curium_string_free(node->value.string_val);
            break;
        case CURIUM_JSON_ARRAY:
            if (node->value.array_val) {
                for (size_t i = 0; i < curium_array_length(node->value.array_val); i++) {
                    struct CuriumJsonNode* elem = *(struct CuriumJsonNode**)curium_array_get(node->value.array_val, i);
                    CuriumJsonNode_delete(elem);
                }
                curium_array_free(node->value.array_val);
            }
            break;
        case CURIUM_JSON_OBJECT:
            if (node->value.object_val) {
                curium_map_t* internal_map = node->value.object_val;
                for (int i = 0; i < internal_map->bucket_count; i++) {
                    curium_map_entry_t* entry = internal_map->buckets[i];
                    while (entry) {
                        struct CuriumJsonNode* val = *(struct CuriumJsonNode**)entry->value;
                        CuriumJsonNode_delete(val);
                        entry = entry->next;
                    }
                }
                curium_map_free(node->value.object_val);
            }
            break;
        default: break;
    }
    curium_free(node);
}

static void stringify_json_node(struct CuriumJsonNode* node, curium_string_t* out) {
    if (!node) {
        curium_string_append(out, "null");
        return;
    }
    switch (node->type) {
        case CURIUM_JSON_NULL: 
            curium_string_append(out, "null"); 
            break;
        case CURIUM_JSON_BOOLEAN: 
            curium_string_append(out, node->value.boolean_val ? "true" : "false"); 
            break;
        case CURIUM_JSON_NUMBER: {
            char buf[64];
            snprintf(buf, sizeof(buf), "%g", node->value.number_val);
            curium_string_append(out, buf);
            break;
        }
        case CURIUM_JSON_STRING:
            curium_string_append(out, "\"");
            curium_string_append(out, node->value.string_val->data);
            curium_string_append(out, "\"");
            break;
        case CURIUM_JSON_ARRAY: {
            curium_string_append(out, "[");
            for (size_t i = 0; i < curium_array_length(node->value.array_val); i++) {
                struct CuriumJsonNode* elem = *(struct CuriumJsonNode**)curium_array_get(node->value.array_val, i);
                stringify_json_node(elem, out);
                if (i < curium_array_length(node->value.array_val) - 1) {
                    curium_string_append(out, ",");
                }
            }
            curium_string_append(out, "]");
            break;
        }
        case CURIUM_JSON_OBJECT: {
            curium_string_append(out, "{");
            curium_map_t* internal_map = node->value.object_val;
            int first = 1;
            for (int i = 0; i < internal_map->bucket_count; i++) {
                curium_map_entry_t* entry = internal_map->buckets[i];
                while (entry) {
                    if (!first) curium_string_append(out, ",");
                    curium_string_append(out, "\"");
                    curium_string_append(out, entry->key);
                    curium_string_append(out, "\":");
                    stringify_json_node(*(struct CuriumJsonNode**)entry->value, out);
                    first = 0;
                    entry = entry->next;
                }
            }
            curium_string_append(out, "}");
            break;
        }
    }
}

curium_string_t* curium_json_stringify(struct CuriumJsonNode* node) {
    curium_string_t* out = curium_string_new("");
    stringify_json_node(node, out);
    return out;
}
