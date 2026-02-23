# Unit test framework - we use a simple single-header approach
# Tests link against the static library

# Helper function to add a test with standard settings
function(qyaml_add_test TEST_NAME TEST_SOURCE)
    add_executable(test-${TEST_NAME} ${TEST_SOURCE})
    target_link_libraries(test-${TEST_NAME} qyaml)
    target_compile_definitions(test-${TEST_NAME} PRIVATE YAML_DECLARE_STATIC)
    if(QYAML_COVERAGE)
        target_link_options(test-${TEST_NAME} PRIVATE --coverage)
    endif()
    add_test(NAME ${TEST_NAME} COMMAND test-${TEST_NAME})
endfunction()

qyaml_add_test(version test_version.c)
qyaml_add_test(reader test_reader.c)
qyaml_add_test(scanner test_scanner.c)
qyaml_add_test(parser test_parser.c)
qyaml_add_test(event-api test_event_api.c)
qyaml_add_test(document-api test_document_api.c)
qyaml_add_test(emitter test_emitter.c)
qyaml_add_test(scalars test_scalars.c)
qyaml_add_test(edge-cases test_edge_cases.c)
qyaml_add_test(differential test_differential.c)
qyaml_add_test(errors test_errors.c)
qyaml_add_test(stress test_stress.c)
qyaml_add_test(writer test_writer.c)
qyaml_add_test(roundtrip test_roundtrip.c)
qyaml_add_test(scanner-comprehensive test_scanner_comprehensive.c)
qyaml_add_test(parser-comprehensive test_parser_comprehensive.c)
qyaml_add_test(api-comprehensive test_api_comprehensive.c)
qyaml_add_test(reader-comprehensive test_reader_comprehensive.c)
qyaml_add_test(errors-comprehensive test_errors_comprehensive.c)
qyaml_add_test(emitter-comprehensive test_emitter_comprehensive.c)
qyaml_add_test(loader-comprehensive test_loader_comprehensive.c)
qyaml_add_test(truncation test_truncation.c)
qyaml_add_test(deep-nesting test_deep_nesting.c)

# OOM injection test: requires --wrap linker flag (Linux/GNU ld)
# Tests that all allocation failure paths return errors cleanly
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_executable(test-oom test_oom.c)
    target_link_libraries(test-oom qyaml)
    target_compile_definitions(test-oom PRIVATE YAML_DECLARE_STATIC)
    target_compile_options(test-oom PRIVATE -O0)
    target_link_options(test-oom PRIVATE
        -Wl,--wrap=malloc -Wl,--wrap=realloc -Wl,--wrap=calloc)
    if(QYAML_COVERAGE)
        target_link_options(test-oom PRIVATE --coverage)
    endif()
    add_test(NAME oom COMMAND test-oom)
endif()

# True differential test: compare against system libyaml via dlopen
add_executable(test-differential-libyaml test_differential_libyaml.c)
target_link_libraries(test-differential-libyaml qyaml ${CMAKE_DL_LIBS})
target_compile_definitions(test-differential-libyaml PRIVATE YAML_DECLARE_STATIC)
if(QYAML_COVERAGE)
    target_link_options(test-differential-libyaml PRIVATE --coverage)
endif()
add_test(NAME differential-libyaml COMMAND test-differential-libyaml)

# libyaml conformance tests -- built from reference libyaml test sources
# but linked against libqyaml to verify API compatibility

set(LIBYAML_TESTS_DIR "${CMAKE_SOURCE_DIR}/reference/libyaml/tests")
set(LIBYAML_EXAMPLES_DIR "${CMAKE_SOURCE_DIR}/reference/libyaml/examples")

if(EXISTS "${LIBYAML_TESTS_DIR}")
    # Helper to build a conformance test from libyaml sources
    function(qyaml_add_conformance_test TEST_NAME TEST_SOURCE)
        add_executable(conformance-${TEST_NAME} ${LIBYAML_TESTS_DIR}/${TEST_SOURCE})
        target_link_libraries(conformance-${TEST_NAME} qyaml)
        target_compile_definitions(conformance-${TEST_NAME} PRIVATE YAML_DECLARE_STATIC)
        if(QYAML_COVERAGE)
            target_link_options(conformance-${TEST_NAME} PRIVATE --coverage)
        endif()
    endfunction()

    # Build conformance test binaries
    qyaml_add_conformance_test(test-version test-version.c)
    qyaml_add_conformance_test(test-reader test-reader.c)
    qyaml_add_conformance_test(run-scanner run-scanner.c)
    qyaml_add_conformance_test(run-parser run-parser.c)
    qyaml_add_conformance_test(run-loader run-loader.c)
    qyaml_add_conformance_test(run-emitter run-emitter.c)
    qyaml_add_conformance_test(run-dumper run-dumper.c)

    # YAML spec test suite tools
    if(EXISTS "${LIBYAML_TESTS_DIR}/run-parser-test-suite.c")
        qyaml_add_conformance_test(run-parser-test-suite run-parser-test-suite.c)
    endif()
    if(EXISTS "${LIBYAML_TESTS_DIR}/run-emitter-test-suite.c")
        qyaml_add_conformance_test(run-emitter-test-suite run-emitter-test-suite.c)
    endif()

    # Collect example YAML files for file-argument tests
    file(GLOB YAML_EXAMPLES "${LIBYAML_EXAMPLES_DIR}/*.yaml")

    # Tests that take no arguments
    add_test(NAME conformance-test-version COMMAND conformance-test-version)
    add_test(NAME conformance-test-reader COMMAND conformance-test-reader)

    # YAML spec test suite (yaml-test-suite project)
    set(YAML_TEST_DATA "${CMAKE_SOURCE_DIR}/reference/yaml-test-data")
    set(YAML_TEST_KNOWN_FAILURES "${CMAKE_SOURCE_DIR}/tests/yaml-test-suite-known-failures.txt")
    if(EXISTS "${YAML_TEST_DATA}" AND EXISTS "${LIBYAML_TESTS_DIR}/run-parser-test-suite.c")
        add_test(NAME yaml-test-suite-parser
            COMMAND ${CMAKE_SOURCE_DIR}/tests/run_yaml_test_suite.sh
                $<TARGET_FILE:conformance-run-parser-test-suite>
                ${YAML_TEST_DATA}
                ${YAML_TEST_KNOWN_FAILURES})
    endif()

    # Tests that take YAML files as arguments
    add_test(NAME conformance-run-scanner COMMAND conformance-run-scanner ${YAML_EXAMPLES})
    add_test(NAME conformance-run-parser COMMAND conformance-run-parser ${YAML_EXAMPLES})
    add_test(NAME conformance-run-loader COMMAND conformance-run-loader ${YAML_EXAMPLES})
    add_test(NAME conformance-run-emitter COMMAND conformance-run-emitter ${YAML_EXAMPLES})
    add_test(NAME conformance-run-dumper COMMAND conformance-run-dumper ${YAML_EXAMPLES})
endif()
