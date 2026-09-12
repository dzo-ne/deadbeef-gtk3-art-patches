cmake_minimum_required(VERSION 3.10)
project(ddb_custom_medialib C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Find GTK3
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)

# Locate DeaDBeeF headers
find_path(DEADBEEF_INCLUDE_DIR
    NAMES deadbeef/deadbeef.h
    PATHS
        ${DEADBEEF_INCLUDE_DIR}
        /opt/deadbeef/include
        /usr/local/include
        /usr/include
        $ENV{DEADBEEF_ROOT}/include
)

if(NOT DEADBEEF_INCLUDE_DIR)
    message(FATAL_ERROR "DeaDBeeF development headers (deadbeef/deadbeef.h) not found. Specify -DDEADBEEF_INCLUDE_DIR=/path/to/include")
endif()

message(STATUS "Found DeaDBeeF headers at: ${DEADBEEF_INCLUDE_DIR}")
message(STATUS "Found GTK3 version: ${GTK3_VERSION}")

set(SOURCES
    src/plugin.c
    src/artwork_cache.c
    src/medialib_tree.c
    src/ui.c
)

# Output module without 'lib' prefix to match DeaDBeeF plugin convention: ddb_custom_medialib.so
add_library(ddb_custom_medialib MODULE ${SOURCES})

set_target_properties(ddb_custom_medialib PROPERTIES
    PREFIX ""
    OUTPUT_NAME "ddb_custom_medialib"
)

target_include_directories(ddb_custom_medialib PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${DEADBEEF_INCLUDE_DIR}
    ${GTK3_INCLUDE_DIRS}
)

target_compile_options(ddb_custom_medialib PRIVATE
    -Wall
    -Wextra
    -O2
    -fPIC
    -D_GNU_SOURCE
    ${GTK3_CFLAGS_OTHER}
)

target_link_directories(ddb_custom_medialib PRIVATE
    ${GTK3_LIBRARY_DIRS}
)

target_link_libraries(ddb_custom_medialib PRIVATE
    ${GTK3_LIBRARIES}
    -lm
)

# Standard install to CMAKE_INSTALL_PREFIX/lib/deadbeef
install(TARGETS ddb_custom_medialib DESTINATION lib/deadbeef)

# Convenience target: make install-user
add_custom_target(install-user
    COMMAND ${CMAKE_COMMAND} -E make_directory "$ENV{HOME}/.local/lib/deadbeef"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:ddb_custom_medialib>" "$ENV{HOME}/.local/lib/deadbeef/"
    DEPENDS ddb_custom_medialib
    COMMENT "Installing ddb_custom_medialib.so into ~/.local/lib/deadbeef/"
)

# Automated testing
enable_testing()
add_executable(test_plugin tests/test_plugin.c)
target_include_directories(test_plugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${DEADBEEF_INCLUDE_DIR}
    ${GTK3_INCLUDE_DIRS}
)
target_link_libraries(test_plugin PRIVATE
    ${GTK3_LIBRARIES}
    ${CMAKE_DL_LIBS}
)
add_test(NAME PluginVerification COMMAND test_plugin $<TARGET_FILE:ddb_custom_medialib>)
