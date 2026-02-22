#include "test_helper.h"

/* Test emitter initialization and deletion */
static void test_emitter_init(void) {
    yaml_emitter_t emitter;

    ASSERT(yaml_emitter_initialize(&emitter), "emitter init");
    yaml_emitter_delete(&emitter);

    /* Re-initialize should work */
    ASSERT(yaml_emitter_initialize(&emitter), "emitter re-init");
    yaml_emitter_delete(&emitter);
}

/* Test emitter configuration */
static void test_emitter_config(void) {
    yaml_emitter_t emitter;
    ASSERT(yaml_emitter_initialize(&emitter), "emitter config init");

    yaml_emitter_set_encoding(&emitter, YAML_UTF8_ENCODING);
    yaml_emitter_set_canonical(&emitter, 1);
    yaml_emitter_set_indent(&emitter, 4);
    yaml_emitter_set_width(&emitter, 80);
    yaml_emitter_set_unicode(&emitter, 1);
    yaml_emitter_set_break(&emitter, YAML_LN_BREAK);

    /* These should not crash */
    ASSERT(1, "emitter config: no crash");

    yaml_emitter_delete(&emitter);
}

/* Test string output */
static void test_string_output(void) {
    yaml_emitter_t emitter;
    yaml_event_t event;
    unsigned char output[4096];
    size_t written = 0;

    ASSERT(yaml_emitter_initialize(&emitter), "string output init");
    yaml_emitter_set_output_string(&emitter, output, sizeof(output), &written);
    yaml_emitter_set_encoding(&emitter, YAML_UTF8_ENCODING);

    /* Emit a minimal stream: stream-start, doc-start, scalar, doc-end, stream-end */
    yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
    ASSERT(yaml_emitter_emit(&emitter, &event), "emit stream start");

    yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1);
    ASSERT(yaml_emitter_emit(&emitter, &event), "emit doc start");

    yaml_scalar_event_initialize(&event, NULL, (const yaml_char_t *)YAML_STR_TAG,
        (const yaml_char_t *)"hello", 5, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    ASSERT(yaml_emitter_emit(&emitter, &event), "emit scalar");

    yaml_document_end_event_initialize(&event, 1);
    ASSERT(yaml_emitter_emit(&emitter, &event), "emit doc end");

    yaml_stream_end_event_initialize(&event);
    ASSERT(yaml_emitter_emit(&emitter, &event), "emit stream end");

    ASSERT(written > 0, "string output: bytes written > 0");
    /* Output should contain "hello" */
    output[written] = '\0';
    ASSERT(strstr((const char *)output, "hello") != NULL,
           "string output: contains hello");

    yaml_emitter_delete(&emitter);
}

/* Test file output */
static void test_file_output(void) {
    yaml_emitter_t emitter;
    yaml_event_t event;

    FILE *tmpf = tmpfile();
    ASSERT_NOT_NULL(tmpf, "file output: create tmp");
    if (!tmpf) return;

    ASSERT(yaml_emitter_initialize(&emitter), "file output init");
    yaml_emitter_set_output_file(&emitter, tmpf);

    yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
    ASSERT(yaml_emitter_emit(&emitter, &event), "file: stream start");

    yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1);
    ASSERT(yaml_emitter_emit(&emitter, &event), "file: doc start");

    yaml_scalar_event_initialize(&event, NULL, (const yaml_char_t *)YAML_STR_TAG,
        (const yaml_char_t *)"world", 5, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    ASSERT(yaml_emitter_emit(&emitter, &event), "file: scalar");

    yaml_document_end_event_initialize(&event, 1);
    ASSERT(yaml_emitter_emit(&emitter, &event), "file: doc end");

    yaml_stream_end_event_initialize(&event);
    ASSERT(yaml_emitter_emit(&emitter, &event), "file: stream end");

    yaml_emitter_delete(&emitter);

    /* Read back and verify */
    rewind(tmpf);
    char buf[4096];
    size_t n = fread(buf, 1, sizeof(buf) - 1, tmpf);
    buf[n] = '\0';
    ASSERT(n > 0, "file output: bytes written");
    ASSERT(strstr(buf, "world") != NULL, "file output: contains world");

    fclose(tmpf);
}

/* Test emit mapping */
static void test_emit_mapping(void) {
    yaml_emitter_t emitter;
    yaml_event_t event;
    unsigned char output[4096];
    size_t written = 0;

    ASSERT(yaml_emitter_initialize(&emitter), "emit map init");
    yaml_emitter_set_output_string(&emitter, output, sizeof(output), &written);

    yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
    yaml_emitter_emit(&emitter, &event);

    yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1);
    yaml_emitter_emit(&emitter, &event);

    yaml_mapping_start_event_initialize(&event, NULL,
        (const yaml_char_t *)YAML_MAP_TAG, 1, YAML_BLOCK_MAPPING_STYLE);
    yaml_emitter_emit(&emitter, &event);

    yaml_scalar_event_initialize(&event, NULL, (const yaml_char_t *)YAML_STR_TAG,
        (const yaml_char_t *)"key", 3, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);

    yaml_scalar_event_initialize(&event, NULL, (const yaml_char_t *)YAML_STR_TAG,
        (const yaml_char_t *)"value", 5, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);

    yaml_mapping_end_event_initialize(&event);
    yaml_emitter_emit(&emitter, &event);

    yaml_document_end_event_initialize(&event, 1);
    yaml_emitter_emit(&emitter, &event);

    yaml_stream_end_event_initialize(&event);
    yaml_emitter_emit(&emitter, &event);

    output[written] = '\0';
    ASSERT(strstr((const char *)output, "key") != NULL, "emit map: has key");
    ASSERT(strstr((const char *)output, "value") != NULL, "emit map: has value");

    yaml_emitter_delete(&emitter);
}

