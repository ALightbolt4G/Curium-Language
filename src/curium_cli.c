#include "curium/core.h"
#include "curium/error.h"
#include "curium/memory.h"
#include "curium/curium_lang.h"
#include "curium/curium_highlight.h"
#include "curium/cmd.h"
#include "curium/project.h"
#include "curium/packages.h"
#include "curium/compiler/lexer.h"
#include "curium/compiler/ast_v2.h"
#include "curium/compiler/parser.h"
#include "curium/compiler/typecheck.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

#ifndef _WIN32
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#define curium_mkdir(dir) mkdir((dir), 0755)
#define curium_access access
#define CURIUM_DEFAULT_OUT "a.out"
#else
#include <sys/stat.h>
#include <direct.h>
#include <io.h>
#define curium_mkdir(dir) _mkdir(dir)
#define curium_access _access
#define CURIUM_DEFAULT_OUT "a.exe"
#endif
#include <ctype.h>

static char* curium_doctor_read_file(const char* path, size_t* out_len);

static void curium_string_append_char(curium_string_t* str, char c) {
    char buf[2] = {c, '\0'};
    curium_string_append(str, buf);
}

static void curium_print_usage(void) {
    fprintf(stderr,
        "cm(curium) V 4.0 — Build Frameworks, Compilers & Dynamic AI\n"
        "Usage:\n"
        "  curium init   <project-name>            Create new Curium project\n"
        "  curium build  [entry.cm] [-o output]    Build Curium program\n"
        "  curium run    [entry.cm] [-o output]    Build and run Curium program\n"
        "  curium check  [file.cm]                 Type check only (fast)\n"
        "  curium doctor [project-dir]             Diagnose project health\n"
        "  curium test                             Run project tests\n"
        "  curium fmt    [file.cm]                 Format source files\n"
        "  curium install [-o path]                Install binary to system\n"
        "  curium emitc  <entry.cm> [-o output.c]  Emit C code only\n"
        "\n"
        "Package Manager:\n"
        "  curium packages init [<name>]           Initialize new project\n"
        "  curium packages install [name@version]  Install package(s)\n"
        "  curium packages remove <name>           Remove package\n"
        "  curium packages update [name]           Update package(s)\n"
        "  curium packages list                    List installed packages\n"
        "  curium packages search <query>          Search registry\n"
        "\n"
        "Options:\n"
        "  -o <path>                           Specify output path\n"
        "  --emit-c                            Emit C code, don't compile\n"
        "  --stat                              Show GC statistics\n"
        "  -v, --version                       Show version\n"
    );
}

static int curium_use_colors(void) {
#ifdef _WIN32
    return _isatty(_fileno(stdout));
#else
    return isatty(fileno(stdout));
#endif
}

static void curium_print_neon_header(const char* project_name) {
    int colors = curium_use_colors();
    const char* cyan = colors ? "\x1b[36m" : "";
    const char* bold = colors ? "\x1b[1m" : "";
    const char* reset = colors ? "\x1b[0m" : "";
    
    printf("\n%s⚡ %scm(curium) V 4.0%s\n", bold, cyan, reset);
    printf("  %s═══════════════════════%s\n", cyan, reset);
    if (project_name && project_name[0]) {
        printf("  📂 Project: %s%s%s\n", bold, project_name, reset);
    }
}

static void curium_print_neon_progress(int percent) {
    int colors = curium_use_colors();
    const char* green = colors ? "\x1b[32m" : "";
    const char* reset = colors ? "\x1b[0m" : "";
    
    int filled = percent / 10;
    printf("  🔨 Building... [");
    for (int i = 0; i < 10; i++) {
        if (i < filled) {
            printf("%s█%s", green, reset);
        } else {
            printf("░");
        }
    }
    printf("] %d%%\n", percent);
}

static void curium_print_neon_success(const char* output, double build_time) {
    int colors = curium_use_colors();
    const char* green = colors ? "\x1b[32m" : "";
    const char* yellow = colors ? "\x1b[33m" : "";
    const char* reset = colors ? "\x1b[0m" : "";
    const char* bold = colors ? "\x1b[1m" : "";
    
    /* Get file size */
    long size = 0;
    FILE* f = fopen(output, "rb");
    if (f) {
        fseek(f, 0, SEEK_END);
        size = ftell(f);
        fclose(f);
    }
    
    const char* size_str = "";
    char size_buf[32];
    if (size > 0) {
        if (size < 1024) {
            snprintf(size_buf, sizeof(size_buf), " (%ldB)", size);
        } else if (size < 1024 * 1024) {
            snprintf(size_buf, sizeof(size_buf), " (%.1fKB)", size / 1024.0);
        } else {
            snprintf(size_buf, sizeof(size_buf), " (%.1fMB)", size / (1024.0 * 1024.0));
        }
        size_str = size_buf;
    }
    
    printf("  %s✅ Success:%s %s%s%s generated%s\n", green, reset, bold, output, reset, size_str);
    if (build_time > 0) {
        printf("  ⏱️  Build time: %s%.2fs%s\n", yellow, build_time, reset);
    }
    printf("  ✨ Happy coding!\n\n");
}

