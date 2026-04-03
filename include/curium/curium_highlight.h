/**
 * @file curium_highlight.h
 * @brief CM Language syntax highlighting API.
 *
 * Provides token-based syntax highlighting for CM source code.
 * Uses the same lexer as the compiler for consistency.
 * Supports ANSI color output and is designed for future LSP integration.
 */
#ifndef CURIUM_HIGHLIGHT_H
#define CURIUM_HIGHLIGHT_H

#include "curium/curium_lang.h"
#include "curium/string.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief ANSI color codes for syntax highlighting.
 */
typedef enum {
    CURIUM_COLOR_DEFAULT = 0,
    CURIUM_COLOR_KEYWORD,     /* Blue */
    CURIUM_COLOR_STRING,      /* Green */
    CURIUM_COLOR_NUMBER,      /* Yellow */
    CURIUM_COLOR_FUNCTION,    /* Cyan */
    CURIUM_COLOR_METHOD,      /* Magenta */
    CURIUM_COLOR_COMMENT,     /* Gray */
    CURIUM_COLOR_OPERATOR,    /* White/Default */
    CURIUM_COLOR_ERROR        /* Red */
} curium_color_t;

/**
 * @brief A highlighted token with its assigned color.
 */
typedef struct {
    curium_token_t token;
    curium_color_t color;
} curium_highlight_token_t;

/**
 * @brief Result of tokenizing a source file for highlighting.
 */
typedef struct {
    curium_highlight_token_t* tokens;
    size_t count;
    size_t capacity;
    int has_error;
    size_t error_line;
    size_t error_column;
    char* error_message;
} curium_highlight_result_t;

/**
 * @brief Highlight a source file and return colorized output.
 *
 * @param source The CM source code to highlight.
 * @param output Pointer to store the colorized output string.
 * @return 0 on success, non-zero on error.
 */
int curium_highlight_source(const char* source, curium_string_t** output);

/**
 * @brief Highlight a file and return colorized output.
 *
 * @param path Path to the .curium file.
 * @param output Pointer to store the colorized output string.
 * @return 0 on success, non-zero on error.
 */
int curium_highlight_file(const char* path, curium_string_t** output);

/**
 * @brief Tokenize source code and classify tokens for highlighting.
 *
 * This is the core API that can be used by LSP, CLI, or other tools.
 *
 * @param source The CM source code to tokenize.
 * @return Highlight result containing classified tokens. Caller must free with curium_highlight_free().
 */
curium_highlight_result_t* curium_tokenize_for_highlight(const char* source);

/**
 * @brief Free a highlight result.
 *
 * @param result The result to free.
 */
void curium_highlight_free(curium_highlight_result_t* result);

/**
 * @brief Get the ANSI color code for a color type.
 *
 * @param color The color type.
 * @param use_color Whether to use ANSI colors (0 for plain text).
 * @return ANSI escape sequence string or empty string.
 */
const char* curium_color_ansi(curium_color_t color, int use_color);

/**
 * @brief Reset ANSI color to default.
 *
 * @param use_color Whether to use ANSI colors.
 * @return ANSI reset sequence or empty string.
 */
const char* curium_color_reset(int use_color);

/**
 * @brief Classify a token kind into a color.
 *
 * @param kind The token kind.
 * @return The corresponding color.
 */
curium_color_t curium_token_to_color(curium_token_kind_t kind);

/**
 * @brief Print error indicator with pointer.
 *
 * @param source The original source code.
 * @param line The line number where error occurred.
 * @param column The column number where error occurred.
 * @param message Error message.
 */
void curium_highlight_print_error(const char* source, size_t line, size_t column, const char* message);

#ifdef __cplusplus
}
#endif

#endif /* CURIUM_HIGHLIGHT_H */
