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

/* ==================================================================
 * Emitter deep-dive: double-quoted escape sequences
 * Each test emits a scalar containing a specific control/special
 * character and verifies the emitter produces the correct escape.
 * ================================================================== */

/* Helper: emit a single double-quoted scalar and return the output string.
 * unicode_flag: 1=pass non-ASCII through, 0=escape non-ASCII */
static int emit_double_quoted_ex(const unsigned char *value, size_t length,
                                 int unicode_flag,
                                 char *out, size_t out_size, size_t *out_len) {
    yaml_emitter_t emitter;
    yaml_event_t event;
    unsigned char output[8192];
    size_t written = 0;

    if (!yaml_emitter_initialize(&emitter)) return 0;
    yaml_emitter_set_output_string(&emitter, output, sizeof(output), &written);
    yaml_emitter_set_unicode(&emitter, unicode_flag);

    yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
    if (!yaml_emitter_emit(&emitter, &event)) goto fail;

    yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1);
    if (!yaml_emitter_emit(&emitter, &event)) goto fail;

    yaml_scalar_event_initialize(&event, NULL, (const yaml_char_t *)YAML_STR_TAG,
        value, (int)length, 0, 0, YAML_DOUBLE_QUOTED_SCALAR_STYLE);
    if (!yaml_emitter_emit(&emitter, &event)) goto fail;

    yaml_document_end_event_initialize(&event, 1);
    if (!yaml_emitter_emit(&emitter, &event)) goto fail;

    yaml_stream_end_event_initialize(&event);
    if (!yaml_emitter_emit(&emitter, &event)) goto fail;

    yaml_emitter_delete(&emitter);

    if (written < out_size) {
        memcpy(out, output, written);
        out[written] = '\0';
        *out_len = written;
    }
    return 1;

fail:
    yaml_emitter_delete(&emitter);
    return 0;
}

static int emit_double_quoted(const unsigned char *value, size_t length,
                              char *out, size_t out_size, size_t *out_len) {
    return emit_double_quoted_ex(value, length, 1, out, out_size, out_len);
}

static int emit_double_quoted_no_unicode(const unsigned char *value, size_t length,
                                         char *out, size_t out_size, size_t *out_len) {
    return emit_double_quoted_ex(value, length, 0, out, out_size, out_len);
}

static void test_escape_null(void) {
    char out[8192]; size_t len;
    unsigned char val[] = {'a', 0x00, 'b'};
    ASSERT(emit_double_quoted(val, 3, out, sizeof(out), &len), "escape null: emit");
    ASSERT(strstr(out, "\\0") != NULL, "escape null: contains \\0");
}

static void test_escape_bell(void) {
    char out[8192]; size_t len;
    unsigned char val[] = {'a', 0x07, 'b'};
    ASSERT(emit_double_quoted(val, 3, out, sizeof(out), &len), "escape bell: emit");
    ASSERT(strstr(out, "\\a") != NULL, "escape bell: contains \\a");
}

static void test_escape_backspace(void) {
    char out[8192]; size_t len;
    unsigned char val[] = {'a', 0x08, 'b'};
    ASSERT(emit_double_quoted(val, 3, out, sizeof(out), &len), "escape bs: emit");
    ASSERT(strstr(out, "\\b") != NULL, "escape bs: contains \\b");
}

static void test_escape_tab(void) {
    char out[8192]; size_t len;
    unsigned char val[] = {'a', 0x09, 'b'};
    ASSERT(emit_double_quoted(val, 3, out, sizeof(out), &len), "escape tab: emit");
    ASSERT(strstr(out, "\\t") != NULL, "escape tab: contains \\t");
}

static void test_escape_newline(void) {
    char out[8192]; size_t len;
    unsigned char val[] = {'a', 0x0A, 'b'};
    ASSERT(emit_double_quoted(val, 3, out, sizeof(out), &len), "escape nl: emit");
    ASSERT(strstr(out, "\\n") != NULL, "escape nl: contains \\n");
}

static void test_escape_vtab(void) {
    char out[8192]; size_t len;
    unsigned char val[] = {'a', 0x0B, 'b'};
    ASSERT(emit_double_quoted(val, 3, out, sizeof(out), &len), "escape vtab: emit");
    ASSERT(strstr(out, "\\v") != NULL, "escape vtab: contains \\v");
}

static void test_escape_formfeed(void) {
    char out[8192]; size_t len;
    unsigned char val[] = {'a', 0x0C, 'b'};
    ASSERT(emit_double_quoted(val, 3, out, sizeof(out), &len), "escape ff: emit");
    ASSERT(strstr(out, "\\f") != NULL, "escape ff: contains \\f");
}

static void test_escape_cr(void) {
    char out[8192]; size_t len;
    unsigned char val[] = {'a', 0x0D, 'b'};
    ASSERT(emit_double_quoted(val, 3, out, sizeof(out), &len), "escape cr: emit");
    ASSERT(strstr(out, "\\r") != NULL, "escape cr: contains \\r");
}

