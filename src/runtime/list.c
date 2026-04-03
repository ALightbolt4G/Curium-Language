#include "curium/list.h"
#include "curium/error.h"
#include <stdio.h>
#include <string.h>

static curium_list_node_t* curium_list_node_new(void* data) {
    curium_list_node_t* node = (curium_list_node_t*)curium_alloc(sizeof(curium_list_node_t), "list_node");
    if (!node) return NULL;
    node->data = data;
    node->next = NULL;
    node->prev = NULL;
    return node;
}

static void curium_list_node_free(curium_list_node_t* node) {
    if (node) curium_free(node);
}

curium_list_t* curium_list_new(void) {
    curium_list_t* list = (curium_list_t*)curium_alloc(sizeof(curium_list_t), "list");
    if (!list) return NULL;
    list->head = NULL;
    list->tail = NULL;
    list->count = 0;
    list->gc_handle.index = 0;
    list->gc_handle.generation = 0;
    return list;
}

void curium_list_free(curium_list_t* list) {
    if (!list) return;
    curium_list_clear(list);
    curium_free(list);
}

int curium_list_append(curium_list_t* list, void* data) {
    if (!list) return -1;
    
    curium_list_node_t* node = curium_list_node_new(data);
    if (!node) return -1;
    
    if (list->tail) {
        node->prev = list->tail;
        list->tail->next = node;
        list->tail = node;
    } else {
        list->head = list->tail = node;
    }
    list->count++;
    return 0;
}

int curium_list_prepend(curium_list_t* list, void* data) {
    if (!list) return -1;
    
    curium_list_node_t* node = curium_list_node_new(data);
    if (!node) return -1;
    
    if (list->head) {
        node->next = list->head;
        list->head->prev = node;
        list->head = node;
    } else {
        list->head = list->tail = node;
    }
    list->count++;
    return 0;
}

int curium_list_remove(curium_list_t* list, void* data) {
    if (!list) return -1;
    
    curium_list_node_t* current = list->head;
    while (current) {
        if (current->data == data) {
            if (current->prev) {
                current->prev->next = current->next;
            } else {
                list->head = current->next;
            }
            if (current->next) {
                current->next->prev = current->prev;
            } else {
                list->tail = current->prev;
            }
            curium_list_node_free(current);
            list->count--;
            return 0;
        }
        current = current->next;
    }
    return -1;
}

int curium_list_remove_at(curium_list_t* list, size_t index) {
    if (!list || index >= list->count) return -1;
    
    curium_list_node_t* current = list->head;
    for (size_t i = 0; i < index && current; i++) {
        current = current->next;
    }
    
    if (!current) return -1;
    
    if (current->prev) {
        current->prev->next = current->next;
    } else {
        list->head = current->next;
    }
    if (current->next) {
        current->next->prev = current->prev;
    } else {
        list->tail = current->prev;
    }
    curium_list_node_free(current);
    list->count--;
    return 0;
}

void* curium_list_get(curium_list_t* list, size_t index) {
    if (!list || index >= list->count) {
        curium_error_set(CURIUM_ERROR_OUT_OF_BOUNDS, "list index out of bounds");
        return NULL;
    }
    
    curium_list_node_t* current = list->head;
    for (size_t i = 0; i < index && current; i++) {
        current = current->next;
    }
    
    return current ? current->data : NULL;
}

int curium_list_set(curium_list_t* list, size_t index, void* data) {
    if (!list || index >= list->count) {
        curium_error_set(CURIUM_ERROR_OUT_OF_BOUNDS, "list index out of bounds");
        return -1;
    }
    
    curium_list_node_t* current = list->head;
    for (size_t i = 0; i < index && current; i++) {
        current = current->next;
    }
    
    if (!current) return -1;
    current->data = data;
    return 0;
}

long curium_list_index_of(curium_list_t* list, void* data) {
    if (!list) return -1;
    
    curium_list_node_t* current = list->head;
    long index = 0;
    while (current) {
        if (current->data == data) return index;
        current = current->next;
        index++;
    }
    return -1;
}

int curium_list_contains(curium_list_t* list, void* data) {
    return curium_list_index_of(list, data) >= 0;
}

size_t curium_list_size(curium_list_t* list) {
    return list ? list->count : 0;
}

int curium_list_empty(curium_list_t* list) {
    return !list || list->count == 0;
}

void curium_list_clear(curium_list_t* list) {
    if (!list) return;
    
    curium_list_node_t* current = list->head;
    while (current) {
        curium_list_node_t* next = current->next;
        curium_list_node_free(current);
        current = next;
    }
    list->head = NULL;
    list->tail = NULL;
    list->count = 0;
}

void curium_list_foreach(curium_list_t* list, curium_list_iter_cb callback, void* userdata) {
    if (!list || !callback) return;
    
    curium_list_node_t* current = list->head;
    size_t index = 0;
    while (current) {
        callback(current->data, index++, userdata);
        current = current->next;
    }
}

void curium_list_print(curium_list_t* list, curium_list_to_string_fn to_string) {
    if (!list) {
        printf("list: null\n");
        return;
    }
    
    printf("list[%zu] { ", list->count);
    curium_list_node_t* current = list->head;
    int first = 1;
    while (current) {
        if (!first) printf(", ");
        first = 0;
        
        if (to_string && current->data) {
            curium_string_t* str = to_string(current->data);
            if (str && str->data) {
                printf("%s", str->data);
            } else {
                printf("null");
            }
            if (str) curium_string_free(str);
        } else {
            printf("%p", current->data);
        }
        current = current->next;
    }
    printf(" }\n");
}
