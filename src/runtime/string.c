#include "curium/string.h"
#include "curium/memory.h"
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#define CURIUM_STRING_COPY 0x01
#define CURIUM_STRING_NOCOPY 0x02

curium_string_t* curium_string_new(const char* initial) {
    curium_string_t* s = (curium_string_t*)curium_alloc(sizeof(curium_string_t), "string");
    if (!s) return NULL;

    size_t len = initial ? strlen(initial) : 0;
    s->length = len;
    s->capacity = len + 1;
    s->data = (char*)curium_alloc(s->capacity, "string_data");
    s->ref_count = 1;
    s->hash = 0;
    s->created = time(NULL);
    s->flags = CURIUM_STRING_COPY;

    if (s->data) {
        if (initial && len > 0) {
            memcpy(s->data, initial, len + 1);
        } else {
            s->data[0] = '\0';
        }
    }
    return s;
}

void curium_string_free(curium_string_t* s) {
    if (!s) return;
    s->ref_count--;
    if (s->ref_count <= 0) {
        if (s->data && !(s->flags & CURIUM_STRING_NOCOPY)) {
            curium_free(s->data);
        }
        curium_free(s);
    }
}

size_t curium_string_length(curium_string_t* s) {
    return s ? s->length : 0;
}

size_t curium_string_length_utf8(curium_string_t* s) {
    if (!s || !s->data) return 0;
    size_t count = 0;
    const unsigned char* p = (const unsigned char*)s->data;
    while (*p) {
        if ((*p & 0xC0) != 0x80) {
            count++;
        }
        p++;
    }
    return count;
}

curium_string_t* curium_string_format(const char* format, ...) {
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

    curium_string_t* result = curium_string_new(buffer);
    free(buffer);
    return result;
}

void curium_string_set(curium_string_t* s, const char* value) {
    if (!s) return;
    if (!value) value = "";
    size_t len = strlen(value);

    if (len + 1 > s->capacity || (s->flags & CURIUM_STRING_NOCOPY)) {
        char* new_data = (char*)curium_alloc(len + 1, "string_data");
        if (!new_data) return;

        if (s->data && !(s->flags & CURIUM_STRING_NOCOPY)) {
            curium_free(s->data);
        }

        s->data = new_data;
        s->capacity = len + 1;
        s->flags &= ~CURIUM_STRING_NOCOPY;
    }

    memcpy(s->data, value, len + 1);
    s->length = len;
    s->hash = 0;
}

void curium_string_append(curium_string_t* s, const char* value) {
    if (!s || !value) return;
    size_t append_len = strlen(value);
    if (append_len == 0) return;

    if (s->length + append_len + 1 > s->capacity) {
        size_t new_cap = (s->length + append_len + 1) * 2;
        char* new_data = (char*)curium_alloc(new_cap, "string_data");
        if (!new_data) return;
        if (s->data) {
            memcpy(new_data, s->data, s->length);
            curium_free(s->data);
        }
        s->data = new_data;
        s->capacity = new_cap;
    }

    memcpy(s->data + s->length, value, append_len);
    s->length += append_len;
    s->data[s->length] = '\0';
    s->hash = 0;
}

void curium_string_upper(curium_string_t* s) {
    if (!s || !s->data) return;
    for (size_t i = 0; i < s->length; i++) {
        s->data[i] = toupper(s->data[i]);
    }
    s->hash = 0;
}

void curium_string_lower(curium_string_t* s) {
    if (!s || !s->data) return;
    for (size_t i = 0; i < s->length; i++) {
        s->data[i] = tolower(s->data[i]);
    }
    s->hash = 0;
}

curium_string_t* curium_input(curium_string_t* prompt) {
    if (prompt && prompt->data) {
        printf("%s", prompt->data);
        fflush(stdout);
    }
    char buffer[2048] = {0};
    if (fgets(buffer, sizeof(buffer), stdin)) {
        buffer[strcspn(buffer, "\n")] = 0;
        return curium_string_new(buffer);
    }
    return NULL;
}

void curium_print_str(curium_string_t* s) {
    if (!s || !s->data) return;
    printf("%s", s->data);
}

void curium_println_str(curium_string_t* s) {
    if (s && s->data) {
        printf("%s", s->data);
    }
}

void curium_print_int(int x) { printf("%d", x); }
void curium_println_int(int x) { printf("%d", x); }
void curium_print_float(double x) { printf("%g", x); }
void curium_println_float(double x) { printf("%g", x); }
void curium_print_cstr(const char* x) { if(x) printf("%s", x); }

void curium_println_cstr(const char* x) { if(x) printf("%s", x); }


