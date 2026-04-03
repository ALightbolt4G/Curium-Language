#include <stdio.h>
#include "curium/http.h"
#include "curium/string.h"

int main() {
    printf("Testing HTTPS direct call...\n");
    CHttpResponse* res = curium_http_get("https://example.com");
    if (res) {
        printf("Secure HTTP GET returned status: %d\n", res->status_code);
        printf("Body length: %zu\n", curium_string_length(res->body));
        CHttpResponse_delete(res);
    } else {
        printf("HTTPS execution failed.\n");
    }
    return 0;
}
