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

    # Collect example YAML files for file-argument tests
    file(GLOB YAML_EXAMPLES "${LIBYAML_EXAMPLES_DIR}/*.yaml")

    # Tests that take no arguments
    add_test(NAME conformance-test-version COMMAND conformance-test-version)
    add_test(NAME conformance-test-reader COMMAND conformance-test-reader)

    # Tests that take YAML files as arguments
    add_test(NAME conformance-run-scanner COMMAND conformance-run-scanner ${YAML_EXAMPLES})
    add_test(NAME conformance-run-parser COMMAND conformance-run-parser ${YAML_EXAMPLES})
    add_test(NAME conformance-run-loader COMMAND conformance-run-loader ${YAML_EXAMPLES})
    add_test(NAME conformance-run-emitter COMMAND conformance-run-emitter ${YAML_EXAMPLES})
    add_test(NAME conformance-run-dumper COMMAND conformance-run-dumper ${YAML_EXAMPLES})
endif()
