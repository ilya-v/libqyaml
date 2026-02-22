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

/* Helper: set up emitter with string output */
static void setup_emitter(yaml_emitter_t *emitter, unsigned char *output,
                          size_t output_size, size_t *written) {
    yaml_emitter_initialize(emitter);
    yaml_emitter_set_output_string(emitter, output, output_size, written);
    yaml_emitter_set_encoding(emitter, YAML_UTF8_ENCODING);
}

/* Helper: emit stream start + explicit doc start with version directive */
static void emit_stream_and_doc_start(yaml_emitter_t *emitter,
                                       yaml_version_directive_t *version,
                                       yaml_tag_directive_t *tag_start,
                                       yaml_tag_directive_t *tag_end,
                                       int implicit) {
    yaml_event_t event;
    yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
    yaml_emitter_emit(emitter, &event);
    yaml_document_start_event_initialize(&event, version, tag_start, tag_end, implicit);
    yaml_emitter_emit(emitter, &event);
}

/* Test explicit document markers (--- and ...) */
static void test_explicit_doc_markers(void) {
    yaml_emitter_t emitter;
    yaml_event_t event;
    unsigned char output[4096];
    size_t written = 0;

    setup_emitter(&emitter, output, sizeof(output), &written);

    /* Explicit doc start (implicit=0 forces --- marker) */
    emit_stream_and_doc_start(&emitter, NULL, NULL, NULL, 0);

    yaml_scalar_event_initialize(&event, NULL, (const yaml_char_t *)YAML_STR_TAG,
        (const yaml_char_t *)"test", 4, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);

    /* Explicit doc end (implicit=0 forces ... marker) */
    yaml_document_end_event_initialize(&event, 0);
    yaml_emitter_emit(&emitter, &event);

    yaml_stream_end_event_initialize(&event);
    yaml_emitter_emit(&emitter, &event);

    output[written] = '\0';
    ASSERT(strstr((const char *)output, "---") != NULL,
           "explicit doc start should produce ---");
    ASSERT(strstr((const char *)output, "...") != NULL,
           "explicit doc end should produce ...");

    yaml_emitter_delete(&emitter);
}

/* Test version directive */
static void test_version_directive(void) {
    yaml_emitter_t emitter;
    yaml_event_t event;
    unsigned char output[4096];
    size_t written = 0;
    yaml_version_directive_t version = { 1, 1 };

    setup_emitter(&emitter, output, sizeof(output), &written);
    emit_stream_and_doc_start(&emitter, &version, NULL, NULL, 0);

    yaml_scalar_event_initialize(&event, NULL, (const yaml_char_t *)YAML_STR_TAG,
        (const yaml_char_t *)"data", 4, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);

    yaml_document_end_event_initialize(&event, 1);
    yaml_emitter_emit(&emitter, &event);

    yaml_stream_end_event_initialize(&event);
    yaml_emitter_emit(&emitter, &event);

    output[written] = '\0';
    ASSERT(strstr((const char *)output, "%YAML 1.1") != NULL,
           "version directive should produce %YAML 1.1");

    yaml_emitter_delete(&emitter);
}

/* Test tag directives */
static void test_tag_directive(void) {
    yaml_emitter_t emitter;
    yaml_event_t event;
    unsigned char output[4096];
    size_t written = 0;

    yaml_tag_directive_t tags[1];
    tags[0].handle = (yaml_char_t *)"!e!";
    tags[0].prefix = (yaml_char_t *)"tag:example.com,2000:";

    setup_emitter(&emitter, output, sizeof(output), &written);
    emit_stream_and_doc_start(&emitter, NULL, &tags[0], &tags[1], 0);

    /* Use the custom tag on a scalar */
    yaml_scalar_event_initialize(&event, NULL,
        (const yaml_char_t *)"tag:example.com,2000:foo",
        (const yaml_char_t *)"bar", 3, 0, 0, YAML_PLAIN_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);

    yaml_document_end_event_initialize(&event, 1);
    yaml_emitter_emit(&emitter, &event);

    yaml_stream_end_event_initialize(&event);
    yaml_emitter_emit(&emitter, &event);

    output[written] = '\0';
    ASSERT(strstr((const char *)output, "%TAG !e! tag:example.com,2000:") != NULL,
           "tag directive should appear in output");
    ASSERT(strstr((const char *)output, "!e!foo") != NULL,
           "custom tag should be shortened to !e!foo");

    yaml_emitter_delete(&emitter);
}