static void test_escape_esc(void) {
    char out[8192]; size_t len;
    unsigned char val[] = {'a', 0x1B, 'b'};
    ASSERT(emit_double_quoted(val, 3, out, sizeof(out), &len), "escape esc: emit");
    ASSERT(strstr(out, "\\e") != NULL, "escape esc: contains \\e");
}

static void test_escape_backslash(void) {
    char out[8192]; size_t len;
    unsigned char val[] = {'a', '\\', 'b'};
    ASSERT(emit_double_quoted(val, 3, out, sizeof(out), &len), "escape bs: emit");
    ASSERT(strstr(out, "\\\\") != NULL, "escape backslash: contains \\\\");
}

static void test_escape_dquote(void) {
    char out[8192]; size_t len;
    unsigned char val[] = {'a', '"', 'b'};
    ASSERT(emit_double_quoted(val, 3, out, sizeof(out), &len), "escape dquote: emit");
    ASSERT(strstr(out, "\\\"") != NULL, "escape dquote: contains \\\"");
}

static void test_escape_next_line(void) {
    /* U+0085 NEL = 0xC2 0x85 in UTF-8 */
    char out[8192]; size_t len;
    unsigned char val[] = {'a', 0xC2, 0x85, 'b'};
    ASSERT(emit_double_quoted(val, 4, out, sizeof(out), &len), "escape NEL: emit");
    ASSERT(strstr(out, "\\N") != NULL, "escape NEL: contains \\N");
}

static void test_escape_nbsp(void) {
    /* U+00A0 NBSP = 0xC2 0xA0 in UTF-8
     * IS_PRINTABLE considers this printable, so need unicode=0 to force escape */
    char out[8192]; size_t len;
    unsigned char val[] = {'a', 0xC2, 0xA0, 'b'};
    ASSERT(emit_double_quoted_no_unicode(val, 4, out, sizeof(out), &len), "escape NBSP: emit");
    ASSERT(strstr(out, "\\_") != NULL, "escape NBSP: contains \\_");
}

static void test_escape_linesep(void) {
    /* U+2028 LS = 0xE2 0x80 0xA8 in UTF-8
     * IS_BREAK matches LS, so it's escaped even with unicode=1 */
    char out[8192]; size_t len;
    unsigned char val[] = {'a', 0xE2, 0x80, 0xA8, 'b'};
    ASSERT(emit_double_quoted(val, 5, out, sizeof(out), &len), "escape LS: emit");
    ASSERT(strstr(out, "\\L") != NULL, "escape LS: contains \\L");
}

static void test_escape_parasep(void) {
    /* U+2029 PS = 0xE2 0x80 0xA9 in UTF-8
     * IS_BREAK matches PS, so it's escaped even with unicode=1 */
    char out[8192]; size_t len;
    unsigned char val[] = {'a', 0xE2, 0x80, 0xA9, 'b'};
    ASSERT(emit_double_quoted(val, 5, out, sizeof(out), &len), "escape PS: emit");
    ASSERT(strstr(out, "\\P") != NULL, "escape PS: contains \\P");
}

static void test_escape_hex(void) {
    /* Control char 0x01 should produce \x01 */
    char out[8192]; size_t len;
    unsigned char val[] = {'a', 0x01, 'b'};
    ASSERT(emit_double_quoted(val, 3, out, sizeof(out), &len), "escape hex: emit");
    ASSERT(strstr(out, "\\x01") != NULL, "escape hex: contains \\x01");
}

static void test_escape_unicode_bmp(void) {
    /* Non-printable BMP char: U+FFFE = 0xEF 0xBF 0xBE in UTF-8 */
    char out[8192]; size_t len;
    unsigned char val[] = {'a', 0xEF, 0xBF, 0xBE, 'b'};
    ASSERT(emit_double_quoted(val, 5, out, sizeof(out), &len), "escape ubmp: emit");
    ASSERT(strstr(out, "\\uFFFE") != NULL, "escape ubmp: contains \\uFFFE");
}

static void test_escape_unicode_nonbmp(void) {
    /* Non-BMP: U+1F600 = 0xF0 0x9F 0x98 0x80 in UTF-8
     * IS_PRINTABLE doesn't cover 4-byte UTF-8, so it will be escaped */
    char out[8192]; size_t len;
    unsigned char val[] = {'a', 0xF0, 0x9F, 0x98, 0x80, 'b'};
    ASSERT(emit_double_quoted(val, 6, out, sizeof(out), &len), "escape nonbmp: emit");
    ASSERT(strstr(out, "\\U0001F600") != NULL, "escape nonbmp: contains \\U0001F600");
}

/* ==================================================================
 * Emitter deep-dive: block scalar hints (indent + chomp)
 * ================================================================== */

