#include "cm/map.h"
#include "cm/memory.h"
#include "cm/error.h"
#include "cm/json.h"
#include "cm/file.h"
#include "cm/string.h"
#include <string.h>

#define CM_MAP_INITIAL_SIZE 16
#define CM_MAP_LOAD_FACTOR 0.75

static uint32_t cm_hash_string(const char* str) {
    uint32_t hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

cm_map_t* cm_map_new(void) {
    cm_map_t* map = (cm_map_t*)cm_alloc(sizeof(cm_map_t), "map");
    if (!map) return NULL;

    map->bucket_count = CM_MAP_INITIAL_SIZE;
    map->size = 0;
    map->load_factor = CM_MAP_LOAD_FACTOR;
    map->growth_factor = 2;
    map->buckets = (cm_map_entry_t**)cm_alloc(sizeof(cm_map_entry_t*) * map->bucket_count, "map_buckets");

    if (!map->buckets) {
        cm_free(map);
        return NULL;
    }
    memset(map->buckets, 0, sizeof(cm_map_entry_t*) * map->bucket_count);
    return map;
}

static void cm_map_resize(cm_map_t* map, size_t new_size) {
    if (!map) return;
    if (new_size == 0 || new_size > SIZE_MAX / sizeof(cm_map_entry_t*)) return;

    cm_map_entry_t** new_buckets = (cm_map_entry_t**)cm_alloc(sizeof(cm_map_entry_t*) * new_size, "map_buckets");
    if (!new_buckets) return;

    memset(new_buckets, 0, sizeof(cm_map_entry_t*) * new_size);

    for (int i = 0; i < map->bucket_count; i++) {
        cm_map_entry_t* entry = map->buckets[i];
        while (entry) {
            cm_map_entry_t* next = entry->next;
            size_t new_index = entry->hash % new_size;
            entry->next = new_buckets[new_index];
            new_buckets[new_index] = entry;
            entry = next;
        }
    }
    cm_free(map->buckets);
    map->buckets = new_buckets;
    map->bucket_count = (int)new_size;
}

void cm_map_set(cm_map_t* map, const char* key, const void* value, size_t value_size) {
    if (!map || !key || !value) return;

    if (map->bucket_count > 0) {
        double needed_capacity = (double)map->bucket_count * (double)map->load_factor;
        if ((double)map->size >= needed_capacity) {
            size_t new_size = (size_t)map->bucket_count * (size_t)map->growth_factor;
            if (new_size > 0 && new_size <= SIZE_MAX / sizeof(cm_map_entry_t*)) {
                cm_map_resize(map, new_size);
            }
        }
    }

    uint32_t hash = cm_hash_string(key);
    int index = hash % map->bucket_count;

    cm_map_entry_t* entry = map->buckets[index];
    while (entry) {
        if (entry->hash == hash && strcmp(entry->key, key) == 0) {
            cm_free(entry->value);
            entry->value = cm_alloc(value_size, "map_value");
            if (!entry->value) {
                cm_error_set(CM_ERROR_MEMORY, "Out of memory in Map Set");
                return;
            }
            memcpy(entry->value, value, value_size);
            entry->value_size = value_size;
            return;
        }
        entry = entry->next;
    }

    entry = (cm_map_entry_t*)cm_alloc(sizeof(cm_map_entry_t), "map_entry");
    if (!entry) { cm_error_set(CM_ERROR_MEMORY, "Map Entry Mem failure"); return; }
    
    size_t key_len = strlen(key) + 1;
    entry->key = (char*)cm_alloc(key_len, "map_key");
    if (!entry->key) { cm_free(entry); return; }
    memcpy(entry->key, key, key_len);

    entry->value = cm_alloc(value_size, "map_value");
    if (!entry->value) { cm_free(entry->key); cm_free(entry); return; }
    memcpy(entry->value, value, value_size);

    entry->value_size = value_size;
    entry->hash = hash;
    entry->next = map->buckets[index];
    map->buckets[index] = entry;
    map->size++;
}

void* cm_map_get(cm_map_t* map, const char* key) {
    if (!map || !key) return NULL;

    uint32_t hash = cm_hash_string(key);
    int index = hash % map->bucket_count;

    cm_map_entry_t* entry = map->buckets[index];
    while (entry) {
        if (entry->hash == hash && strcmp(entry->key, key) == 0) {
            return entry->value;
        }
        entry = entry->next;
    }
    return NULL;
}

int cm_map_has(cm_map_t* map, const char* key) {
    return cm_map_get(map, key) != NULL;
}

void cm_map_free(cm_map_t* map) {
    if (!map) return;
    for (int i = 0; i < map->bucket_count; i++) {
        cm_map_entry_t* entry = map->buckets[i];
        while (entry) {
            cm_map_entry_t* next = entry->next;
            if (entry->key) cm_free(entry->key);
            if (entry->value) cm_free(entry->value);
            cm_free(entry);
            entry = next;
        }
    }
    if (map->buckets) cm_free(map->buckets);
    cm_free(map);
}

size_t cm_map_size(cm_map_t* map) {
    return map ? (size_t)map->size : 0;
}

int cm_map_save_to_json(cm_map_t* map, const char* filepath) {
    if (!map || !filepath) return 0;
    cm_string_t* json_str = cm_string_new("{");
    int first = 1;
    for (int i = 0; i < map->bucket_count; i++) {
        cm_map_entry_t* entry = map->buckets[i];
        while (entry) {
            if (!first) {
                cm_string_t* t = cm_string_format("%s,", json_str->data);
                cm_string_set(json_str, t->data);
                cm_string_free(t);
            }
            cm_string_t* t2 = cm_string_format("%s\"%s\":\"%s\"", json_str->data, entry->key, (char*)entry->value);
            cm_string_set(json_str, t2->data);
            cm_string_free(t2);
            first = 0;
            entry = entry->next;
        }
    }
    cm_string_t* t3 = cm_string_format("%s}", json_str->data);
    cm_string_set(json_str, t3->data);
    cm_string_free(t3);
    
    int success = cm_file_write(filepath, json_str->data);
    cm_string_free(json_str);
    return success;
}

cm_map_t* cm_map_load_from_json(const char* filepath) {
    if (!filepath) return NULL;
    cm_string_t* raw_json = cm_file_read(filepath);
    if (!raw_json) return NULL;
    
    struct CMJsonNode* root = cm_json_parse(raw_json->data);
    cm_string_free(raw_json);
    
    if (!root || root->type != CM_JSON_OBJECT) {
        if (root) CMJsonNode_delete(root);
        return NULL;
    }
    
    cm_map_t* new_map = cm_map_new();
    cm_map_t* json_map = root->value.object_val;
    for (int i = 0; i < json_map->bucket_count; i++) {
        cm_map_entry_t* entry = json_map->buckets[i];
        while (entry) {
            struct CMJsonNode* val_node = *(struct CMJsonNode**)entry->value;
            if (val_node && val_node->type == CM_JSON_STRING) {
                cm_map_set(new_map, entry->key, val_node->value.string_val->data, strlen(val_node->value.string_val->data) + 1);
            }
            entry = entry->next;
        }
    }
    CMJsonNode_delete(root);
    return new_map;
}