/* Test canonical mode emitter */
static void test_canonical_mode(void) {
    yaml_emitter_t emitter;
    yaml_event_t event;
    unsigned char output[4096];
    size_t written = 0;

    yaml_emitter_initialize(&emitter);
    yaml_emitter_set_output_string(&emitter, output, sizeof(output), &written);
    yaml_emitter_set_canonical(&emitter, 1);

    yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
    yaml_emitter_emit(&emitter, &event);

    yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1);
    yaml_emitter_emit(&emitter, &event);

    yaml_mapping_start_event_initialize(&event, NULL,
        (const yaml_char_t *)YAML_MAP_TAG, 1, YAML_ANY_MAPPING_STYLE);
    yaml_emitter_emit(&emitter, &event);

    yaml_scalar_event_initialize(&event, NULL, (const yaml_char_t *)YAML_STR_TAG,
        (const yaml_char_t *)"key", 3, 1, 1, YAML_ANY_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);

    yaml_scalar_event_initialize(&event, NULL, (const yaml_char_t *)YAML_STR_TAG,
        (const yaml_char_t *)"val", 3, 1, 1, YAML_ANY_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);

    yaml_mapping_end_event_initialize(&event);
    yaml_emitter_emit(&emitter, &event);

    yaml_document_end_event_initialize(&event, 1);
    yaml_emitter_emit(&emitter, &event);

    yaml_stream_end_event_initialize(&event);
    yaml_emitter_emit(&emitter, &event);

    output[written] = '\0';
    /* Canonical mode forces explicit tags and flow style */
    ASSERT(strstr((const char *)output, "---") != NULL,
           "canonical should produce explicit doc start");
    ASSERT(strstr((const char *)output, "!!map") != NULL ||
           strstr((const char *)output, "tag:yaml.org,2002:map") != NULL,
           "canonical should produce explicit map tag");

    yaml_emitter_delete(&emitter);
}

/* Test flow sequence style */
static void test_flow_sequence(void) {
    yaml_emitter_t emitter;
    yaml_event_t event;
    unsigned char output[4096];
    size_t written = 0;

    setup_emitter(&emitter, output, sizeof(output), &written);

    yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
    yaml_emitter_emit(&emitter, &event);

    yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1);
    yaml_emitter_emit(&emitter, &event);

    yaml_sequence_start_event_initialize(&event, NULL,
        (const yaml_char_t *)YAML_SEQ_TAG, 1, YAML_FLOW_SEQUENCE_STYLE);
    yaml_emitter_emit(&emitter, &event);

    yaml_scalar_event_initialize(&event, NULL, (const yaml_char_t *)YAML_STR_TAG,
        (const yaml_char_t *)"a", 1, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);

    yaml_scalar_event_initialize(&event, NULL, (const yaml_char_t *)YAML_STR_TAG,
        (const yaml_char_t *)"b", 1, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);

    yaml_sequence_end_event_initialize(&event);
    yaml_emitter_emit(&emitter, &event);

    yaml_document_end_event_initialize(&event, 1);
    yaml_emitter_emit(&emitter, &event);

    yaml_stream_end_event_initialize(&event);
    yaml_emitter_emit(&emitter, &event);

    output[written] = '\0';
    ASSERT(strstr((const char *)output, "[") != NULL,
           "flow sequence should use [ bracket");

    yaml_emitter_delete(&emitter);
}

/* Test flow mapping style */
static void test_flow_mapping(void) {
    yaml_emitter_t emitter;
    yaml_event_t event;
    unsigned char output[4096];
    size_t written = 0;

    setup_emitter(&emitter, output, sizeof(output), &written);

    yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
    yaml_emitter_emit(&emitter, &event);

    yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1);
    yaml_emitter_emit(&emitter, &event);

    yaml_mapping_start_event_initialize(&event, NULL,
        (const yaml_char_t *)YAML_MAP_TAG, 1, YAML_FLOW_MAPPING_STYLE);
    yaml_emitter_emit(&emitter, &event);

    yaml_scalar_event_initialize(&event, NULL, (const yaml_char_t *)YAML_STR_TAG,
        (const yaml_char_t *)"x", 1, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);

    yaml_scalar_event_initialize(&event, NULL, (const yaml_char_t *)YAML_STR_TAG,
        (const yaml_char_t *)"1", 1, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);

    yaml_mapping_end_event_initialize(&event);
    yaml_emitter_emit(&emitter, &event);

    yaml_document_end_event_initialize(&event, 1);
    yaml_emitter_emit(&emitter, &event);

    yaml_stream_end_event_initialize(&event);
    yaml_emitter_emit(&emitter, &event);

    output[written] = '\0';
    ASSERT(strstr((const char *)output, "{") != NULL,
           "flow mapping should use { brace");

    yaml_emitter_delete(&emitter);
}

