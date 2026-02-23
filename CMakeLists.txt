cmake_minimum_required(VERSION 3.14)
project(libqyaml VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Version numbers matching libyaml for API compatibility
set(YAML_VERSION_MAJOR 0)
set(YAML_VERSION_MINOR 2)
set(YAML_VERSION_PATCH 5)
set(YAML_VERSION_STRING "${YAML_VERSION_MAJOR}.${YAML_VERSION_MINOR}.${YAML_VERSION_PATCH}")

# Generate config.h
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/config.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/include/config.h
)

option(QYAML_BUILD_TESTS "Build tests" ON)
option(QYAML_BUILD_BENCHMARKS "Build benchmarks" OFF)
option(QYAML_COVERAGE "Build with code coverage instrumentation" OFF)
set(QYAML_PGO "" CACHE STRING "Profile-Guided Optimization mode: generate, use, or path to profile data dir")

# Library sources
set(QYAML_SOURCES
    src/api.c
    src/reader.c
    src/scanner.c
    src/parser.c
    src/loader.c
    src/emitter.c
    src/dumper.c
    src/writer.c
)

# Static library (primary target)
add_library(qyaml STATIC ${QYAML_SOURCES})
target_include_directories(qyaml
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_compile_definitions(qyaml PRIVATE YAML_DECLARE_STATIC HAVE_CONFIG_H)
target_include_directories(qyaml PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/include)

# Shared library
add_library(qyaml_shared SHARED ${QYAML_SOURCES})
set_target_properties(qyaml_shared PROPERTIES OUTPUT_NAME qyaml)
target_include_directories(qyaml_shared
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_compile_definitions(qyaml_shared PRIVATE YAML_DECLARE_EXPORT HAVE_CONFIG_H)
target_include_directories(qyaml_shared PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/include)

# Compiler warnings
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(qyaml PRIVATE -Wall -Wextra -pedantic)
    target_compile_options(qyaml_shared PRIVATE -Wall -Wextra -pedantic)
endif()

# Code coverage instrumentation
if(QYAML_COVERAGE)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(qyaml PRIVATE --coverage -fprofile-arcs -ftest-coverage)
        target_link_options(qyaml PRIVATE --coverage)
    endif()
endif()

# Profile-Guided Optimization
if(QYAML_PGO)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        if(QYAML_PGO STREQUAL "generate")
            set(_pgo_dir "${CMAKE_BINARY_DIR}/pgo-profdata")
            file(MAKE_DIRECTORY "${_pgo_dir}")
            if(CMAKE_C_COMPILER_ID MATCHES "Clang")
                set(_pgo_gen_flags -fprofile-generate="${_pgo_dir}")
            else()
                set(_pgo_gen_flags -fprofile-generate -fprofile-dir=${_pgo_dir})
            endif()
            target_compile_options(qyaml PRIVATE ${_pgo_gen_flags})
            target_compile_options(qyaml_shared PRIVATE ${_pgo_gen_flags})
            # PUBLIC link options so executables linking qyaml get the profiling runtime
            target_link_options(qyaml PUBLIC ${_pgo_gen_flags})
            target_link_options(qyaml_shared PUBLIC ${_pgo_gen_flags})
            message(STATUS "PGO: instrumented build (profile data -> ${_pgo_dir})")
        elseif(QYAML_PGO STREQUAL "use")
            set(_pgo_dir "${CMAKE_BINARY_DIR}/pgo-profdata")
            if(CMAKE_C_COMPILER_ID MATCHES "Clang")
                target_compile_options(qyaml PRIVATE -fprofile-use="${_pgo_dir}")
                target_compile_options(qyaml_shared PRIVATE -fprofile-use="${_pgo_dir}")
            else()
                target_compile_options(qyaml PRIVATE -fprofile-use -fprofile-dir=${_pgo_dir} -fprofile-correction)
                target_compile_options(qyaml_shared PRIVATE -fprofile-use -fprofile-dir=${_pgo_dir} -fprofile-correction)
            endif()
            message(STATUS "PGO: optimized build (profile data <- ${_pgo_dir})")
        else()
            # Treat as path to profile data directory
            if(CMAKE_C_COMPILER_ID MATCHES "Clang")
                target_compile_options(qyaml PRIVATE -fprofile-use="${QYAML_PGO}")
                target_compile_options(qyaml_shared PRIVATE -fprofile-use="${QYAML_PGO}")
            else()
                target_compile_options(qyaml PRIVATE -fprofile-use -fprofile-dir=${QYAML_PGO} -fprofile-correction)
                target_compile_options(qyaml_shared PRIVATE -fprofile-use -fprofile-dir=${QYAML_PGO} -fprofile-correction)
            endif()
            message(STATUS "PGO: optimized build (profile data <- ${QYAML_PGO})")
        endif()
    else()
        message(WARNING "PGO is only supported with GCC and Clang")
    endif()
endif()

# Also provide a yaml alias target for drop-in replacement
add_library(yaml ALIAS qyaml)

if(QYAML_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(QYAML_BUILD_BENCHMARKS)
    add_subdirectory(bench)
endif()
