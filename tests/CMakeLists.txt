# Unit test framework - we use a simple single-header approach
# Tests link against the static library

# Test version API
add_executable(test-version test_version.c)
target_link_libraries(test-version qyaml)
target_compile_definitions(test-version PRIVATE YAML_DECLARE_STATIC)
add_test(NAME version COMMAND test-version)

# Test reader
add_executable(test-reader test_reader.c)
target_link_libraries(test-reader qyaml)
target_compile_definitions(test-reader PRIVATE YAML_DECLARE_STATIC)
add_test(NAME reader COMMAND test-reader)

# Scanner test
add_executable(test-scanner test_scanner.c)
target_link_libraries(test-scanner qyaml)
target_compile_definitions(test-scanner PRIVATE YAML_DECLARE_STATIC)
add_test(NAME scanner COMMAND test-scanner)

# Parser test
add_executable(test-parser test_parser.c)
target_link_libraries(test-parser qyaml)
target_compile_definitions(test-parser PRIVATE YAML_DECLARE_STATIC)
add_test(NAME parser COMMAND test-parser)

# Event API test
add_executable(test-event-api test_event_api.c)
target_link_libraries(test-event-api qyaml)
target_compile_definitions(test-event-api PRIVATE YAML_DECLARE_STATIC)
add_test(NAME event-api COMMAND test-event-api)

# Document API test
add_executable(test-document-api test_document_api.c)
target_link_libraries(test-document-api qyaml)
target_compile_definitions(test-document-api PRIVATE YAML_DECLARE_STATIC)
add_test(NAME document-api COMMAND test-document-api)

# Emitter test
add_executable(test-emitter test_emitter.c)
target_link_libraries(test-emitter qyaml)
target_compile_definitions(test-emitter PRIVATE YAML_DECLARE_STATIC)
add_test(NAME emitter COMMAND test-emitter)

# Comprehensive scalar test
add_executable(test-scalars test_scalars.c)
target_link_libraries(test-scalars qyaml)
target_compile_definitions(test-scalars PRIVATE YAML_DECLARE_STATIC)
add_test(NAME scalars COMMAND test-scalars)

# Edge cases test
add_executable(test-edge-cases test_edge_cases.c)
target_link_libraries(test-edge-cases qyaml)
target_compile_definitions(test-edge-cases PRIVATE YAML_DECLARE_STATIC)
add_test(NAME edge-cases COMMAND test-edge-cases)

# Differential test
add_executable(test-differential test_differential.c)
target_link_libraries(test-differential qyaml)
target_compile_definitions(test-differential PRIVATE YAML_DECLARE_STATIC)
add_test(NAME differential COMMAND test-differential)

# Error handling test
add_executable(test-errors test_errors.c)
target_link_libraries(test-errors qyaml)
target_compile_definitions(test-errors PRIVATE YAML_DECLARE_STATIC)
add_test(NAME errors COMMAND test-errors)

# Stress test
add_executable(test-stress test_stress.c)
target_link_libraries(test-stress qyaml)
target_compile_definitions(test-stress PRIVATE YAML_DECLARE_STATIC)
add_test(NAME stress COMMAND test-stress)