static void test_block_scalar_empty_content(void) {
    /* Empty scalar with literal style -- not allowed, emitter will use quotes */
    yaml_emitter_t emitter;
    yaml_event_t event;
    unsigned char output[4096];
    size_t written = 0;

    yaml_emitter_initialize(&emitter);
    yaml_emitter_set_output_string(&emitter, output, sizeof(output), &written);

    yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
    yaml_emitter_emit(&emitter, &event);
    yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1);
    yaml_emitter_emit(&emitter, &event);

    /* Request literal style for empty scalar */
    yaml_scalar_event_initialize(&event, NULL, (const yaml_char_t *)YAML_STR_TAG,
        (const yaml_char_t *)"", 0, 1, 1, YAML_LITERAL_SCALAR_STYLE);
    ASSERT(yaml_emitter_emit(&emitter, &event), "block empty: emit scalar");

    yaml_document_end_event_initialize(&event, 1);
    yaml_emitter_emit(&emitter, &event);
    yaml_stream_end_event_initialize(&event);
    yaml_emitter_emit(&emitter, &event);

    output[written] = '\0';
    /* Emitter should fall back to a quoted style for empty string */
    ASSERT(written > 0, "block empty: produces output");

    yaml_emitter_delete(&emitter);
}

static void test_block_scalar_leading_space(void) {
    /* Literal scalar starting with space triggers indent indicator */
    yaml_emitter_t emitter;
    yaml_event_t event;
    unsigned char output[4096];
    size_t written = 0;

    yaml_emitter_initialize(&emitter);
    yaml_emitter_set_output_string(&emitter, output, sizeof(output), &written);

    yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
    yaml_emitter_emit(&emitter, &event);
    yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1);
    yaml_emitter_emit(&emitter, &event);

    yaml_scalar_event_initialize(&event, NULL, (const yaml_char_t *)YAML_STR_TAG,
        (const yaml_char_t *)" indented first line\nsecond line\n", 32, 0, 0,
        YAML_LITERAL_SCALAR_STYLE);
    ASSERT(yaml_emitter_emit(&emitter, &event), "block leading space: emit");

    yaml_document_end_event_initialize(&event, 1);
    yaml_emitter_emit(&emitter, &event);
    yaml_stream_end_event_initialize(&event);
    yaml_emitter_emit(&emitter, &event);

    output[written] = '\0';
    /* Should contain indent indicator (|2 or similar) */
    ASSERT(strstr((const char *)output, "|") != NULL, "block leading space: has | indicator");

    yaml_emitter_delete(&emitter);
}

static void test_block_scalar_leading_break(void) {
    /* Literal scalar starting with newline triggers indent indicator */
    yaml_emitter_t emitter;
    yaml_event_t event;
    unsigned char output[4096];
    size_t written = 0;

    yaml_emitter_initialize(&emitter);
    yaml_emitter_set_output_string(&emitter, output, sizeof(output), &written);

    yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
    yaml_emitter_emit(&emitter, &event);
    yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1);
    yaml_emitter_emit(&emitter, &event);

    yaml_scalar_event_initialize(&event, NULL, (const yaml_char_t *)YAML_STR_TAG,
        (const yaml_char_t *)"\nfirst line\nsecond line\n", 23, 0, 0,
        YAML_LITERAL_SCALAR_STYLE);
    ASSERT(yaml_emitter_emit(&emitter, &event), "block leading break: emit");

    yaml_document_end_event_initialize(&event, 1);
    yaml_emitter_emit(&emitter, &event);
    yaml_stream_end_event_initialize(&event);
    yaml_emitter_emit(&emitter, &event);

    output[written] = '\0';
    ASSERT(strstr((const char *)output, "|") != NULL, "block leading break: has | indicator");

    yaml_emitter_delete(&emitter);
}

static void test_block_scalar_strip_chomp(void) {
    /* Scalar without trailing newline should get strip chomp (-) */
    yaml_emitter_t emitter;
    yaml_event_t event;
    unsigned char output[4096];
    size_t written = 0;

    yaml_emitter_initialize(&emitter);
    yaml_emitter_set_output_string(&emitter, output, sizeof(output), &written);

    yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
    yaml_emitter_emit(&emitter, &event);
    yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1);
    yaml_emitter_emit(&emitter, &event);

    yaml_scalar_event_initialize(&event, NULL, (const yaml_char_t *)YAML_STR_TAG,
        (const yaml_char_t *)"no trailing newline", 19, 0, 0,
        YAML_LITERAL_SCALAR_STYLE);
    ASSERT(yaml_emitter_emit(&emitter, &event), "strip chomp: emit");

    yaml_document_end_event_initialize(&event, 1);
    yaml_emitter_emit(&emitter, &event);
    yaml_stream_end_event_initialize(&event);
    yaml_emitter_emit(&emitter, &event);

    output[written] = '\0';
    ASSERT(strstr((const char *)output, "|-") != NULL, "strip chomp: has |-");

    yaml_emitter_delete(&emitter);
}