static void curium_print_neon_error(const char* message) {
    int colors = curium_use_colors();
    const char* red = colors ? "\x1b[31m" : "";
    const char* reset = colors ? "\x1b[0m" : "";
    const char* bold = colors ? "\x1b[1m" : "";
    
    printf("\n  %s❌ Error:%s %s%s%s\n\n", bold, reset, red, message, reset);
}

static double curium_get_time(void) {
    return (double)clock() / CLOCKS_PER_SEC;
}

static const char* curium_detect_entry_file(int argc, char** argv) {
    if (argc >= 3 && argv[2][0] != '-') {
        return argv[2];
    }
    if (curium_access("src/main.cm", 0) == 0) {
        return "src/main.cm";
    }
    if (curium_access("main.cm", 0) == 0) {
        return "main.cm";
    }
    return NULL;
}

static int curium_run_tests(const char* dir) {
    int pass = 0, fail = 0;
#ifndef _WIN32
    DIR* d = opendir(dir);
    if (!d) return 0;
    struct dirent* ent;
    while ((ent = readdir(d)) != NULL) {
        if (strstr(ent->d_name, "_test.cm")) {
            char full[1024];
            snprintf(full, sizeof(full), "%s/%s", dir, ent->d_name);
            printf("  🧪 Compiling %s...\n", full);
            int rc = curium_compile_file(full, "curium_test.out");
            if (rc == 0) {
                curium_cmd_t* c = curium_cmd_new("./curium_test.out");
                curium_cmd_result_t* r = c ? curium_cmd_run(c) : NULL;
                if (r && r->exit_code == 0) {
                    printf("  ✅ PASS: %s\n", ent->d_name);
                    pass++;
                } else {
                    printf("  ❌ FAIL: %s\n", ent->d_name);
                    fail++;
                }
                if (c) curium_cmd_free(c);
                if (r) curium_cmd_result_free(r);
            } else {
                printf("  ❌ COMPILATION FAILED: %s\n", ent->d_name);
                fail++;
            }
        }
    }
    closedir(d);
#else
    char pattern[1024];
    snprintf(pattern, sizeof(pattern), "%s\\*_test.cm", dir);
    struct _finddata_t data;
    intptr_t handle = _findfirst(pattern, &data);
    if (handle != -1L) {
        do {
            char full[1024];
            snprintf(full, sizeof(full), "%s\\%s", dir, data.name);
            printf("  🧪 Compiling %s...\n", full);
            int rc = curium_compile_file(full, "curium_test.exe");
            if (rc == 0) {
                curium_cmd_t* c = curium_cmd_new("curium_test.exe");
                curium_cmd_result_t* r = c ? curium_cmd_run(c) : NULL;
                if (r && r->exit_code == 0) {
                    printf("  ✅ PASS: %s\n", data.name);
                    pass++;
                } else {
                    printf("  ❌ FAIL: %s\n", data.name);
                    fail++;
                }
                if (c) curium_cmd_free(c);
                if (r) curium_cmd_result_free(r);
            } else {
                printf("  ❌ COMPILATION FAILED: %s\n", data.name);
                fail++;
            }
        } while (_findnext(handle, &data) == 0);
        _findclose(handle);
    }
#endif
    printf("\n  📊 Test Results: %d passed, %d failed\n\n", pass, fail);
    return fail > 0 ? 1 : 0;
}

