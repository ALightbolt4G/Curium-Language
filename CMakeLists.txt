cmake_minimum_required(VERSION 3.10)
project(Curium VERSION 5.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED True)

# Strict compilation flags
if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_C_COMPILER_ID MATCHES "Clang")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Werror -pedantic")
endif()

# Library sources
set(CURIUM_RUNTIME_SOURCES
    src/runtime/memory.c
    src/runtime/error.c
    src/runtime/error_detail.c
    src/runtime/error_obj.c
    src/runtime/debug.c
    src/runtime/project.c
    src/runtime/string.c
    src/runtime/array.c
    src/runtime/map.c
    src/runtime/json.c
    src/runtime/http.c
    src/runtime/file.c
    src/runtime/thread.c
    src/runtime/cmd.c
    src/runtime/core.c
    src/runtime/list.c
    src/runtime/dynarray.c
    src/runtime/packages.c
    src/runtime/backend.c
    src/runtime/option.c
    src/runtime/result.c
    src/runtime/safe_ptr.c
    src/runtime/oop.c
)

# Core library target (static lib)
add_library(cm STATIC ${CURIUM_RUNTIME_SOURCES})
target_include_directories(cm PUBLIC include)

# Platform specific linking
if(WIN32)
    target_link_libraries(cm ws2_32)
endif()

# Tests
enable_testing()

add_executable(curium_test tests/main_test.c)
target_link_libraries(curium_test cm)

add_test(NAME CURIUM_CoreTest COMMAND curium_test)

# Compiler frontend/codegen smoke test
add_executable(curium_compiler_test tests/curium_compiler_test.c)
target_link_libraries(curium_compiler_test curium_compiler cm)
add_test(NAME CURIUM_CompilerTest COMMAND curium_compiler_test)
set_tests_properties(CURIUM_CompilerTest PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# Backend server example
add_executable(curium_server server.c)
target_link_libraries(curium_server cm)

if(WIN32)
    target_link_libraries(curium_server ws2_32)
endif()

# CM Language Compiler CLI
# Use an internal target name to avoid clashing with the 'cm' static library.
# Compiler sources
set(CURIUM_COMPILER_SOURCES
    src/compiler/lexer/lexer.c
    src/compiler/lexer/lexer_v2.c
    src/compiler/ast/ast.c
    src/compiler/ast/ast_v2.c
    src/compiler/arena.c
    src/compiler/parser/parser.c
    src/compiler/parser/parser_v2.c
    src/compiler/codegen/codegen_v2.c
    src/compiler/tokens/tokens.c
    src/compiler/typecheck.c
    src/curium_codegen.c
    src/curium_highlight.c
    src/compiler/safety/curium_safety.c
)
# Compiler library
add_library(curium_compiler STATIC ${CURIUM_COMPILER_SOURCES})
target_link_libraries(curium_compiler cm)
target_include_directories(curium_compiler PUBLIC include src)

# Release: optimize the compiler itself for faster builds of user code
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(curium_compiler PRIVATE $<$<CONFIG:Release>:-O3>)
elseif(MSVC)
    target_compile_options(curium_compiler PRIVATE $<$<CONFIG:Release>:/O2>)
endif()

# CLI executable
add_executable(curium_cli src/curium_cli.c)
target_link_libraries(curium_cli curium_compiler cm)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(curium_cli PRIVATE $<$<CONFIG:Release>:-O3>)
elseif(MSVC)
    target_compile_options(curium_cli PRIVATE $<$<CONFIG:Release>:/O2>)
endif()

# Public CLI name is `cm` (static library target remains `cm` as libcm.a / cm.lib)
set_target_properties(curium_cli PROPERTIES RUNTIME_OUTPUT_NAME "cm")

# Package Manager CLI executable
add_executable(cur_cli src/cur_cli.c)
target_link_libraries(cur_cli cm)
set_target_properties(cur_cli PROPERTIES RUNTIME_OUTPUT_NAME "cur")

# Copy disabled - file is locked by running process
# add_custom_command(TARGET curium_cli POST_BUILD
#     COMMAND ${CMAKE_COMMAND} -E copy
#         "$<TARGET_FILE:curium_cli>"
#         "${CMAKE_SOURCE_DIR}/$<TARGET_FILE_NAME:curium_cli>"
#     COMMENT "Copying cm compiler to project root..."
# )

# Copy public_html next to the server executable so relative paths work
add_custom_command(TARGET curium_server POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/public_html"
        "$<TARGET_FILE_DIR:curium_server>/public_html"
    COMMENT "Copying public_html to build directory..."
)