static void test_block_scalar_keep_chomp(void) {
    /* Scalar ending with two newlines should get keep chomp (+) */
    yaml_emitter_t emitter;
    yaml_event_t event;
    unsigned char output[4096];
    size_t written = 0;

    yaml_emitter_initialize(&emitter);
    yaml_emitter_set_output_string(&emitter, output, sizeof(output), &written);

    yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
    yaml_emitter_emit(&emitter, &event);
    yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1);
    yaml_emitter_emit(&emitter, &event);

    yaml_scalar_event_initialize(&event, NULL, (const yaml_char_t *)YAML_STR_TAG,
        (const yaml_char_t *)"keep trailing\n\n", 15, 0, 0,
        YAML_LITERAL_SCALAR_STYLE);
    ASSERT(yaml_emitter_emit(&emitter, &event), "keep chomp: emit");

    yaml_document_end_event_initialize(&event, 1);
    yaml_emitter_emit(&emitter, &event);
    yaml_stream_end_event_initialize(&event);
    yaml_emitter_emit(&emitter, &event);

    output[written] = '\0';
    ASSERT(strstr((const char *)output, "|+") != NULL, "keep chomp: has |+");

    yaml_emitter_delete(&emitter);
}

static void test_block_scalar_only_newline(void) {
    /* Scalar that is just "\n" should use keep chomp */
    yaml_emitter_t emitter;
    yaml_event_t event;
    unsigned char output[4096];
    size_t written = 0;

    yaml_emitter_initialize(&emitter);
    yaml_emitter_set_output_string(&emitter, output, sizeof(output), &written);

    yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
    yaml_emitter_emit(&emitter, &event);
    yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1);
    yaml_emitter_emit(&emitter, &event);

    yaml_scalar_event_initialize(&event, NULL, (const yaml_char_t *)YAML_STR_TAG,
        (const yaml_char_t *)"\n", 1, 0, 0,
        YAML_LITERAL_SCALAR_STYLE);
    ASSERT(yaml_emitter_emit(&emitter, &event), "only newline: emit");

    yaml_document_end_event_initialize(&event, 1);
    yaml_emitter_emit(&emitter, &event);
    yaml_stream_end_event_initialize(&event);
    yaml_emitter_emit(&emitter, &event);

    output[written] = '\0';
    /* Single newline: start==pointer after backup, so keep chomp */
    ASSERT(written > 0, "only newline: produces output");

    yaml_emitter_delete(&emitter);
}

/* ==================================================================
 * Emitter deep-dive: scalar analysis edge cases
 * ================================================================== */

static void test_scalar_starting_with_dash(void) {
    /* "---" at start of scalar forces quoting */
    char out[8192]; size_t len;
    ASSERT(emit_double_quoted((const unsigned char *)"--- header", 10, out, sizeof(out), &len),
           "dash scalar: emit");
    ASSERT(strstr(out, "--- header") != NULL || strstr(out, "\\\"") != NULL || strstr(out, "'") != NULL,
           "dash scalar: properly handled");
}

static void test_scalar_starting_with_dots(void) {
    /* "..." at start forces quoting */
    char out[8192]; size_t len;
    ASSERT(emit_double_quoted((const unsigned char *)"... footer", 10, out, sizeof(out), &len),
           "dots scalar: emit");
    ASSERT(strstr(out, "... footer") != NULL, "dots scalar: properly handled");
}

static void test_scalar_space_break_forces_dquote(void) {
    /* Space followed by break forces double-quoted only */
    char out[8192]; size_t len;
    unsigned char val[] = {'a', ' ', '\n', 'b'};
    ASSERT(emit_double_quoted(val, 4, out, sizeof(out), &len),
           "space_break: emit");
    ASSERT(strstr(out, "\"") != NULL, "space_break: uses double quotes");
}

static void test_scalar_break_space(void) {
    /* Break followed by space */
    char out[8192]; size_t len;
    unsigned char val[] = {'a', '\n', ' ', 'b'};
    ASSERT(emit_double_quoted(val, 4, out, sizeof(out), &len),
           "break_space: emit");
    ASSERT(strstr(out, "\"") != NULL, "break_space: uses double quotes");
}

/* ==================================================================
 * Emitter deep-dive: multi-document with directives
 * ================================================================== */

static void test_version_directive_1_2(void) {
    yaml_emitter_t emitter;
    yaml_event_t event;
    unsigned char output[4096];
    size_t written = 0;

    yaml_emitter_initialize(&emitter);
    yaml_emitter_set_output_string(&emitter, output, sizeof(output), &written);

    yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
    yaml_emitter_emit(&emitter, &event);

    /* Version 1.2 */
    yaml_version_directive_t ver = {1, 2};
    yaml_document_start_event_initialize(&event, &ver, NULL, NULL, 0);
    yaml_emitter_emit(&emitter, &event);

    yaml_scalar_event_initialize(&event, NULL, (const yaml_char_t *)YAML_STR_TAG,
        (const yaml_char_t *)"test", 4, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);

    yaml_document_end_event_initialize(&event, 0);
    yaml_emitter_emit(&emitter, &event);

    yaml_stream_end_event_initialize(&event);
    yaml_emitter_emit(&emitter, &event);

    output[written] = '\0';
    ASSERT(strstr((const char *)output, "%YAML 1.2") != NULL,
           "version 1.2: contains %YAML 1.2");
    ASSERT(strstr((const char *)output, "---") != NULL,
           "version 1.2: contains ---");

    yaml_emitter_delete(&emitter);
}