static int curium_format_file(const char* path) {
    size_t len;
    char* src = curium_doctor_read_file(path, &len);
    if (!src) return -1;
    
    curium_string_t* out = curium_string_new("");
    int indent = 0;
    int in_str = 0;
    int in_char = 0;
    int in_line_comment = 0;
    int in_block_comment = 0;
    int needs_indent = 0;
    
    for (size_t i = 0; i < len; i++) {
        char c = src[i];
        char next = (i + 1 < len) ? src[i+1] : '\0';
        
        if (in_line_comment) {
            curium_string_append_char(out, c);
            if (c == '\n') {
                in_line_comment = 0;
                needs_indent = 1;
            }
            continue;
        }
        if (in_block_comment) {
            curium_string_append_char(out, c);
            if (c == '*' && next == '/') {
                curium_string_append_char(out, next);
                i++;
                in_block_comment = 0;
            }
            continue;
        }
        if (in_str) {
            curium_string_append_char(out, c);
            if (c == '\\' && next) { curium_string_append_char(out, next); i++; }
            else if (c == '"') in_str = 0;
            continue;
        }
        if (in_char) {
            curium_string_append_char(out, c);
            if (c == '\\' && next) { curium_string_append_char(out, next); i++; }
            else if (c == '\'') in_char = 0;
            continue;
        }
        
        if (c == '/' && next == '/') {
            if (needs_indent) {
                for(int j=0; j<indent*4; j++) curium_string_append_char(out, ' ');
                needs_indent = 0;
            }
            in_line_comment = 1;
            curium_string_append_char(out, c);
            curium_string_append_char(out, next);
            i++;
            continue;
        }
        if (c == '/' && next == '*') {
            if (needs_indent) {
                for(int j=0; j<indent*4; j++) curium_string_append_char(out, ' ');
                needs_indent = 0;
            }
            in_block_comment = 1;
            curium_string_append_char(out, c);
            curium_string_append_char(out, next);
            i++;
            continue;
        }
        if (c == '"') in_str = 1;
        if (c == '\'') in_char = 1;
        
        if (c == '{') {
            if (needs_indent) {
                for(int j=0; j<indent*4; j++) curium_string_append_char(out, ' ');
                needs_indent = 0;
            }
            curium_string_append_char(out, '{');
            curium_string_append_char(out, '\n');
            indent++;
            needs_indent = 1;
            while(i+1 < len && isspace((unsigned char)src[i+1])) i++;
            continue;
        }
        if (c == '}') {
            indent--;
            if (indent < 0) indent = 0;
            if (needs_indent || (out->length > 0 && out->data[out->length-1] == '\n')) {
                for(int j=0; j<indent*4; j++) curium_string_append_char(out, ' ');
            }
            curium_string_append_char(out, '}');
            needs_indent = 0;
            continue;
        }
        if (c == ';') {
            curium_string_append_char(out, ';');
            curium_string_append_char(out, '\n');
            needs_indent = 1;
            while(i+1 < len && isspace((unsigned char)src[i+1])) {
                if (src[i+1] == '\n' && i+2 < len && src[i+2] == '\n') {
                    curium_string_append_char(out, '\n');
                    i++;
                }
                i++;
            }
            continue;
        }
        
        if (c == '\n') {
            curium_string_append_char(out, '\n');
            needs_indent = 1;
            while(i+1 < len && isspace((unsigned char)src[i+1]) && src[i+1] != '\n') i++;
            continue;
        }
        
        if (needs_indent && !isspace((unsigned char)c)) {
            for(int j=0; j<indent*4; j++) curium_string_append_char(out, ' ');
            needs_indent = 0;
        }
        
        curium_string_append_char(out, c);
    }
    
    FILE* fw = fopen(path, "wb");
    if (fw) {
        fwrite(out->data, 1, out->length, fw);
        fclose(fw);
    }
    
    curium_string_free(out);
    free(src);
    return 0;
}

/* ============================================================================
 * cm doctor — comprehensive project health checks
 *
 * Checks (Rust-safety-inspired: catch problems early before they cause crashes):
 *   1. curium.json / curium_config.json config file present & readable
 *   2. src/ directory + at least one .cm source file exists
 *   3. A C compiler (gcc or clang) is in PATH
 *   4. CM runtime header (curium/core.h) is accessible from include/
 *   5. Each .cm file lexes & parses without errors (v2 pipeline)
 *   6. Raw malloc/free without gc namespace detected (memory safety warning)
 * ==========================================================================*/

/* Helper: try to find a command in PATH by attempting a version flag */
static int curium_doctor_cmd_exists(const char* cmd) {
    char buf[256];
#ifdef _WIN32
    snprintf(buf, sizeof(buf), "where %s >NUL 2>&1", cmd);
#else
    snprintf(buf, sizeof(buf), "which %s >/dev/null 2>&1", cmd);
#endif
    return system(buf) == 0;
}

/* Helper: read entire file into a newly allocated buffer (caller must free) */
static char* curium_doctor_read_file(const char* path, size_t* out_len) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); return NULL; }
    char* buf = (char*)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t rd = fread(buf, 1, (size_t)sz, f);
    if (rd != (size_t)sz) {
        free(buf);
        buf = NULL;
        fclose(f);
        return NULL;
    }
    buf[rd] = '\0';
    if (out_len) *out_len = rd;
    return buf;
}

typedef struct {
    int colors;
    const char* green;
    const char* yellow;
    const char* red;
    const char* cyan;
    const char* bold;
    const char* reset;
} curium_doctor_theme_t;

static curium_doctor_theme_t curium_doctor_theme(void) {
    curium_doctor_theme_t t;
    t.colors = curium_use_colors();
    t.green  = t.colors ? "\x1b[32m" : "";
    t.yellow = t.colors ? "\x1b[33m" : "";
    t.red    = t.colors ? "\x1b[31m" : "";
    t.cyan   = t.colors ? "\x1b[36m" : "";
    t.bold   = t.colors ? "\x1b[1m"  : "";
    t.reset  = t.colors ? "\x1b[0m"  : "";
    return t;
}

#define DR_OK(fmt, ...)  printf("  %s✅%s " fmt "\n", th.green,  th.reset, ##__VA_ARGS__)
#define DR_WARN(fmt, ...) printf("  %s⚠️ %s " fmt "\n", th.yellow, th.reset, ##__VA_ARGS__)
#define DR_FAIL(fmt, ...) printf("  %s❌%s " fmt "\n", th.red,    th.reset, ##__VA_ARGS__); issues++
#define DR_INFO(fmt, ...) printf("  %s🔍%s " fmt "\n", th.cyan,   th.reset, ##__VA_ARGS__)

