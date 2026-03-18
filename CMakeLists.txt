cmake_minimum_required(VERSION 3.10)
project(CMLibrary VERSION 5.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED True)

# Strict compilation flags
if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_C_COMPILER_ID MATCHES "Clang")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Werror -pedantic")
endif()

# Library sources
set(CM_SOURCES
    src/memory.c
    src/error.c
    src/string.c
    src/array.c
    src/map.c
    src/json.c
    src/http.c
    src/file.c
    src/thread.c
    src/cmd.c
    src/core.c
)

# Core library target
add_library(cm STATIC ${CM_SOURCES})
target_include_directories(cm PUBLIC include)

# Platform specific linking
if(WIN32)
    target_link_libraries(cm ws2_32)
endif()

# Tests
enable_testing()

add_executable(cm_test tests/main_test.c)
target_link_libraries(cm_test cm)

add_test(NAME CM_CoreTest COMMAND cm_test)

# Backend server example
add_executable(cm_server server.c)
target_link_libraries(cm_server cm)

if(WIN32)
    target_link_libraries(cm_server ws2_32)
endif()

# Copy public_html next to the server executable so relative paths work
add_custom_command(TARGET cm_server POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/public_html"
        "$<TARGET_FILE_DIR:cm_server>/public_html"
    COMMENT "Copying public_html to build directory..."
)