/* Test anchors and aliases */
static void test_anchors_aliases(void) {
    yaml_emitter_t emitter;
    yaml_event_t event;
    unsigned char output[4096];
    size_t written = 0;

    setup_emitter(&emitter, output, sizeof(output), &written);

    yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
    yaml_emitter_emit(&emitter, &event);

    yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1);
    yaml_emitter_emit(&emitter, &event);

    yaml_mapping_start_event_initialize(&event, NULL,
        (const yaml_char_t *)YAML_MAP_TAG, 1, YAML_BLOCK_MAPPING_STYLE);
    yaml_emitter_emit(&emitter, &event);

    /* key: first */
    yaml_scalar_event_initialize(&event, NULL, (const yaml_char_t *)YAML_STR_TAG,
        (const yaml_char_t *)"first", 5, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);

    /* value: &anchor "shared" */
    yaml_scalar_event_initialize(&event,
        (const yaml_char_t *)"anchor",
        (const yaml_char_t *)YAML_STR_TAG,
        (const yaml_char_t *)"shared", 6, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);

    /* key: second */
    yaml_scalar_event_initialize(&event, NULL, (const yaml_char_t *)YAML_STR_TAG,
        (const yaml_char_t *)"second", 6, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);

    /* value: *anchor */
    yaml_alias_event_initialize(&event, (const yaml_char_t *)"anchor");
    yaml_emitter_emit(&emitter, &event);

    yaml_mapping_end_event_initialize(&event);
    yaml_emitter_emit(&emitter, &event);

    yaml_document_end_event_initialize(&event, 1);
    yaml_emitter_emit(&emitter, &event);

    yaml_stream_end_event_initialize(&event);
    yaml_emitter_emit(&emitter, &event);

    output[written] = '\0';
    ASSERT(strstr((const char *)output, "&anchor") != NULL,
           "anchor should appear as &anchor");
    ASSERT(strstr((const char *)output, "*anchor") != NULL,
           "alias should appear as *anchor");

    yaml_emitter_delete(&emitter);
}

/* Test all scalar styles */
static void test_scalar_styles(void) {
    yaml_emitter_t emitter;
    yaml_event_t event;
    unsigned char output[8192];
    size_t written = 0;

    setup_emitter(&emitter, output, sizeof(output), &written);

    yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
    yaml_emitter_emit(&emitter, &event);

    yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1);
    yaml_emitter_emit(&emitter, &event);

    yaml_mapping_start_event_initialize(&event, NULL,
        (const yaml_char_t *)YAML_MAP_TAG, 1, YAML_BLOCK_MAPPING_STYLE);
    yaml_emitter_emit(&emitter, &event);

    /* Plain */
    yaml_scalar_event_initialize(&event, NULL, (const yaml_char_t *)YAML_STR_TAG,
        (const yaml_char_t *)"plain_key", 9, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);
    yaml_scalar_event_initialize(&event, NULL, (const yaml_char_t *)YAML_STR_TAG,
        (const yaml_char_t *)"plain value", 11, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);

    /* Single-quoted */
    yaml_scalar_event_initialize(&event, NULL, (const yaml_char_t *)YAML_STR_TAG,
        (const yaml_char_t *)"single_key", 10, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);
    yaml_scalar_event_initialize(&event, NULL, (const yaml_char_t *)YAML_STR_TAG,
        (const yaml_char_t *)"single's value", 14, 1, 1, YAML_SINGLE_QUOTED_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);

    /* Double-quoted */
    yaml_scalar_event_initialize(&event, NULL, (const yaml_char_t *)YAML_STR_TAG,
        (const yaml_char_t *)"double_key", 10, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);
    yaml_scalar_event_initialize(&event, NULL, (const yaml_char_t *)YAML_STR_TAG,
        (const yaml_char_t *)"double \"value\"", 14, 1, 1, YAML_DOUBLE_QUOTED_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);

    /* Literal block */
    yaml_scalar_event_initialize(&event, NULL, (const yaml_char_t *)YAML_STR_TAG,
        (const yaml_char_t *)"literal_key", 11, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);
    yaml_scalar_event_initialize(&event, NULL, (const yaml_char_t *)YAML_STR_TAG,
        (const yaml_char_t *)"line1\nline2\n", 12, 1, 1, YAML_LITERAL_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);

    /* Folded block */
    yaml_scalar_event_initialize(&event, NULL, (const yaml_char_t *)YAML_STR_TAG,
        (const yaml_char_t *)"folded_key", 10, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);
    yaml_scalar_event_initialize(&event, NULL, (const yaml_char_t *)YAML_STR_TAG,
        (const yaml_char_t *)"long line that should fold\n", 27, 1, 1, YAML_FOLDED_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);

    yaml_mapping_end_event_initialize(&event);
    yaml_emitter_emit(&emitter, &event);

    yaml_document_end_event_initialize(&event, 1);
    yaml_emitter_emit(&emitter, &event);

    yaml_stream_end_event_initialize(&event);
    yaml_emitter_emit(&emitter, &event);

    output[written] = '\0';
    ASSERT(strstr((const char *)output, "plain value") != NULL, "plain scalar");
    ASSERT(strstr((const char *)output, "'single''s value'") != NULL, "single-quoted scalar");
    ASSERT(strstr((const char *)output, "\"double \\\"value\\\"\"") != NULL, "double-quoted scalar");
    ASSERT(strstr((const char *)output, "|") != NULL, "literal block indicator");
    ASSERT(strstr((const char *)output, ">") != NULL, "folded block indicator");

    yaml_emitter_delete(&emitter);
}

