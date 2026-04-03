#include <stdio.h>
#include "curium/string.h"

int main() {
    /* 15 bytes in UTF-8, but only 5 characters (ñ, é, ó, 漢, 字) */
    curium_string_t* str = curium_string_new("ñéó漢字");
    printf("Raw byte length: %zu\n", curium_string_length(str));
    printf("UTF-8 character length: %zu\n", curium_string_length_utf8(str));
    
    if (curium_string_length_utf8(str) == 5 && curium_string_length(str) > 5) {
        printf("UTF-8 count SUCCESS\n");
    } else {
        printf("UTF-8 count FAILED\n");
    }
    
    curium_string_free(str);
    return 0;
}
