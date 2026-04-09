#include "curium/core.h"
#include "curium/error.h"
#include "curium/memory.h"
#include "curium/curium_lang.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    curium_gc_init();
    curium_init_error_detector();

    /* Front-end/codegen smoke test: emit C from a fixture .curium file. */
    int rc = curium_emit_c_file("tests/fixtures/hello.cm", "tests/fixtures/hello_out.c");
    if (rc != 0) {
        fprintf(stderr, "curium_emit_c_file failed: %s\n", curium_error_get_message());
        curium_gc_shutdown();
        return 1;
    }

    /* If we got here, parsing + desugaring + codegen succeeded. */
    assert(rc == 0);

    curium_gc_shutdown();
    return 0;
}