/* Test multiple documents in a stream */
static void test_multi_document(void) {
    yaml_emitter_t emitter;
    yaml_event_t event;
    unsigned char output[4096];
    size_t written = 0;

    setup_emitter(&emitter, output, sizeof(output), &written);

    yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
    yaml_emitter_emit(&emitter, &event);

    /* Document 1 */
    yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1);
    yaml_emitter_emit(&emitter, &event);
    yaml_scalar_event_initialize(&event, NULL, (const yaml_char_t *)YAML_STR_TAG,
        (const yaml_char_t *)"doc1", 4, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);
    yaml_document_end_event_initialize(&event, 1);
    yaml_emitter_emit(&emitter, &event);

    /* Document 2 */
    yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 0);
    yaml_emitter_emit(&emitter, &event);
    yaml_scalar_event_initialize(&event, NULL, (const yaml_char_t *)YAML_STR_TAG,
        (const yaml_char_t *)"doc2", 4, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);
    yaml_document_end_event_initialize(&event, 0);
    yaml_emitter_emit(&emitter, &event);

    yaml_stream_end_event_initialize(&event);
    yaml_emitter_emit(&emitter, &event);

    output[written] = '\0';
    ASSERT(strstr((const char *)output, "doc1") != NULL, "multi-doc: doc1");
    ASSERT(strstr((const char *)output, "doc2") != NULL, "multi-doc: doc2");
    ASSERT(strstr((const char *)output, "---") != NULL, "multi-doc: --- separator");
    ASSERT(strstr((const char *)output, "...") != NULL, "multi-doc: ... marker");

    yaml_emitter_delete(&emitter);
}

/* Test nested structures: mapping containing sequences and mappings */
static void test_nested_structures(void) {
    yaml_emitter_t emitter;
    yaml_event_t event;
    unsigned char output[8192];
    size_t written = 0;

    setup_emitter(&emitter, output, sizeof(output), &written);

    yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
    yaml_emitter_emit(&emitter, &event);

    yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1);
    yaml_emitter_emit(&emitter, &event);

    /* Top-level mapping */
    yaml_mapping_start_event_initialize(&event, NULL,
        (const yaml_char_t *)YAML_MAP_TAG, 1, YAML_BLOCK_MAPPING_STYLE);
    yaml_emitter_emit(&emitter, &event);

    /* key: items */
    yaml_scalar_event_initialize(&event, NULL, (const yaml_char_t *)YAML_STR_TAG,
        (const yaml_char_t *)"items", 5, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);

    /* value: sequence of mappings */
    yaml_sequence_start_event_initialize(&event, NULL,
        (const yaml_char_t *)YAML_SEQ_TAG, 1, YAML_BLOCK_SEQUENCE_STYLE);
    yaml_emitter_emit(&emitter, &event);

    /* First item: a mapping */
    yaml_mapping_start_event_initialize(&event, NULL,
        (const yaml_char_t *)YAML_MAP_TAG, 1, YAML_FLOW_MAPPING_STYLE);
    yaml_emitter_emit(&emitter, &event);
    yaml_scalar_event_initialize(&event, NULL, (const yaml_char_t *)YAML_STR_TAG,
        (const yaml_char_t *)"name", 4, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);
    yaml_scalar_event_initialize(&event, NULL, (const yaml_char_t *)YAML_STR_TAG,
        (const yaml_char_t *)"alpha", 5, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);
    yaml_mapping_end_event_initialize(&event);
    yaml_emitter_emit(&emitter, &event);

    /* Second item: another mapping */
    yaml_mapping_start_event_initialize(&event, NULL,
        (const yaml_char_t *)YAML_MAP_TAG, 1, YAML_FLOW_MAPPING_STYLE);
    yaml_emitter_emit(&emitter, &event);
    yaml_scalar_event_initialize(&event, NULL, (const yaml_char_t *)YAML_STR_TAG,
        (const yaml_char_t *)"name", 4, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);
    yaml_scalar_event_initialize(&event, NULL, (const yaml_char_t *)YAML_STR_TAG,
        (const yaml_char_t *)"beta", 4, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);
    yaml_mapping_end_event_initialize(&event);
    yaml_emitter_emit(&emitter, &event);

    yaml_sequence_end_event_initialize(&event);
    yaml_emitter_emit(&emitter, &event);

    yaml_mapping_end_event_initialize(&event);
    yaml_emitter_emit(&emitter, &event);

    yaml_document_end_event_initialize(&event, 1);
    yaml_emitter_emit(&emitter, &event);

    yaml_stream_end_event_initialize(&event);
    yaml_emitter_emit(&emitter, &event);

    output[written] = '\0';
    ASSERT(strstr((const char *)output, "items") != NULL, "nested: has items key");
    ASSERT(strstr((const char *)output, "alpha") != NULL, "nested: has alpha");
    ASSERT(strstr((const char *)output, "beta") != NULL, "nested: has beta");

    yaml_emitter_delete(&emitter);
}

