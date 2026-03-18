#include "cm/array.h"
#include "cm/memory.h"
#include <string.h>

cm_array_t* cm_array_new(size_t element_size, size_t initial_capacity) {
    if (element_size == 0) return NULL;
    cm_array_t* arr = (cm_array_t*)cm_alloc(sizeof(cm_array_t), "array");
    if (!arr) return NULL;

    arr->element_size = element_size;
    arr->capacity = initial_capacity > 0 ? initial_capacity : 16;
    arr->length = 0;
    arr->data = cm_alloc(element_size * arr->capacity, "array_data");
    arr->ref_counts = (int*)cm_alloc(sizeof(int) * arr->capacity, "array_refs");
    arr->element_destructor = NULL;
    arr->flags = 0;

    if (!arr->data || !arr->ref_counts) {
        if (arr->data) cm_free(arr->data);
        if (arr->ref_counts) cm_free(arr->ref_counts);
        cm_free(arr);
        return NULL;
    }

    memset(arr->ref_counts, 0, sizeof(int) * arr->capacity);
    return arr;
}

void cm_array_free(cm_array_t* arr) {
    if (!arr) return;

    if (arr->element_destructor) {
        for (size_t i = 0; i < arr->length; i++) {
            void* elem = (char*)arr->data + (i * arr->element_size);
            arr->element_destructor(elem);
        }
    }

    cm_free(arr->data);
    cm_free(arr->ref_counts);
    cm_free(arr);
}

void* cm_array_get(cm_array_t* arr, size_t index) {
    if (!arr) return NULL;
    if (index >= arr->length) return NULL;

    arr->ref_counts[index]++;
    return (char*)arr->data + (index * arr->element_size);
}

void cm_array_push(cm_array_t* arr, const void* value) {
    if (!arr || !value) return;

    if (arr->length >= arr->capacity) {
        size_t new_capacity = arr->capacity * 2;
        void* new_data = cm_alloc(arr->element_size * new_capacity, "array_data");
        int* new_refs = (int*)cm_alloc(sizeof(int) * new_capacity, "array_refs");

        if (!new_data || !new_refs) {
            if (new_data) cm_free(new_data);
            if (new_refs) cm_free(new_refs);
            return;
        }

        memcpy(new_data, arr->data, arr->element_size * arr->length);
        memset(new_refs, 0, sizeof(int) * new_capacity);

        cm_free(arr->data);
        cm_free(arr->ref_counts);

        arr->data = new_data;
        arr->ref_counts = new_refs;
        arr->capacity = new_capacity;
    }

    void* dest = (char*)arr->data + (arr->length * arr->element_size);
    memcpy(dest, value, arr->element_size);
    arr->length++;
}

void* cm_array_pop(cm_array_t* arr) {
    if (!arr || arr->length == 0) return NULL;
    arr->length--;
    return (char*)arr->data + (arr->length * arr->element_size);
}

size_t cm_array_length(cm_array_t* arr) {
    return arr ? arr->length : 0;
}