/* Scan a CM source string for bare malloc/free (memory safety check) */
static int curium_doctor_has_raw_malloc(const char* src) {
    /* Look for malloc( or free( NOT preceded by curium_ or gc. */
    const char* p = src;
    while ((p = strstr(p, "malloc(")) != NULL) {
        /* Check the 4 chars before 'malloc' */
        if (p >= src + 4 &&
            strncmp(p - 4, "curium_",  3) != 0 &&
            strncmp(p - 3, "gc.",  3) != 0) {
            return 1;
        }
        p++;
    }
    return 0;
}
static int curium_doctor_has_raw_free(const char* src) {
    const char* p = src;
    while ((p = strstr(p, "free(")) != NULL) {
        if (p >= src + 4 &&
            strncmp(p - 4, "curium_",  3) != 0 &&
            strncmp(p - 3, "gc.",  3) != 0) {
            return 1;
        }
        p++;
    }
    return 0;
}

static int curium_doctor(const char* project_dir) {
    curium_doctor_theme_t th = curium_doctor_theme();
    int issues = 0;
    char path[512];

    printf("\n%s⚕️  Curium Doctor — Project Health Report%s\n", th.bold, th.reset);
    printf("  %s━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━%s\n", th.cyan, th.reset);
    printf("  Scanning: %s%s%s\n\n", th.bold, project_dir, th.reset);

    /* ── Check 1: Config file ──────────────────────────────────────────── */
    snprintf(path, sizeof(path), "%s/curium.json", project_dir);
    FILE* cfg = fopen(path, "r");
    if (!cfg) {
        snprintf(path, sizeof(path), "%s/curium_config.json", project_dir);
        cfg = fopen(path, "r");
    }
    if (cfg) {
        /* Quick JSON validity: look for opening { */
        int ch = fgetc(cfg);
        while (ch == ' ' || ch == '\n' || ch == '\r' || ch == '\t') ch = fgetc(cfg);
        if (ch == '{') {
            DR_OK("Config file found and looks valid (%s)", path);
        } else {
            DR_FAIL("Config file exists but may be malformed (no opening '{') — %s", path);
        }
        fclose(cfg);
    } else {
        DR_WARN("No curium.json found in %s (run 'cm init <name>' to create one)", project_dir);
    }

    /* ── Check 2: Source files ─────────────────────────────────────────── */
    {
        char src_dir[512];
        snprintf(src_dir, sizeof(src_dir), "%s/src", project_dir);
        int src_found = 0;
        int curium_files  = 0;

#ifdef _WIN32
        /* Use dir /b to count .cm files */
        char cmd[2048];
        snprintf(cmd, sizeof(cmd), "dir /b /s \"%s\\*.cm\" >NUL 2>&1", src_dir);
        src_found = (curium_access(src_dir, 0) == 0);
        if (src_found) {
            /* Count via a temp file trick */
            snprintf(cmd, sizeof(cmd),
                "dir /b \"%s\\*.cm\" 2>NUL | find /v \"\" /c > %s\\__curium_count__.tmp 2>NUL",
                src_dir, src_dir);
            system(cmd);
            char tmp_path[600];
            snprintf(tmp_path, sizeof(tmp_path), "%s\\__curium_count__.tmp", src_dir);
            FILE* tf = fopen(tmp_path, "r");
            if (tf) { 
                if (fscanf(tf, "%d", &curium_files) != 1) curium_files = 0; 
                fclose(tf); 
                remove(tmp_path); 
            }
        }
#else
        src_found = (curium_access(src_dir, 0) == 0);
        if (src_found) {
            char count_cmd[2048];
            snprintf(count_cmd, sizeof(count_cmd),
                "find '%s' -name '*.cm' 2>/dev/null | wc -l", src_dir);
            FILE* fp = popen(count_cmd, "r");
            if (fp) { if (fscanf(fp, "%d", &curium_files) != 1) curium_files = 0; pclose(fp); }
        }
#endif
        if (!src_found) {
            DR_FAIL("src/ directory missing — create it and add .cm files");
        } else if (curium_files == 0) {
            DR_WARN("src/ exists but contains no .cm files");
        } else {
            DR_OK("%d .cm source file(s) found in src/", curium_files);
        }
    }

    /* ── Check 3: C compiler ──────────────────────────────────────────── */
    if (curium_doctor_cmd_exists("gcc")) {
        DR_OK("gcc found in PATH");
    } else if (curium_doctor_cmd_exists("clang")) {
        DR_OK("clang found in PATH");
    } else if (curium_doctor_cmd_exists("cc")) {
        DR_OK("cc found in PATH");
    } else {
        DR_FAIL("No C compiler found (gcc/clang/cc) — CM needs one to compile output");
    }

    /* ── Check 4: CM runtime headers ──────────────────────────────────── */
    snprintf(path, sizeof(path), "%s/include/curium/core.h", project_dir);
    if (curium_access(path, 0) == 0) {
        DR_OK("CM runtime headers found (include/curium/core.h)");
    } else {
        DR_WARN("include/curium/core.h not found — make sure the Curium library is in your project");
    }

    /* ── Check 5: Syntax check each .cm file ──────────────────────────── */
    printf("\n  %sSyntax Check%s\n", th.bold, th.reset);
    {
        static const char* const scan_dirs[] = {
            "src", "src/models", "src/services", "src/utils", NULL
        };
        int files_checked = 0;
        int parse_errors  = 0;
        int i;

        for (i = 0; scan_dirs[i]; i++) {
            char dir_path[512];
            snprintf(dir_path, sizeof(dir_path), "%s/%s", project_dir, scan_dirs[i]);

#ifdef _WIN32
            char find_cmd[2048];
            snprintf(find_cmd, sizeof(find_cmd), "dir /b \"%s\\*.cm\" 2>NUL", dir_path);
            FILE* dp = popen(find_cmd, "r");
            if (!dp) continue;
            char fname[512];
            while (fgets(fname, sizeof(fname), dp)) {
                fname[strcspn(fname, "\r\n")] = '\0';
                if (!fname[0]) continue;
                char full[1024];
                snprintf(full, sizeof(full), "%s/%s", dir_path, fname);
#else
            char find_cmd[2048];
            snprintf(find_cmd, sizeof(find_cmd), "find '%s' -maxdepth 1 -name '*.cm' 2>/dev/null", dir_path);
            FILE* dp = popen(find_cmd, "r");
            if (!dp) continue;
            char full[1024];
            while (fgets(full, sizeof(full), dp)) {
                full[strcspn(full, "\r\n")] = '\0';
                if (!full[0]) continue;
#endif
                {
                    size_t src_len = 0;
                    char*  src     = curium_doctor_read_file(full, &src_len);
                    if (!src) {
                        DR_FAIL("Cannot read %s", full);
                        parse_errors++;
                        files_checked++;
                        continue;
                    }

                    if (curium_doctor_has_raw_malloc(src) || curium_doctor_has_raw_free(src)) {
                        DR_WARN("%s — uses raw malloc/free", full);
                    }

                    curium_lexer_t lx;
                    curium_lexer_v2_init(&lx, src);
                    curium_token_t tok;
                    do { tok = curium_lexer_v2_next_token(&lx); } while (tok.kind != CURIUM_TOK_EOF);

                    curium_error_clear();
                    curium_ast_v2_list_t ast = curium_parse_v2(src);
                    const char* err_msg = curium_error_get_message();

                    if (err_msg && err_msg[0] != '\0') {
                        DR_FAIL("%s — parse error: %s", full, err_msg);
                        parse_errors++;
                    } else {
                        DR_OK("%s — OK", full);
                    }
                    curium_ast_v2_free_list(&ast);
                    free(src);
                    files_checked++;
                }
            }
            pclose(dp);
        }

        if (files_checked == 0) {
            DR_WARN("No .cm files found to syntax-check");
        } else if (parse_errors == 0) {
            printf("  %s✨ All %d file(s) parsed successfully%s\n", th.green, files_checked, th.reset);
        }
    }

    /* ── Summary ──────────────────────────────────────────────────────── */
    printf("\n  %s━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━%s\n", th.cyan, th.reset);
    if (issues == 0) {
        printf("  %s%s✅ Project looks healthy! No issues found.%s\n\n", th.bold, th.green, th.reset);
    } else {
        printf("  %s%s❌ Found %d issue(s).%s\n\n", th.bold, th.red, issues, th.reset);
    }

    return issues > 0 ? 1 : 0;
}

static const char* curium_arg_value(int argc, char** argv, const char* flag, const char* fallback) {
    for (int i = 0; i < argc - 1; ++i) {
        if (strcmp(argv[i], flag) == 0) return argv[i + 1];
    }
    return fallback;
}

static int curium_scaffold_new_project(const char* name) {
    if (!name || !name[0]) return 1;
    
    /* Create enterprise folder structure */
    curium_mkdir(name);
    
    char path[512];
    
    /* Core source directories */
    snprintf(path, sizeof(path), "%s/src", name);
    curium_mkdir(path);
    snprintf(path, sizeof(path), "%s/src/models", name);
    curium_mkdir(path);
    snprintf(path, sizeof(path), "%s/src/services", name);
    curium_mkdir(path);
    snprintf(path, sizeof(path), "%s/src/utils", name);
    curium_mkdir(path);
    
    /* Test directory */
    snprintf(path, sizeof(path), "%s/tests", name);
    curium_mkdir(path);
    
    /* Public assets */
    snprintf(path, sizeof(path), "%s/public_html", name);
    curium_mkdir(path);
    snprintf(path, sizeof(path), "%s/public_html/styles", name);
    curium_mkdir(path);
    snprintf(path, sizeof(path), "%s/public_html/scripts", name);
    curium_mkdir(path);
    
    /* Documentation and build output */
    snprintf(path, sizeof(path), "%s/docs", name);
    curium_mkdir(path);
    snprintf(path, sizeof(path), "%s/dist", name);
    curium_mkdir(path);
    snprintf(path, sizeof(path), "%s/.cm", name);
    curium_mkdir(path);
    
    /* Generate curium.json */
    snprintf(path, sizeof(path), "%s/curium.json", name);
    FILE* f = fopen(path, "wb");
    if (f) {
        curium_string_t* curium_json = curium_project_generate_curium_json(name, "A CM project");
        fwrite(curium_json->data, 1, curium_json->length, f);
        curium_string_free(curium_json);
        fclose(f);
    }
    
    /* Generate .gitignore */
    snprintf(path, sizeof(path), "%s/.gitignore", name);
    f = fopen(path, "wb");
    if (f) {
        curium_string_t* gitignore = curium_project_generate_gitignore();
        fwrite(gitignore->data, 1, gitignore->length, f);
        curium_string_free(gitignore);
        fclose(f);
    }
    
    /* Generate README.md */
    snprintf(path, sizeof(path), "%s/README.md", name);
    f = fopen(path, "wb");
    if (f) {
        curium_string_t* readme = curium_project_generate_readme(name);
        fwrite(readme->data, 1, readme->length, f);
        curium_string_free(readme);
        fclose(f);
    }
    
    /* Generate main.cm with v2 syntax */
    snprintf(path, sizeof(path), "%s/src/main.cm", name);
    f = fopen(path, "wb");
    if (f) {
        const char* main_cm =
            "// Curium Project Entry Point\n"
            "\n"
            "fn main() {\n"
            "    print(\"Hello, Curium!\");\n"
            "}\n";
        fwrite(main_cm, 1, strlen(main_cm), f);
        fclose(f);
    }
    
    /* Generate index.html */
    snprintf(path, sizeof(path), "%s/public_html/index.html", name);
    f = fopen(path, "wb");
    if (f) {
        const char* html =
            "<!doctype html>\n"
            "<html>\n"
            "<head>\n"
            "  <meta charset=\"utf-8\" />\n"
            "  <title>";
        fwrite(html, 1, strlen(html), f);
        fwrite(name, 1, strlen(name), f);
        const char* html2 = "</title>\n"
            "  <link rel=\"stylesheet\" href=\"/styles/main.css\" />\n"
            "</head>\n"
            "<body>\n"
            "  <h1>";
        fwrite(html2, 1, strlen(html2), f);
        fwrite(name, 1, strlen(name), f);
        const char* html3 = "</h1>\n"
            "  <div id=\"app\"></div>\n"
            "  <script src=\"/scripts/main.js\"></script>\n"
            "</body>\n"
            "</html>\n";
        fwrite(html3, 1, strlen(html3), f);
        fclose(f);
    }
    
    /* Generate CSS */
    snprintf(path, sizeof(path), "%s/public_html/styles/main.css", name);
    f = fopen(path, "wb");
    if (f) {
        const char* css =
            "body {\n"
            "  font-family: system-ui, -apple-system, sans-serif;\n"
            "  margin: 0;\n"
            "  padding: 40px;\n"
            "  background: #f5f5f5;\n"
            "}\n"
            "h1 { color: #333; }\n";
        fwrite(css, 1, strlen(css), f);
        fclose(f);
    }
    
    /* Generate JS */
    snprintf(path, sizeof(path), "%s/public_html/scripts/main.js", name);
    f = fopen(path, "wb");
    if (f) {
        const char* js =
            "document.addEventListener('DOMContentLoaded', () => {\n"
            "  document.getElementById('app').textContent = 'CM App Ready';\n"
            "});\n";
        fwrite(js, 1, strlen(js), f);
        fclose(f);
    }
    
    /* Print success with Neon styling */
    curium_print_neon_header(name);
    curium_print_neon_progress(100);
    printf("  📁 Created enterprise structure:\n");
    printf("     src/, tests/, docs/, dist/, public_html/\n");
    curium_print_neon_success(name, 0);
    
    return 0;
}

extern int curium_opt_show_stat;

int curium_opt_release = 0;
int curium_opt_debug = 0;

int main(int argc, char** argv) {
    curium_gc_init();
    curium_init_error_detector();

    curium_opt_show_stat = 0;
    
    for (int i = 1; i < argc; ) {
        if (strcmp(argv[i], "--stat") == 0) {
            curium_opt_show_stat = 1;
            for (int j = i; j < argc - 1; j++) argv[j] = argv[j + 1];
            argc--;
        } else if (strcmp(argv[i], "--release") == 0) {
            curium_opt_release = 1;
            for (int j = i; j < argc - 1; j++) argv[j] = argv[j + 1];
            argc--;
        } else if (strcmp(argv[i], "--debug") == 0) {
            curium_opt_debug = 1;
            for (int j = i; j < argc - 1; j++) argv[j] = argv[j + 1];
            argc--;
        } else {
            i++;
        }
    }

    if (argc >= 2 && (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0)) {
        printf("cm(curium) V 4.0\n");
        curium_gc_shutdown();
        return 0;
    }

    if (argc < 2) {
        curium_print_usage();
        curium_gc_shutdown();
        return 1;
    }

    /* Compatibility mode: cm <entry.cm> [output] */
    if (argc >= 2 && argv[1][0] != '-') {
        const char* cmd = argv[1];
        if (strcmp(cmd, "build") != 0 &&
            strcmp(cmd, "run") != 0 &&
            strcmp(cmd, "new") != 0 &&
            strcmp(cmd, "init") != 0 &&
            strcmp(cmd, "doctor") != 0 &&
            strcmp(cmd, "check") != 0 &&
            strcmp(cmd, "test") != 0 &&
            strcmp(cmd, "fmt") != 0 &&
            strcmp(cmd, "install") != 0 &&
            strcmp(cmd, "packages") != 0 &&
            strcmp(cmd, "emitc") != 0 &&
            strcmp(cmd, "highlight") != 0) {
            const char* entry = argv[1];
            const char* out   = (argc >= 3) ? argv[2] : CURIUM_DEFAULT_OUT;
            
            /* Load project config to get name */
            curium_project_config_t config;
            curium_project_load_config(&config, ".");
            const char* project_name = curium_project_get_name(&config);
            
            double start_time = curium_get_time();
            curium_print_neon_header(project_name);
            curium_print_neon_progress(50);
            
            int rc = curium_compile_file(entry, out);
            if (rc != 0) {
                curium_print_neon_error(curium_error_get_message());
                curium_gc_shutdown();
                return rc;
            }
            
            curium_print_neon_progress(100);
            double build_time = curium_get_time() - start_time;
            curium_print_neon_success(out, build_time);
            curium_gc_shutdown();
            return 0;
        }
    }

    const char* sub = argv[1];
    if (strcmp(sub, "init") == 0 || strcmp(sub, "new") == 0) {
        if (argc < 3) { curium_print_usage(); curium_gc_shutdown(); return 1; }
        int rc = curium_scaffold_new_project(argv[2]);
        curium_gc_shutdown();
        return rc;
    }

    if (strcmp(sub, "check") == 0 || strcmp(sub, "doctor") == 0) {
        if (strcmp(sub, "doctor") == 0) {
            const char* project_dir = (argc >= 3) ? argv[2] : ".";
            int rc = curium_doctor(project_dir);
            curium_gc_shutdown();
            return rc;
        }

        /* check subcommand */
        const char* entry = curium_detect_entry_file(argc, argv);
        if (!entry) {
            fprintf(stderr, "Error: No input file specified or found (src/main.cm).\n");
            curium_gc_shutdown();
            return 1;
        }
        
        /* Check if file exists */
        FILE* f = fopen(entry, "r");
        if (!f) {
            fprintf(stderr, "cm: cannot open file '%s'\n", entry);
            curium_gc_shutdown();
            return 1;
        }
        fclose(f);
        
        double start_time = curium_get_time();
        curium_print_neon_header("typecheck");
        curium_print_neon_progress(50);
        
        /* TODO: Run v2 pipeline: lexer_v2 -> parser_v2 -> typecheck only */
        printf("  📄 Checking: %s\n", entry);
        
        size_t src_len = 0;
        char* src = curium_doctor_read_file(entry, &src_len);
        if (!src) {
            printf("  ❌ Cannot read %s\n", entry);
            curium_gc_shutdown();
            return 1;
        }

        curium_error_clear();
        curium_ast_v2_list_t ast = curium_parse_v2(src);
        
        if (curium_error_get_message()[0] != '\0') {
            printf("  ❌ Parse failed\n");
            curium_ast_v2_free_list(&ast);
            free(src);
            curium_gc_shutdown();
            return 1;
        }

        curium_typecheck_ctx_t* tc_ctx = curium_typecheck_new(src, entry);
        int ok = curium_typecheck_module(tc_ctx, &ast);
        
        int errs = curium_typecheck_get_error_count(tc_ctx);
        int warns = curium_typecheck_get_warning_count(tc_ctx);
        
        curium_typecheck_free(tc_ctx);
        curium_ast_v2_free_list(&ast);
        free(src);

        curium_print_neon_progress(100);
        double check_time = curium_get_time() - start_time;
        
        if (errs > 0) {
            printf("\n");
            puts(curium_error_get_message());
        }

        if (!ok) {
            printf("  ❌ Typecheck failed with %d error(s)\n", errs);
            printf("  ⏱️  Check time: %.2fs\n\n", check_time);
            curium_gc_shutdown();
            return 1;
        }

        printf("  ✅ File checked successfully (%d warnings)\n", warns);
        printf("  ⏱️  Check time: %.2fs\n\n", check_time);
        curium_gc_shutdown();
        return 0;
    }

    if (strcmp(sub, "test") == 0) {
        curium_print_neon_header("tests");
        printf("  🧪 Running tests...\n");
        const char* dir = (argc >= 3 && argv[2][0] != '-') ? argv[2] : "tests";
        if (curium_access(dir, 0) != 0) {
            dir = "src"; /* Fallback to src folder */
            if (curium_access(dir, 0) != 0) {
                dir = "."; /* Fallback to current folder */
            }
        }
        int rc = curium_run_tests(dir);
        curium_gc_shutdown();
        return rc;
    }

    if (strcmp(sub, "fmt") == 0) {
        const char* path = (argc >= 3) ? argv[2] : "src/main.cm";
        printf("  📝 Formatting %s...\n", path);
        if (curium_format_file(path) == 0) {
            printf("  ✅ Formatted successfully.\n\n");
        } else {
            printf("  ❌ Failed to format %s\n\n", path);
        }
        curium_gc_shutdown();
        return 0;
    }

    if (strcmp(sub, "install") == 0) {
        const char* out = curium_arg_value(argc, argv, "-o", "a.out");
        printf("  📦 Installing %s...\n", out);
        /* TODO: Copy binary to system path */
        #ifdef _WIN32
        printf("  Windows: Copy to %%LOCALAPPDATA%%\\bin or C:\\Windows\\System32\\\n");
        #else
        printf("  Unix: Copy to /usr/local/bin/ or ~/.local/bin/\n");
        #endif
        printf("  ⚠️  Install command not yet fully implemented\n\n");
        curium_gc_shutdown();
        return 0;
    }

    if (strcmp(sub, "emitc") == 0) {
        const char* entry = curium_detect_entry_file(argc, argv);
        if (!entry) { curium_print_usage(); curium_gc_shutdown(); return 1; }
        const char* outc = curium_arg_value(argc, argv, "-o", "curium_out.c");
        int rc = curium_emit_c_file(entry, outc);
        if (rc != 0) {
            fprintf(stderr, "cm: emitc failed: %s\n", curium_error_get_message());
            curium_gc_shutdown();
            return rc;
        }
        printf("cm: wrote C output to '%s'\n", outc);
        curium_gc_shutdown();
        return 0;
    }

    if (strcmp(sub, "build") == 0 || strcmp(sub, "run") == 0) {
        const char* entry = curium_detect_entry_file(argc, argv);
        if (!entry) { curium_print_usage(); curium_gc_shutdown(); return 1; }
        const char* out = curium_arg_value(argc, argv, "-o", CURIUM_DEFAULT_OUT);

        /* Load project config */
        curium_project_config_t config;
        curium_project_load_config(&config, ".");
        const char* project_name = curium_project_get_name(&config);

        double start_time = curium_get_time();
        curium_print_neon_header(project_name);
        curium_print_neon_progress(50);

        int rc = curium_compile_file(entry, out);
        if (rc != 0) {
            curium_print_neon_error(curium_error_get_message());
            curium_gc_shutdown();
            return rc;
        }
        
        curium_print_neon_progress(100);
        double build_time = curium_get_time() - start_time;
        curium_print_neon_success(out, build_time);

        if (strcmp(sub, "run") == 0) {
            curium_cmd_t* c = curium_cmd_new(out);
            curium_cmd_result_t* r = c ? curium_cmd_run(c) : NULL;
            if (c) curium_cmd_free(c);
            if (r) {
                if (r->stdout_output) printf("%s", r->stdout_output->data);
                if (r->stderr_output && r->stderr_output->length) fprintf(stderr, "%s", r->stderr_output->data);
                rc = r->exit_code;
                curium_cmd_result_free(r);
            }
            curium_gc_shutdown();
            return rc;
        }

        curium_gc_shutdown();
        return 0;
    }

    if (strcmp(sub, "highlight") == 0) {
        if (argc < 3) { curium_print_usage(); curium_gc_shutdown(); return 1; }
        const char* path = argv[2];
        curium_string_t* output = NULL;
        int rc = curium_highlight_file(path, &output);
        if (rc != 0) {
            fprintf(stderr, "cm: highlight failed: could not read file '%s'\n", path);
            curium_gc_shutdown();
            return 1;
        }
        if (output && output->data) {
            printf("%s\n", output->data);
            curium_string_free(output);
        }
        curium_gc_shutdown();
        return 0;
    }

    if (strcmp(sub, "packages") == 0) {
        if (argc < 3) { curium_print_usage(); curium_gc_shutdown(); return 1; }
        const char* pkg_cmd = argv[2];
        int rc = 1;
        
        if (strcmp(pkg_cmd, "init") == 0) {
            const char* name = (argc >= 4) ? argv[3] : NULL;
            rc = curium_packages_cmd_init(name);
        }
        else if (strcmp(pkg_cmd, "install") == 0) {
            const char* pkg_spec = (argc >= 4) ? argv[3] : NULL;
            const char* version = NULL;
            if (pkg_spec) {
                /* Parse name@version format */
                char* at = strchr(pkg_spec, '@');
                if (at) {
                    *at = '\0';
                    version = at + 1;
                }
            }
            rc = curium_packages_cmd_install(pkg_spec, version);
        }
        else if (strcmp(pkg_cmd, "remove") == 0) {
            if (argc < 4) { fprintf(stderr, "Error: Package name required\n"); curium_gc_shutdown(); return 1; }
            rc = curium_packages_cmd_remove(argv[3]);
        }
        else if (strcmp(pkg_cmd, "update") == 0) {
            const char* name = (argc >= 4) ? argv[3] : NULL;
            rc = curium_packages_cmd_update(name);
        }
        else if (strcmp(pkg_cmd, "list") == 0) {
            rc = curium_packages_cmd_list();
        }
        else if (strcmp(pkg_cmd, "search") == 0) {
            if (argc < 4) { fprintf(stderr, "Error: Search query required\n"); curium_gc_shutdown(); return 1; }
            rc = curium_packages_cmd_search(argv[3]);
        }
        else {
            fprintf(stderr, "Unknown packages command: %s\n", pkg_cmd);
            curium_print_usage();
        }
        
        curium_gc_shutdown();
        return rc;
    }

    curium_print_usage();
    curium_gc_shutdown();
    return 1;
}

