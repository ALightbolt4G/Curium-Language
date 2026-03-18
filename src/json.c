#include "cm/json.h"
#include "cm/memory.h"
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static struct CMJsonNode* parse_json_value(const char** ptr);

static void skip_whitespace(const char** ptr) {
    while (isspace(**ptr)) {
        (*ptr)++;
    }
}

static struct CMJsonNode* create_json_node(CMJsonType type) {
    struct CMJsonNode* node = (struct CMJsonNode*)cm_alloc(sizeof(struct CMJsonNode), "CMJsonNode");
    if (node) node->type = type;
    return node;
}

static struct CMJsonNode* parse_json_string(const char** ptr) {
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
    
    struct CMJsonNode* node = create_json_node(CM_JSON_STRING);
    node->value.string_val = cm_string_new(buf);
    free(buf);
    return node;
}

static struct CMJsonNode* parse_json_number(const char** ptr) {
    char* end;
    double val = strtod(*ptr, &end);
    *ptr = end;
    struct CMJsonNode* node = create_json_node(CM_JSON_NUMBER);
    node->value.number_val = val;
    return node;
}

static struct CMJsonNode* parse_json_boolean(const char** ptr) {
    struct CMJsonNode* node = create_json_node(CM_JSON_BOOLEAN);
    if (strncmp(*ptr, "true", 4) == 0) {
        node->value.boolean_val = 1;
        *ptr += 4;
    } else {
        node->value.boolean_val = 0;
        *ptr += 5;
    }
    return node;
}

static struct CMJsonNode* parse_json_null(const char** ptr) {
    struct CMJsonNode* node = create_json_node(CM_JSON_NULL);
    *ptr += 4;
    return node;
}

static struct CMJsonNode* parse_json_array(const char** ptr) {
    struct CMJsonNode* node = create_json_node(CM_JSON_ARRAY);
    node->value.array_val = cm_array_new(sizeof(struct CMJsonNode*), 4);
    
    (*ptr)++;
    skip_whitespace(ptr);
    
    while (**ptr && **ptr != ']') {
        struct CMJsonNode* elem = parse_json_value(ptr);
        if (elem) {
            cm_array_push(node->value.array_val, &elem);
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

static struct CMJsonNode* parse_json_object(const char** ptr) {
    struct CMJsonNode* node = create_json_node(CM_JSON_OBJECT);
    node->value.object_val = cm_map_new();
    
    (*ptr)++;
    skip_whitespace(ptr);
    
    while (**ptr && **ptr != '}') {
        struct CMJsonNode* key_node = parse_json_string(ptr);
        skip_whitespace(ptr);
        if (**ptr == ':') {
            (*ptr)++;
            skip_whitespace(ptr);
            struct CMJsonNode* val_node = parse_json_value(ptr);
            if (val_node) {
                cm_map_set(node->value.object_val, key_node->value.string_val->data, &val_node, sizeof(struct CMJsonNode*));
            }
        }
        CMJsonNode_delete(key_node);
        
        skip_whitespace(ptr);
        if (**ptr == ',') {
            (*ptr)++;
            skip_whitespace(ptr);
        }
    }
    if (**ptr == '}') (*ptr)++;
    return node;
}

static struct CMJsonNode* parse_json_value(const char** ptr) {
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

struct CMJsonNode* cm_json_parse(const char* json_str) {
    if (!json_str) return NULL;
    const char* ptr = json_str;
    return parse_json_value(&ptr);
}

void CMJsonNode_delete(struct CMJsonNode* node) {
    if (!node) return;
    switch (node->type) {
        case CM_JSON_STRING:
            if (node->value.string_val) cm_string_free(node->value.string_val);
            break;
        case CM_JSON_ARRAY:
            if (node->value.array_val) {
                for (size_t i = 0; i < cm_array_length(node->value.array_val); i++) {
                    struct CMJsonNode* elem = *(struct CMJsonNode**)cm_array_get(node->value.array_val, i);
                    CMJsonNode_delete(elem);
                }
                cm_array_free(node->value.array_val);
            }
            break;
        case CM_JSON_OBJECT:
            if (node->value.object_val) {
                cm_map_t* internal_map = node->value.object_val;
                for (int i = 0; i < internal_map->bucket_count; i++) {
                    cm_map_entry_t* entry = internal_map->buckets[i];
                    while (entry) {
                        struct CMJsonNode* val = *(struct CMJsonNode**)entry->value;
                        CMJsonNode_delete(val);
                        entry = entry->next;
                    }
                }
                cm_map_free(node->value.object_val);
            }
            break;
        default: break;
    }
    cm_free(node);
}

static void stringify_json_node(struct CMJsonNode* node, cm_string_t* out) {
    if (!node) {
        cm_string_append(out, "null");
        return;
    }
    switch (node->type) {
        case CM_JSON_NULL: 
            cm_string_append(out, "null"); 
            break;
        case CM_JSON_BOOLEAN: 
            cm_string_append(out, node->value.boolean_val ? "true" : "false"); 
            break;
        case CM_JSON_NUMBER: {
            char buf[64];
            snprintf(buf, sizeof(buf), "%g", node->value.number_val);
            cm_string_append(out, buf);
            break;
        }
        case CM_JSON_STRING:
            cm_string_append(out, "\"");
            cm_string_append(out, node->value.string_val->data);
            cm_string_append(out, "\"");
            break;
        case CM_JSON_ARRAY: {
            cm_string_append(out, "[");
            for (size_t i = 0; i < cm_array_length(node->value.array_val); i++) {
                struct CMJsonNode* elem = *(struct CMJsonNode**)cm_array_get(node->value.array_val, i);
                stringify_json_node(elem, out);
                if (i < cm_array_length(node->value.array_val) - 1) {
                    cm_string_append(out, ",");
                }
            }
            cm_string_append(out, "]");
            break;
        }
        case CM_JSON_OBJECT: {
            cm_string_append(out, "{");
            cm_map_t* internal_map = node->value.object_val;
            int first = 1;
            for (int i = 0; i < internal_map->bucket_count; i++) {
                cm_map_entry_t* entry = internal_map->buckets[i];
                while (entry) {
                    if (!first) cm_string_append(out, ",");
                    cm_string_append(out, "\"");
                    cm_string_append(out, entry->key);
                    cm_string_append(out, "\":");
                    stringify_json_node(*(struct CMJsonNode**)entry->value, out);
                    first = 0;
                    entry = entry->next;
                }
            }
            cm_string_append(out, "}");
            break;
        }
    }
}

cm_string_t* cm_json_stringify(struct CMJsonNode* node) {
    cm_string_t* out = cm_string_new("");
    stringify_json_node(node, out);
    return out;
}