/* Test emit sequence */
static void test_emit_sequence(void) {
    yaml_emitter_t emitter;
    yaml_event_t event;
    unsigned char output[4096];
    size_t written = 0;

    ASSERT(yaml_emitter_initialize(&emitter), "emit seq init");
    yaml_emitter_set_output_string(&emitter, output, sizeof(output), &written);

    yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
    yaml_emitter_emit(&emitter, &event);

    yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1);
    yaml_emitter_emit(&emitter, &event);

    yaml_sequence_start_event_initialize(&event, NULL,
        (const yaml_char_t *)YAML_SEQ_TAG, 1, YAML_BLOCK_SEQUENCE_STYLE);
    yaml_emitter_emit(&emitter, &event);

    yaml_scalar_event_initialize(&event, NULL, (const yaml_char_t *)YAML_STR_TAG,
        (const yaml_char_t *)"item1", 5, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);

    yaml_scalar_event_initialize(&event, NULL, (const yaml_char_t *)YAML_STR_TAG,
        (const yaml_char_t *)"item2", 5, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);

    yaml_sequence_end_event_initialize(&event);
    yaml_emitter_emit(&emitter, &event);

    yaml_document_end_event_initialize(&event, 1);
    yaml_emitter_emit(&emitter, &event);

    yaml_stream_end_event_initialize(&event);
    yaml_emitter_emit(&emitter, &event);

    output[written] = '\0';
    ASSERT(strstr((const char *)output, "item1") != NULL, "emit seq: has item1");
    ASSERT(strstr((const char *)output, "item2") != NULL, "emit seq: has item2");

    yaml_emitter_delete(&emitter);
}

/* Test dumper API */
static void test_dump(void) {
    yaml_emitter_t emitter;
    yaml_document_t doc;
    unsigned char output[4096];
    size_t written = 0;

    ASSERT(yaml_emitter_initialize(&emitter), "dump init emitter");
    yaml_emitter_set_output_string(&emitter, output, sizeof(output), &written);

    ASSERT(yaml_emitter_open(&emitter), "dump open");

    ASSERT(yaml_document_initialize(&doc, NULL, NULL, NULL, 1, 1), "dump init doc");
    int map = yaml_document_add_mapping(&doc, (const yaml_char_t *)YAML_MAP_TAG,
              YAML_BLOCK_MAPPING_STYLE);
    int k = yaml_document_add_scalar(&doc, (const yaml_char_t *)YAML_STR_TAG,
             (const yaml_char_t *)"name", 4, YAML_PLAIN_SCALAR_STYLE);
    int v = yaml_document_add_scalar(&doc, (const yaml_char_t *)YAML_STR_TAG,
             (const yaml_char_t *)"test", 4, YAML_PLAIN_SCALAR_STYLE);
    yaml_document_append_mapping_pair(&doc, map, k, v);

    ASSERT(yaml_emitter_dump(&emitter, &doc), "dump doc");

    ASSERT(yaml_emitter_close(&emitter), "dump close");

    output[written] = '\0';
    ASSERT(strstr((const char *)output, "name") != NULL, "dump: has name");
    ASSERT(strstr((const char *)output, "test") != NULL, "dump: has test");

    yaml_emitter_delete(&emitter);
}

/* Test round-trip: parse then emit */
static void test_round_trip(void) {
    const char *yaml_input = "key: value\n";

    /* Parse */
    yaml_parser_t parser;
    ASSERT(yaml_parser_initialize(&parser), "roundtrip parse init");
    yaml_parser_set_input_string(&parser, (const unsigned char *)yaml_input,
                                  strlen(yaml_input));

    /* Collect events */
    yaml_event_t events[32];
    int event_count = 0;
    while (event_count < 32) {
        ASSERT(yaml_parser_parse(&parser, &events[event_count]), "roundtrip parse");
        if (events[event_count].type == YAML_STREAM_END_EVENT) {
            event_count++;
            break;
        }
        event_count++;
    }
    yaml_parser_delete(&parser);

    /* Emit */
    yaml_emitter_t emitter;
    unsigned char output[4096];
    size_t written = 0;
    ASSERT(yaml_emitter_initialize(&emitter), "roundtrip emit init");
    yaml_emitter_set_output_string(&emitter, output, sizeof(output), &written);

    for (int i = 0; i < event_count; i++) {
        ASSERT(yaml_emitter_emit(&emitter, &events[i]), "roundtrip emit");
    }

    yaml_emitter_delete(&emitter);

    output[written] = '\0';
    ASSERT(strstr((const char *)output, "key") != NULL, "roundtrip: has key");
    ASSERT(strstr((const char *)output, "value") != NULL, "roundtrip: has value");
}

int main(void) {
    TEST_SUITE_BEGIN("Emitter");

    test_emitter_init();
    test_emitter_config();
    test_string_output();
    test_file_output();
    test_emit_mapping();
    test_emit_sequence();
    test_dump();
    test_round_trip();

    TEST_SUITE_END();
}