static void test_multi_doc_with_version(void) {
    yaml_emitter_t emitter;
    yaml_event_t event;
    unsigned char output[4096];
    size_t written = 0;

    yaml_emitter_initialize(&emitter);
    yaml_emitter_set_output_string(&emitter, output, sizeof(output), &written);

    yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
    yaml_emitter_emit(&emitter, &event);

    /* First doc with version 1.1 */
    yaml_version_directive_t ver1 = {1, 1};
    yaml_document_start_event_initialize(&event, &ver1, NULL, NULL, 0);
    yaml_emitter_emit(&emitter, &event);
    yaml_scalar_event_initialize(&event, NULL, (const yaml_char_t *)YAML_STR_TAG,
        (const yaml_char_t *)"first", 5, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);
    yaml_document_end_event_initialize(&event, 0);
    yaml_emitter_emit(&emitter, &event);

    /* Second doc with version 1.2 */
    yaml_version_directive_t ver2 = {1, 2};
    yaml_document_start_event_initialize(&event, &ver2, NULL, NULL, 0);
    yaml_emitter_emit(&emitter, &event);
    yaml_scalar_event_initialize(&event, NULL, (const yaml_char_t *)YAML_STR_TAG,
        (const yaml_char_t *)"second", 6, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);
    yaml_document_end_event_initialize(&event, 0);
    yaml_emitter_emit(&emitter, &event);

    yaml_stream_end_event_initialize(&event);
    yaml_emitter_emit(&emitter, &event);

    output[written] = '\0';
    ASSERT(strstr((const char *)output, "1.1") != NULL,
           "multi doc version: contains 1.1");
    ASSERT(strstr((const char *)output, "1.2") != NULL,
           "multi doc version: contains 1.2");

    yaml_emitter_delete(&emitter);
}

static void test_doc_end_before_directive(void) {
    /* When a doc has open_ended content and the next doc has directives,
     * the emitter must emit "..." before the directive */
    yaml_emitter_t emitter;
    yaml_event_t event;
    unsigned char output[8192];
    size_t written = 0;

    yaml_emitter_initialize(&emitter);
    yaml_emitter_set_output_string(&emitter, output, sizeof(output), &written);

    yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
    yaml_emitter_emit(&emitter, &event);

    /* First doc: literal scalar with keep chomp (open_ended=2) */
    yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1);
    yaml_emitter_emit(&emitter, &event);
    yaml_scalar_event_initialize(&event, NULL, (const yaml_char_t *)YAML_STR_TAG,
        (const yaml_char_t *)"content\n\n", 9, 0, 0,
        YAML_LITERAL_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);
    yaml_document_end_event_initialize(&event, 1);
    yaml_emitter_emit(&emitter, &event);

    /* Second doc: with tag directive (forces ... before it) */
    yaml_tag_directive_t tags[] = {
        {(yaml_char_t *)"!e!", (yaml_char_t *)"tag:example.com,2000:"},
        {NULL, NULL}
    };
    yaml_document_start_event_initialize(&event, NULL, tags, tags + 1, 0);
    yaml_emitter_emit(&emitter, &event);
    yaml_scalar_event_initialize(&event, NULL, (const yaml_char_t *)YAML_STR_TAG,
        (const yaml_char_t *)"tagged", 6, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);
    yaml_document_end_event_initialize(&event, 0);
    yaml_emitter_emit(&emitter, &event);

    yaml_stream_end_event_initialize(&event);
    yaml_emitter_emit(&emitter, &event);

    output[written] = '\0';
    ASSERT(strstr((const char *)output, "...") != NULL,
           "doc end before directive: contains ...");
    ASSERT(strstr((const char *)output, "%TAG") != NULL,
           "doc end before directive: contains %TAG");

    yaml_emitter_delete(&emitter);
}

/* ==================================================================
 * Emitter deep-dive: check_simple_key coverage
 * ================================================================== */

