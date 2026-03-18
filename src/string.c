#include "cm/string.h"
#include "cm/memory.h"
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#define CM_STRING_COPY 0x01
#define CM_STRING_NOCOPY 0x02

cm_string_t* cm_string_new(const char* initial) {
    cm_string_t* s = (cm_string_t*)cm_alloc(sizeof(cm_string_t), "string");
    if (!s) return NULL;

    size_t len = initial ? strlen(initial) : 0;
    s->length = len;
    s->capacity = len + 1;
    s->data = (char*)cm_alloc(s->capacity, "string_data");
    s->ref_count = 1;
    s->hash = 0;
    s->created = time(NULL);
    s->flags = CM_STRING_COPY;

    if (s->data) {
        if (initial && len > 0) {
            memcpy(s->data, initial, len + 1);
        } else {
            s->data[0] = '\0';
        }
    }
    return s;
}

void cm_string_free(cm_string_t* s) {
    if (!s) return;
    s->ref_count--;
    if (s->ref_count <= 0) {
        if (s->data && !(s->flags & CM_STRING_NOCOPY)) {
            cm_free(s->data);
        }
        cm_free(s);
    }
}

size_t cm_string_length(cm_string_t* s) {
    return s ? s->length : 0;
}

cm_string_t* cm_string_format(const char* format, ...) {
    if (!format) return NULL;
    va_list args;
    va_start(args, format);
    int size = vsnprintf(NULL, 0, format, args);
    va_end(args);

    if (size < 0) return NULL;
    char* buffer = (char*)malloc(size + 1);
    if (!buffer) return NULL;

    va_start(args, format);
    vsnprintf(buffer, size + 1, format, args);
    va_end(args);

    cm_string_t* result = cm_string_new(buffer);
    free(buffer);
    return result;
}

void cm_string_set(cm_string_t* s, const char* value) {
    if (!s) return;
    if (!value) value = "";
    size_t len = strlen(value);

    if (len + 1 > s->capacity || (s->flags & CM_STRING_NOCOPY)) {
        char* new_data = (char*)cm_alloc(len + 1, "string_data");
        if (!new_data) return;

        if (s->data && !(s->flags & CM_STRING_NOCOPY)) {
            cm_free(s->data);
        }

        s->data = new_data;
        s->capacity = len + 1;
        s->flags &= ~CM_STRING_NOCOPY;
    }

    memcpy(s->data, value, len + 1);
    s->length = len;
    s->hash = 0;
}

void cm_string_append(cm_string_t* s, const char* value) {
    if (!s || !value) return;
    size_t append_len = strlen(value);
    if (append_len == 0) return;

    if (s->length + append_len + 1 > s->capacity) {
        size_t new_cap = (s->length + append_len + 1) * 2;
        char* new_data = (char*)cm_alloc(new_cap, "string_data");
        if (!new_data) return;
        if (s->data) {
            memcpy(new_data, s->data, s->length);
            cm_free(s->data);
        }
        s->data = new_data;
        s->capacity = new_cap;
    }

    memcpy(s->data + s->length, value, append_len);
    s->length += append_len;
    s->data[s->length] = '\0';
    s->hash = 0;
}

void cm_string_upper(cm_string_t* s) {
    if (!s || !s->data) return;
    for (size_t i = 0; i < s->length; i++) {
        s->data[i] = toupper(s->data[i]);
    }
    s->hash = 0;
}

void cm_string_lower(cm_string_t* s) {
    if (!s || !s->data) return;
    for (size_t i = 0; i < s->length; i++) {
        s->data[i] = tolower(s->data[i]);
    }
    s->hash = 0;
}

cm_string_t* cm_input(const char* prompt) {
    if (prompt) {
        printf("%s", prompt);
        fflush(stdout);
    }
    char buffer[2048] = {0};
    if (fgets(buffer, sizeof(buffer), stdin)) {
        buffer[strcspn(buffer, "\n")] = 0;
        return cm_string_new(buffer);
    }
    return NULL;
}
