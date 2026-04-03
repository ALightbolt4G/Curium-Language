#include "curium/core.h"
#include "curium/packages.h"
#include "curium/error.h"
#include "curium/memory.h"
#include <stdio.h>
#include <string.h>

static void curium_packages_print_usage(void) {
    fprintf(stderr,
        "Cur Package Manager v3.0\n"
        "Usage:\n"
        "  cur init [<name>]           Initialize new curium.json\n"
        "  cur install [name@version]  Install package(s)\n"
        "  cur remove <name>           Remove package\n"
        "  cur update [name]           Update package(s)\n"
        "  cur list                    List installed packages\n"
        "  cur search <query>          Search registry\n"
        "  cur help                    Show this help\n"
    );
}

int main(int argc, char** argv) {
    curium_gc_init();
    curium_init_error_detector();

    if (argc < 2) {
        curium_packages_print_usage();
        curium_gc_shutdown();
        return 1;
    }

    const char* sub = argv[1];
    int rc = 1;

    if (strcmp(sub, "help") == 0) {
        curium_packages_print_usage();
        rc = 0;
    } else if (strcmp(sub, "init") == 0) {
        const char* name = (argc >= 3) ? argv[2] : NULL;
        rc = curium_packages_cmd_init(name);
    } else if (strcmp(sub, "install") == 0) {
        const char* pkg_spec = (argc >= 3) ? argv[2] : NULL;
        const char* version = NULL;
        if (pkg_spec) {
            char* at = strchr(pkg_spec, '@');
            if (at) {
                *at = '\0';
                version = at + 1;
            }
        }
        rc = curium_packages_cmd_install(pkg_spec, version);
    } else if (strcmp(sub, "remove") == 0) {
        if (argc < 3) { fprintf(stderr, "Error: Package name required\n"); curium_gc_shutdown(); return 1; }
        rc = curium_packages_cmd_remove(argv[2]);
    } else if (strcmp(sub, "update") == 0) {
        const char* name = (argc >= 3) ? argv[2] : NULL;
        rc = curium_packages_cmd_update(name);
    } else if (strcmp(sub, "list") == 0) {
        rc = curium_packages_cmd_list();
    } else if (strcmp(sub, "search") == 0) {
        if (argc < 3) { fprintf(stderr, "Error: Search query required\n"); curium_gc_shutdown(); return 1; }
        rc = curium_packages_cmd_search(argv[2]);
    } else {
        fprintf(stderr, "Unknown command: cur %s\n", sub);
        curium_packages_print_usage();
    }

    curium_gc_shutdown();
    return rc;
}