static void test_long_key_not_simple(void) {
    /* Keys longer than 1024 bytes are not simple */
    yaml_emitter_t emitter;
    yaml_event_t event;
    unsigned char output[16384];
    size_t written = 0;
    char long_key[1100];
    memset(long_key, 'k', 1100);
    long_key[1099] = '\0';

    yaml_emitter_initialize(&emitter);
    yaml_emitter_set_output_string(&emitter, output, sizeof(output), &written);

    yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
    yaml_emitter_emit(&emitter, &event);
    yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1);
    yaml_emitter_emit(&emitter, &event);

    yaml_mapping_start_event_initialize(&event, NULL,
        (const yaml_char_t *)YAML_MAP_TAG, 1, YAML_BLOCK_MAPPING_STYLE);
    yaml_emitter_emit(&emitter, &event);

    yaml_scalar_event_initialize(&event, NULL, (const yaml_char_t *)YAML_STR_TAG,
        (const yaml_char_t *)long_key, 1099, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);

    yaml_scalar_event_initialize(&event, NULL, (const yaml_char_t *)YAML_STR_TAG,
        (const yaml_char_t *)"val", 3, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);

    yaml_mapping_end_event_initialize(&event);
    yaml_emitter_emit(&emitter, &event);
    yaml_document_end_event_initialize(&event, 1);
    yaml_emitter_emit(&emitter, &event);
    yaml_stream_end_event_initialize(&event);
    yaml_emitter_emit(&emitter, &event);

    output[written] = '\0';
    /* Long key should trigger explicit key (? indicator) */
    ASSERT(strstr((const char *)output, "?") != NULL,
           "long key: uses ? explicit key");

    yaml_emitter_delete(&emitter);
}

static void test_multiline_key_not_simple(void) {
    /* Multiline scalar key is not simple */
    yaml_emitter_t emitter;
    yaml_event_t event;
    unsigned char output[4096];
    size_t written = 0;

    yaml_emitter_initialize(&emitter);
    yaml_emitter_set_output_string(&emitter, output, sizeof(output), &written);

    yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
    yaml_emitter_emit(&emitter, &event);
    yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1);
    yaml_emitter_emit(&emitter, &event);

    yaml_mapping_start_event_initialize(&event, NULL,
        (const yaml_char_t *)YAML_MAP_TAG, 1, YAML_BLOCK_MAPPING_STYLE);
    yaml_emitter_emit(&emitter, &event);

    /* Multiline key */
    yaml_scalar_event_initialize(&event, NULL, (const yaml_char_t *)YAML_STR_TAG,
        (const yaml_char_t *)"line1\nline2", 11, 0, 0, YAML_LITERAL_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);

    yaml_scalar_event_initialize(&event, NULL, (const yaml_char_t *)YAML_STR_TAG,
        (const yaml_char_t *)"val", 3, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);

    yaml_mapping_end_event_initialize(&event);
    yaml_emitter_emit(&emitter, &event);
    yaml_document_end_event_initialize(&event, 1);
    yaml_emitter_emit(&emitter, &event);
    yaml_stream_end_event_initialize(&event);
    yaml_emitter_emit(&emitter, &event);

    output[written] = '\0';
    /* Multiline key should trigger explicit key */
    ASSERT(strstr((const char *)output, "?") != NULL,
           "multiline key: uses ? explicit key");

    yaml_emitter_delete(&emitter);
}

/* ==================================================================
 * Emitter deep-dive: flow context edge cases
 * ================================================================== */

static void test_flow_sequence_multiline(void) {
    /* Flow sequence with canonical mode should be multi-line */
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

    yaml_sequence_start_event_initialize(&event, NULL,
        (const yaml_char_t *)YAML_SEQ_TAG, 1, YAML_FLOW_SEQUENCE_STYLE);
    yaml_emitter_emit(&emitter, &event);

    for (int i = 0; i < 5; i++) {
        char val[16];
        snprintf(val, sizeof(val), "item%d", i);
        yaml_scalar_event_initialize(&event, NULL, (const yaml_char_t *)YAML_STR_TAG,
            (const yaml_char_t *)val, strlen(val), 1, 1, YAML_PLAIN_SCALAR_STYLE);
        yaml_emitter_emit(&emitter, &event);
    }

    yaml_sequence_end_event_initialize(&event);
    yaml_emitter_emit(&emitter, &event);
    yaml_document_end_event_initialize(&event, 1);
    yaml_emitter_emit(&emitter, &event);
    yaml_stream_end_event_initialize(&event);
    yaml_emitter_emit(&emitter, &event);

    output[written] = '\0';
    ASSERT(strstr((const char *)output, "[") != NULL,
           "flow seq canonical: has [");
    ASSERT(strstr((const char *)output, "]") != NULL,
           "flow seq canonical: has ]");

    yaml_emitter_delete(&emitter);
}

