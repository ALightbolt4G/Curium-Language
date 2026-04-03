/**
 * @file map.h
 * @brief Hash map structures mapping securely.
 */
#ifndef CURIUM_MAP_H
#define CURIUM_MAP_H

#include "core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct curium_map_entry {
    char* key;
    void* value;
    size_t value_size;
    uint32_t hash;
    struct curium_map_entry* next;
} curium_map_entry_t;

struct curium_map {
    curium_map_entry_t** buckets;
    int bucket_count;
    int size;
    float load_factor;
    int growth_factor;
};

/**
 * @brief allocate structural hash base natively.
 */
curium_map_t* curium_map_new(void);

/**
 * @brief resolve dictionary bounds naturally completely.
 */
void curium_map_free(curium_map_t* map);

/**
 * @brief map values internally parsing boundaries aggressively intuitively.
 */
void curium_map_set(curium_map_t* map, const char* key, const void* value, size_t value_size);

/**
 * @brief extract buckets inherently searching mappings sequentially.
 */
void* curium_map_get(curium_map_t* map, const char* key);

/**
 * @brief probe available hash links safely explicitly.
 */
int curium_map_has(curium_map_t* map, const char* key);

/**
 * @brief fetches structural capacities implicitly safely.
 */
size_t curium_map_size(curium_map_t* map);

/**
 * @brief serialize string maps to JSON.
 */
int curium_map_save_to_json(struct curium_map* map, const char* filepath);

/**
 * @brief deserialize map from JSON file natively.
 */
struct curium_map* curium_map_load_from_json(const char* filepath);

#ifdef __cplusplus
}
#endif

/**
 * @brief Macro for iterating over map entries.
 */
#define curium_map_foreach(map, key_var, value_var) \
    for (int _i = 0; _i < (map)->bucket_count; _i++) \
        for (curium_map_entry_t* _e = (map)->buckets[_i]; _e; _e = _e->next) \
            for (const char* key_var = _e->key; key_var; key_var = NULL) \
                for (void* value_var = _e->value; value_var; value_var = NULL)

#endif /* CURIUM_MAP_H */