/* Test non-default tags (verbatim tag) */
static void test_non_default_tag(void) {
    yaml_emitter_t emitter;
    yaml_event_t event;
    unsigned char output[4096];
    size_t written = 0;

    setup_emitter(&emitter, output, sizeof(output), &written);

    yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
    yaml_emitter_emit(&emitter, &event);

    yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1);
    yaml_emitter_emit(&emitter, &event);

    /* Scalar with non-default tag -- forces tag output */
    yaml_scalar_event_initialize(&event, NULL,
        (const yaml_char_t *)"!!int",
        (const yaml_char_t *)"42", 2, 0, 0, YAML_PLAIN_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);

    yaml_document_end_event_initialize(&event, 1);
    yaml_emitter_emit(&emitter, &event);

    yaml_stream_end_event_initialize(&event);
    yaml_emitter_emit(&emitter, &event);

    output[written] = '\0';
    /* The tag may appear as !!int, !<tag:...int>, or !%21int depending
     * on whether the !! handle is registered. We just check it's present. */
    ASSERT(strstr((const char *)output, "int") != NULL,
           "non-default tag should be emitted (contains 'int')");
    ASSERT(strstr((const char *)output, "42") != NULL,
           "scalar value should be present");

    yaml_emitter_delete(&emitter);
}

/* Test emitting with width limit (line wrapping) */
static void test_width_limit(void) {
    yaml_emitter_t emitter;
    yaml_event_t event;
    unsigned char output[8192];
    size_t written = 0;

    yaml_emitter_initialize(&emitter);
    yaml_emitter_set_output_string(&emitter, output, sizeof(output), &written);
    yaml_emitter_set_width(&emitter, 20);

    yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
    yaml_emitter_emit(&emitter, &event);

    yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1);
    yaml_emitter_emit(&emitter, &event);

    /* Long double-quoted string that should wrap */
    yaml_scalar_event_initialize(&event, NULL, (const yaml_char_t *)YAML_STR_TAG,
        (const yaml_char_t *)"this is a fairly long string that should be wrapped at the width limit",
        70, 1, 1, YAML_DOUBLE_QUOTED_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);

    yaml_document_end_event_initialize(&event, 1);
    yaml_emitter_emit(&emitter, &event);

    yaml_stream_end_event_initialize(&event);
    yaml_emitter_emit(&emitter, &event);

    /* Output should be multi-line due to wrapping */
    output[written] = '\0';
    ASSERT(written > 0, "width limit: output produced");

    yaml_emitter_delete(&emitter);
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
    test_explicit_doc_markers();
    test_version_directive();
    test_tag_directive();
    test_canonical_mode();
    test_flow_sequence();
    test_flow_mapping();
    test_anchors_aliases();
    test_scalar_styles();
    test_multi_document();
    test_nested_structures();
    test_non_default_tag();
    test_width_limit();

    TEST_SUITE_END();
}