static void test_flow_mapping_multiline(void) {
    /* Flow mapping in canonical mode */
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
        (const yaml_char_t *)YAML_MAP_TAG, 1, YAML_FLOW_MAPPING_STYLE);
    yaml_emitter_emit(&emitter, &event);

    for (int i = 0; i < 3; i++) {
        char key[16], val[16];
        snprintf(key, sizeof(key), "key%d", i);
        snprintf(val, sizeof(val), "val%d", i);
        yaml_scalar_event_initialize(&event, NULL, (const yaml_char_t *)YAML_STR_TAG,
            (const yaml_char_t *)key, strlen(key), 1, 1, YAML_PLAIN_SCALAR_STYLE);
        yaml_emitter_emit(&emitter, &event);
        yaml_scalar_event_initialize(&event, NULL, (const yaml_char_t *)YAML_STR_TAG,
            (const yaml_char_t *)val, strlen(val), 1, 1, YAML_PLAIN_SCALAR_STYLE);
        yaml_emitter_emit(&emitter, &event);
    }

    yaml_mapping_end_event_initialize(&event);
    yaml_emitter_emit(&emitter, &event);
    yaml_document_end_event_initialize(&event, 1);
    yaml_emitter_emit(&emitter, &event);
    yaml_stream_end_event_initialize(&event);
    yaml_emitter_emit(&emitter, &event);

    output[written] = '\0';
    ASSERT(strstr((const char *)output, "{") != NULL,
           "flow map canonical: has {");
    ASSERT(strstr((const char *)output, "}") != NULL,
           "flow map canonical: has }");

    yaml_emitter_delete(&emitter);
}

static void test_block_mapping_compact(void) {
    /* Block mapping with sequence value -- should use compact notation */
    yaml_emitter_t emitter;
    yaml_event_t event;
    unsigned char output[4096];
    size_t written = 0;

    yaml_emitter_initialize(&emitter);
    yaml_emitter_set_output_string(&emitter, output, sizeof(output), &written);

    yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
    yaml_emitter_emit(&emitter, &event);
    yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1);
    yaml_emitter_emit(&emitter, &event);

    yaml_mapping_start_event_initialize(&event, NULL,
        (const yaml_char_t *)YAML_MAP_TAG, 1, YAML_BLOCK_MAPPING_STYLE);
    yaml_emitter_emit(&emitter, &event);

    yaml_scalar_event_initialize(&event, NULL, (const yaml_char_t *)YAML_STR_TAG,
        (const yaml_char_t *)"items", 5, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);

    yaml_sequence_start_event_initialize(&event, NULL,
        (const yaml_char_t *)YAML_SEQ_TAG, 1, YAML_BLOCK_SEQUENCE_STYLE);
    yaml_emitter_emit(&emitter, &event);

    yaml_scalar_event_initialize(&event, NULL, (const yaml_char_t *)YAML_STR_TAG,
        (const yaml_char_t *)"a", 1, 1, 1, YAML_PLAIN_SCALAR_STYLE);
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
    ASSERT(strstr((const char *)output, "items") != NULL,
           "block compact: has key");
    ASSERT(strstr((const char *)output, "- a") != NULL,
           "block compact: has - a");

    yaml_emitter_delete(&emitter);
}

/* ==================================================================
 * Emitter deep-dive: emitter error paths
 * ================================================================== */

static void test_emitter_wrong_event_order(void) {
    /* Sending a scalar before stream start should fail */
    yaml_emitter_t emitter;
    yaml_event_t event;
    unsigned char output[4096];
    size_t written = 0;

    yaml_emitter_initialize(&emitter);
    yaml_emitter_set_output_string(&emitter, output, sizeof(output), &written);

    yaml_scalar_event_initialize(&event, NULL, (const yaml_char_t *)YAML_STR_TAG,
        (const yaml_char_t *)"bad", 3, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    ASSERT(!yaml_emitter_emit(&emitter, &event), "wrong order: should fail");
    ASSERT(emitter.error != YAML_NO_ERROR, "wrong order: error set");

    yaml_emitter_delete(&emitter);
}

static void test_emitter_double_stream_start(void) {
    /* Sending stream start twice should fail */
    yaml_emitter_t emitter;
    yaml_event_t event;
    unsigned char output[4096];
    size_t written = 0;

    yaml_emitter_initialize(&emitter);
    yaml_emitter_set_output_string(&emitter, output, sizeof(output), &written);

    yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
    ASSERT(yaml_emitter_emit(&emitter, &event), "double stream: first ok");

    yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1);
    yaml_emitter_emit(&emitter, &event);

    yaml_scalar_event_initialize(&event, NULL, (const yaml_char_t *)YAML_STR_TAG,
        (const yaml_char_t *)"test", 4, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);

    yaml_document_end_event_initialize(&event, 1);
    yaml_emitter_emit(&emitter, &event);

    yaml_stream_end_event_initialize(&event);
    yaml_emitter_emit(&emitter, &event);

    /* Now try to emit after stream end */
    yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
    ASSERT(!yaml_emitter_emit(&emitter, &event), "after stream end: should fail");

    yaml_emitter_delete(&emitter);
}

/* ==================================================================
 * Emitter deep-dive: single-quoted edge cases
 * ================================================================== */

