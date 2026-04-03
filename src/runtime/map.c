#include "curium/map.h"
#include "curium/memory.h"
#include "curium/error.h"
#include "curium/json.h"
#include "curium/file.h"
#include "curium/string.h"
#include <string.h>

#define CURIUM_MAP_INITIAL_SIZE 16
#define CURIUM_MAP_LOAD_FACTOR 0.75

static uint32_t curium_hash_string(const char* str) {
    uint32_t hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

curium_map_t* curium_map_new(void) {
    curium_map_t* map = (curium_map_t*)curium_alloc(sizeof(curium_map_t), "map");
    if (!map) return NULL;

    map->bucket_count = CURIUM_MAP_INITIAL_SIZE;
    map->size = 0;
    map->load_factor = CURIUM_MAP_LOAD_FACTOR;
    map->growth_factor = 2;
    map->buckets = (curium_map_entry_t**)curium_alloc(sizeof(curium_map_entry_t*) * map->bucket_count, "map_buckets");

    if (!map->buckets) {
        curium_free(map);
        return NULL;
    }
    memset(map->buckets, 0, sizeof(curium_map_entry_t*) * map->bucket_count);
    return map;
}

static void curium_map_resize(curium_map_t* map, size_t new_size) {
    if (!map) return;
    if (new_size == 0 || new_size > SIZE_MAX / sizeof(curium_map_entry_t*)) return;

    curium_map_entry_t** new_buckets = (curium_map_entry_t**)curium_alloc(sizeof(curium_map_entry_t*) * new_size, "map_buckets");
    if (!new_buckets) return;

    memset(new_buckets, 0, sizeof(curium_map_entry_t*) * new_size);

    for (int i = 0; i < map->bucket_count; i++) {
        curium_map_entry_t* entry = map->buckets[i];
        while (entry) {
            curium_map_entry_t* next = entry->next;
            size_t new_index = entry->hash % new_size;
            entry->next = new_buckets[new_index];
            new_buckets[new_index] = entry;
            entry = next;
        }
    }
    curium_free(map->buckets);
    map->buckets = new_buckets;
    map->bucket_count = (int)new_size;
}

void curium_map_set(curium_map_t* map, const char* key, const void* value, size_t value_size) {
    if (!map || !key || !value) return;

    if (map->bucket_count > 0) {
        double needed_capacity = (double)map->bucket_count * (double)map->load_factor;
        if ((double)map->size >= needed_capacity) {
            size_t new_size = (size_t)map->bucket_count * (size_t)map->growth_factor;
            if (new_size > 0 && new_size <= SIZE_MAX / sizeof(curium_map_entry_t*)) {
                curium_map_resize(map, new_size);
            }
        }
    }

    uint32_t hash = curium_hash_string(key);
    int index = hash % map->bucket_count;

    curium_map_entry_t* entry = map->buckets[index];
    while (entry) {
        if (entry->hash == hash && strcmp(entry->key, key) == 0) {
            curium_free(entry->value);
            entry->value = curium_alloc(value_size, "map_value");
            if (!entry->value) {
                curium_error_set(CURIUM_ERROR_MEMORY, "Out of memory in Map Set");
                return;
            }
            memcpy(entry->value, value, value_size);
            entry->value_size = value_size;
            return;
        }
        entry = entry->next;
    }

    entry = (curium_map_entry_t*)curium_alloc(sizeof(curium_map_entry_t), "map_entry");
    if (!entry) { curium_error_set(CURIUM_ERROR_MEMORY, "Map Entry Mem failure"); return; }
    
    size_t key_len = strlen(key) + 1;
    entry->key = (char*)curium_alloc(key_len, "map_key");
    if (!entry->key) { curium_free(entry); return; }
    memcpy(entry->key, key, key_len);

    entry->value = curium_alloc(value_size, "map_value");
    if (!entry->value) { curium_free(entry->key); curium_free(entry); return; }
    memcpy(entry->value, value, value_size);

    entry->value_size = value_size;
    entry->hash = hash;
    entry->next = map->buckets[index];
    map->buckets[index] = entry;
    map->size++;
}

void* curium_map_get(curium_map_t* map, const char* key) {
    if (!map || !key) return NULL;

    uint32_t hash = curium_hash_string(key);
    int index = hash % map->bucket_count;

    curium_map_entry_t* entry = map->buckets[index];
    while (entry) {
        if (entry->hash == hash && strcmp(entry->key, key) == 0) {
            return entry->value;
        }
        entry = entry->next;
    }
    return NULL;
}

int curium_map_has(curium_map_t* map, const char* key) {
    return curium_map_get(map, key) != NULL;
}

void curium_map_free(curium_map_t* map) {
    if (!map) return;
    for (int i = 0; i < map->bucket_count; i++) {
        curium_map_entry_t* entry = map->buckets[i];
        while (entry) {
            curium_map_entry_t* next = entry->next;
            if (entry->key) curium_free(entry->key);
            if (entry->value) curium_free(entry->value);
            curium_free(entry);
            entry = next;
        }
    }
    if (map->buckets) curium_free(map->buckets);
    curium_free(map);
}

size_t curium_map_size(curium_map_t* map) {
    return map ? (size_t)map->size : 0;
}

int curium_map_save_to_json(curium_map_t* map, const char* filepath) {
    if (!map || !filepath) return 0;
    curium_string_t* json_str = curium_string_new("{");
    int first = 1;
    for (int i = 0; i < map->bucket_count; i++) {
        curium_map_entry_t* entry = map->buckets[i];
        while (entry) {
            if (!first) {
                curium_string_t* t = curium_string_format("%s,", json_str->data);
                curium_string_set(json_str, t->data);
                curium_string_free(t);
            }
            curium_string_t* t2 = curium_string_format("%s\"%s\":\"%s\"", json_str->data, entry->key, (char*)entry->value);
            curium_string_set(json_str, t2->data);
            curium_string_free(t2);
            first = 0;
            entry = entry->next;
        }
    }
    curium_string_t* t3 = curium_string_format("%s}", json_str->data);
    curium_string_set(json_str, t3->data);
    curium_string_free(t3);
    
    int success = curium_file_write(filepath, json_str->data);
    curium_string_free(json_str);
    return success;
}

curium_map_t* curium_map_load_from_json(const char* filepath) {
    if (!filepath) return NULL;
    curium_string_t* raw_json = curium_file_read(filepath);
    if (!raw_json) return NULL;
    
    struct CuriumJsonNode* root = curium_json_parse(raw_json->data);
    curium_string_free(raw_json);
    
    if (!root || root->type != CURIUM_JSON_OBJECT) {
        if (root) CuriumJsonNode_delete(root);
        return NULL;
    }
    
    curium_map_t* new_map = curium_map_new();
    curium_map_t* json_map = root->value.object_val;
    for (int i = 0; i < json_map->bucket_count; i++) {
        curium_map_entry_t* entry = json_map->buckets[i];
        while (entry) {
            struct CuriumJsonNode* val_node = *(struct CuriumJsonNode**)entry->value;
            if (val_node && val_node->type == CURIUM_JSON_STRING) {
                curium_map_set(new_map, entry->key, val_node->value.string_val->data, strlen(val_node->value.string_val->data) + 1);
            }
            entry = entry->next;
        }
    }
    CuriumJsonNode_delete(root);
    return new_map;
}