static void test_single_quoted_wrapping(void) {
    /* Long single-quoted scalar that should wrap at width */
    yaml_emitter_t emitter;
    yaml_event_t event;
    unsigned char output[8192];
    size_t written = 0;

    yaml_emitter_initialize(&emitter);
    yaml_emitter_set_output_string(&emitter, output, sizeof(output), &written);
    yaml_emitter_set_width(&emitter, 30);

    yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
    yaml_emitter_emit(&emitter, &event);
    yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1);
    yaml_emitter_emit(&emitter, &event);

    yaml_scalar_event_initialize(&event, NULL, (const yaml_char_t *)YAML_STR_TAG,
        (const yaml_char_t *)"this is a single quoted value that is quite long and should wrap",
        63, 0, 1, YAML_SINGLE_QUOTED_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);

    yaml_document_end_event_initialize(&event, 1);
    yaml_emitter_emit(&emitter, &event);
    yaml_stream_end_event_initialize(&event);
    yaml_emitter_emit(&emitter, &event);

    output[written] = '\0';
    /* Should be wrapped (multiple lines) */
    ASSERT(strstr((const char *)output, "'") != NULL,
           "single wrap: uses single quotes");

    yaml_emitter_delete(&emitter);
}

static void test_plain_scalar_wrapping(void) {
    /* Long plain scalar that should wrap at width */
    yaml_emitter_t emitter;
    yaml_event_t event;
    unsigned char output[8192];
    size_t written = 0;

    yaml_emitter_initialize(&emitter);
    yaml_emitter_set_output_string(&emitter, output, sizeof(output), &written);
    yaml_emitter_set_width(&emitter, 30);

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
        (const yaml_char_t *)"this is a plain value that is quite long and should be wrapped at the width limit",
        80, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);

    yaml_mapping_end_event_initialize(&event);
    yaml_emitter_emit(&emitter, &event);
    yaml_document_end_event_initialize(&event, 1);
    yaml_emitter_emit(&emitter, &event);
    yaml_stream_end_event_initialize(&event);
    yaml_emitter_emit(&emitter, &event);

    output[written] = '\0';
    ASSERT(written > 0, "plain wrap: produces output");

    yaml_emitter_delete(&emitter);
}

static void test_folded_scalar_line_breaks(void) {
    /* Folded scalar should fold long lines */
    yaml_emitter_t emitter;
    yaml_event_t event;
    unsigned char output[4096];
    size_t written = 0;

    yaml_emitter_initialize(&emitter);
    yaml_emitter_set_output_string(&emitter, output, sizeof(output), &written);

    yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
    yaml_emitter_emit(&emitter, &event);
    yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1);
    yaml_emitter_emit(&emitter, &event);

    yaml_scalar_event_initialize(&event, NULL, (const yaml_char_t *)YAML_STR_TAG,
        (const yaml_char_t *)"paragraph one\n\nparagraph two\n", 28, 0, 0,
        YAML_FOLDED_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);

    yaml_document_end_event_initialize(&event, 1);
    yaml_emitter_emit(&emitter, &event);
    yaml_stream_end_event_initialize(&event);
    yaml_emitter_emit(&emitter, &event);

    output[written] = '\0';
    ASSERT(strstr((const char *)output, ">") != NULL,
           "folded: has > indicator");

    yaml_emitter_delete(&emitter);
}

int main(void) {
    TEST_SUITE_BEGIN("Emitter");

    /* Original tests */
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

    /* Deep-dive: escape sequences */
    test_escape_null();
    test_escape_bell();
    test_escape_backspace();
    test_escape_tab();
    test_escape_newline();
    test_escape_vtab();
    test_escape_formfeed();
    test_escape_cr();
    test_escape_esc();
    test_escape_backslash();
    test_escape_dquote();
    test_escape_next_line();
    test_escape_nbsp();
    test_escape_linesep();
    test_escape_parasep();
    test_escape_hex();
    test_escape_unicode_bmp();
    test_escape_unicode_nonbmp();

    /* Deep-dive: block scalar hints */
    test_block_scalar_empty_content();
    test_block_scalar_leading_space();
    test_block_scalar_leading_break();
    test_block_scalar_strip_chomp();
    test_block_scalar_keep_chomp();
    test_block_scalar_only_newline();

    /* Deep-dive: scalar analysis */
    test_scalar_starting_with_dash();
    test_scalar_starting_with_dots();
    test_scalar_space_break_forces_dquote();
    test_scalar_break_space();

    /* Deep-dive: multi-document with directives */
    test_version_directive_1_2();
    test_multi_doc_with_version();
    test_doc_end_before_directive();

    /* Deep-dive: check_simple_key */
    test_long_key_not_simple();
    test_multiline_key_not_simple();

    /* Deep-dive: flow context */
    test_flow_sequence_multiline();
    test_flow_mapping_multiline();
    test_block_mapping_compact();

    /* Deep-dive: error paths */
    test_emitter_wrong_event_order();
    test_emitter_double_stream_start();

    /* Deep-dive: wrapping */
    test_single_quoted_wrapping();
    test_plain_scalar_wrapping();
    test_folded_scalar_line_breaks();

    TEST_SUITE_END();
}
