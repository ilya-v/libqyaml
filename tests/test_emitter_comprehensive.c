#include "test_helper.h"

/*
 * Comprehensive emitter tests covering all event types, styles, settings,
 * and the emit-then-parse roundtrip for various YAML constructs.
 */

/* Helper: emit events, capture output, then parse and verify */
struct output_buffer {
    unsigned char *data;
    size_t len;
    size_t cap;
};

static int buffer_write_handler(void *data, unsigned char *buffer, size_t size) {
    struct output_buffer *buf = data;
    if (buf->len + size > buf->cap) {
        size_t new_cap = (buf->cap + size) * 2;
        unsigned char *new_data = realloc(buf->data, new_cap);
        if (!new_data) return 0;
        buf->data = new_data;
        buf->cap = new_cap;
    }
    memcpy(buf->data + buf->len, buffer, size);
    buf->len += size;
    return 1;
}

static void buffer_init(struct output_buffer *buf) {
    buf->data = malloc(1024);
    buf->len = 0;
    buf->cap = 1024;
}

static void buffer_free(struct output_buffer *buf) {
    free(buf->data);
}

/* Helper: emit a simple scalar document and return the output */
static int emit_scalar(const char *value, yaml_scalar_style_t style,
                        struct output_buffer *out) {
    yaml_emitter_t emitter;
    yaml_event_t event;

    yaml_emitter_initialize(&emitter);
    yaml_emitter_set_output(&emitter, buffer_write_handler, out);
    yaml_emitter_set_unicode(&emitter, 1);

    yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
    if (!yaml_emitter_emit(&emitter, &event)) goto fail;

    yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1);
    if (!yaml_emitter_emit(&emitter, &event)) goto fail;

    yaml_scalar_event_initialize(&event, NULL, NULL,
        (yaml_char_t *)value, strlen(value), 1, 1, style);
    if (!yaml_emitter_emit(&emitter, &event)) goto fail;

    yaml_document_end_event_initialize(&event, 1);
    if (!yaml_emitter_emit(&emitter, &event)) goto fail;

    yaml_stream_end_event_initialize(&event);
    if (!yaml_emitter_emit(&emitter, &event)) goto fail;

    yaml_emitter_delete(&emitter);
    return 1;

fail:
    yaml_emitter_delete(&emitter);
    return 0;
}

/* Helper: parse output and find scalar value */
static int output_contains_scalar(struct output_buffer *out, const char *expected) {
    yaml_parser_t parser;
    yaml_event_t event;
    int found = 0;

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, out->data, out->len);

    while (1) {
        if (!yaml_parser_parse(&parser, &event)) break;
        if (event.type == YAML_SCALAR_EVENT) {
            if (strcmp((const char *)event.data.scalar.value, expected) == 0)
                found = 1;
        }
        int done = (event.type == YAML_STREAM_END_EVENT);
        yaml_event_delete(&event);
        if (done) break;
    }
    yaml_parser_delete(&parser);
    return found;
}

/* ========== Scalar style emission ========== */

static void test_e_plain_scalar(void) {
    struct output_buffer out;
    buffer_init(&out);
    ASSERT(emit_scalar("hello", YAML_PLAIN_SCALAR_STYLE, &out), "emit plain");
    ASSERT(output_contains_scalar(&out, "hello"), "rt plain");
    buffer_free(&out);
}

static void test_e_single_quoted_scalar(void) {
    struct output_buffer out;
    buffer_init(&out);
    ASSERT(emit_scalar("hello", YAML_SINGLE_QUOTED_SCALAR_STYLE, &out), "emit single");
    ASSERT(output_contains_scalar(&out, "hello"), "rt single");
    buffer_free(&out);
}

static void test_e_double_quoted_scalar(void) {
    struct output_buffer out;
    buffer_init(&out);
    ASSERT(emit_scalar("hello", YAML_DOUBLE_QUOTED_SCALAR_STYLE, &out), "emit double");
    ASSERT(output_contains_scalar(&out, "hello"), "rt double");
    buffer_free(&out);
}

static void test_e_literal_scalar(void) {
    struct output_buffer out;
    buffer_init(&out);
    ASSERT(emit_scalar("hello\nworld\n", YAML_LITERAL_SCALAR_STYLE, &out), "emit literal");
    ASSERT(output_contains_scalar(&out, "hello\nworld\n"), "rt literal");
    buffer_free(&out);
}

static void test_e_folded_scalar(void) {
    struct output_buffer out;
    buffer_init(&out);
    ASSERT(emit_scalar("hello\nworld\n", YAML_FOLDED_SCALAR_STYLE, &out), "emit folded");
    ASSERT(output_contains_scalar(&out, "hello\nworld\n"), "rt folded");
    buffer_free(&out);
}

static void test_e_any_scalar(void) {
    struct output_buffer out;
    buffer_init(&out);
    ASSERT(emit_scalar("hello", YAML_ANY_SCALAR_STYLE, &out), "emit any");
    ASSERT(output_contains_scalar(&out, "hello"), "rt any");
    buffer_free(&out);
}

/* ========== Special scalar values ========== */

static void test_e_empty_scalar(void) {
    struct output_buffer out;
    buffer_init(&out);
    ASSERT(emit_scalar("", YAML_SINGLE_QUOTED_SCALAR_STYLE, &out), "emit empty");
    ASSERT(output_contains_scalar(&out, ""), "rt empty");
    buffer_free(&out);
}

static void test_e_scalar_with_newline(void) {
    struct output_buffer out;
    buffer_init(&out);
    ASSERT(emit_scalar("line1\nline2", YAML_DOUBLE_QUOTED_SCALAR_STYLE, &out), "emit newline");
    ASSERT(output_contains_scalar(&out, "line1\nline2"), "rt newline");
    buffer_free(&out);
}

static void test_e_scalar_with_tab(void) {
    struct output_buffer out;
    buffer_init(&out);
    ASSERT(emit_scalar("a\tb", YAML_DOUBLE_QUOTED_SCALAR_STYLE, &out), "emit tab");
    ASSERT(output_contains_scalar(&out, "a\tb"), "rt tab");
    buffer_free(&out);
}

static void test_e_scalar_with_special_chars(void) {
    struct output_buffer out;
    buffer_init(&out);
    ASSERT(emit_scalar("a: b", YAML_DOUBLE_QUOTED_SCALAR_STYLE, &out), "emit colon");
    ASSERT(output_contains_scalar(&out, "a: b"), "rt colon");
    buffer_free(&out);
}

static void test_e_scalar_with_hash(void) {
    struct output_buffer out;
    buffer_init(&out);
    ASSERT(emit_scalar("a # b", YAML_DOUBLE_QUOTED_SCALAR_STYLE, &out), "emit hash");
    ASSERT(output_contains_scalar(&out, "a # b"), "rt hash");
    buffer_free(&out);
}

static void test_e_scalar_with_quotes(void) {
    struct output_buffer out;
    buffer_init(&out);
    ASSERT(emit_scalar("he said \"hi\"", YAML_DOUBLE_QUOTED_SCALAR_STYLE, &out), "emit quotes");
    ASSERT(output_contains_scalar(&out, "he said \"hi\""), "rt quotes");
    buffer_free(&out);
}

static void test_e_scalar_with_backslash(void) {
    struct output_buffer out;
    buffer_init(&out);
    ASSERT(emit_scalar("a\\b", YAML_DOUBLE_QUOTED_SCALAR_STYLE, &out), "emit backslash");
    ASSERT(output_contains_scalar(&out, "a\\b"), "rt backslash");
    buffer_free(&out);
}

static void test_e_scalar_number(void) {
    struct output_buffer out;
    buffer_init(&out);
    ASSERT(emit_scalar("42", YAML_PLAIN_SCALAR_STYLE, &out), "emit number");
    ASSERT(output_contains_scalar(&out, "42"), "rt number");
    buffer_free(&out);
}

static void test_e_scalar_bool_true(void) {
    struct output_buffer out;
    buffer_init(&out);
    ASSERT(emit_scalar("true", YAML_PLAIN_SCALAR_STYLE, &out), "emit true");
    ASSERT(output_contains_scalar(&out, "true"), "rt true");
    buffer_free(&out);
}

static void test_e_scalar_null(void) {
    struct output_buffer out;
    buffer_init(&out);
    ASSERT(emit_scalar("null", YAML_PLAIN_SCALAR_STYLE, &out), "emit null");
    ASSERT(output_contains_scalar(&out, "null"), "rt null");
    buffer_free(&out);
}

/* ========== Mapping emission ========== */

static void test_e_block_mapping(void) {
    struct output_buffer out;
    buffer_init(&out);

    yaml_emitter_t emitter;
    yaml_event_t event;
    yaml_emitter_initialize(&emitter);
    yaml_emitter_set_output(&emitter, buffer_write_handler, &out);

    yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
    yaml_emitter_emit(&emitter, &event);
    yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1);
    yaml_emitter_emit(&emitter, &event);

    yaml_mapping_start_event_initialize(&event, NULL, NULL, 1, YAML_BLOCK_MAPPING_STYLE);
    yaml_emitter_emit(&emitter, &event);

    yaml_scalar_event_initialize(&event, NULL, NULL, (yaml_char_t *)"key", 3, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);
    yaml_scalar_event_initialize(&event, NULL, NULL, (yaml_char_t *)"val", 3, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);

    yaml_mapping_end_event_initialize(&event);
    yaml_emitter_emit(&emitter, &event);

    yaml_document_end_event_initialize(&event, 1);
    yaml_emitter_emit(&emitter, &event);
    yaml_stream_end_event_initialize(&event);
    yaml_emitter_emit(&emitter, &event);

    yaml_emitter_delete(&emitter);

    ASSERT(output_contains_scalar(&out, "key"), "block map: key");
    ASSERT(output_contains_scalar(&out, "val"), "block map: val");
    buffer_free(&out);
}

static void test_e_flow_mapping(void) {
    struct output_buffer out;
    buffer_init(&out);

    yaml_emitter_t emitter;
    yaml_event_t event;
    yaml_emitter_initialize(&emitter);
    yaml_emitter_set_output(&emitter, buffer_write_handler, &out);

    yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
    yaml_emitter_emit(&emitter, &event);
    yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1);
    yaml_emitter_emit(&emitter, &event);

    yaml_mapping_start_event_initialize(&event, NULL, NULL, 1, YAML_FLOW_MAPPING_STYLE);
    yaml_emitter_emit(&emitter, &event);

    yaml_scalar_event_initialize(&event, NULL, NULL, (yaml_char_t *)"a", 1, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);
    yaml_scalar_event_initialize(&event, NULL, NULL, (yaml_char_t *)"1", 1, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);

    yaml_mapping_end_event_initialize(&event);
    yaml_emitter_emit(&emitter, &event);

    yaml_document_end_event_initialize(&event, 1);
    yaml_emitter_emit(&emitter, &event);
    yaml_stream_end_event_initialize(&event);
    yaml_emitter_emit(&emitter, &event);

    yaml_emitter_delete(&emitter);

    ASSERT(output_contains_scalar(&out, "a"), "flow map: key");
    ASSERT(output_contains_scalar(&out, "1"), "flow map: val");
    buffer_free(&out);
}

/* ========== Sequence emission ========== */

static void test_e_block_sequence(void) {
    struct output_buffer out;
    buffer_init(&out);

    yaml_emitter_t emitter;
    yaml_event_t event;
    yaml_emitter_initialize(&emitter);
    yaml_emitter_set_output(&emitter, buffer_write_handler, &out);

    yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
    yaml_emitter_emit(&emitter, &event);
    yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1);
    yaml_emitter_emit(&emitter, &event);

    yaml_sequence_start_event_initialize(&event, NULL, NULL, 1, YAML_BLOCK_SEQUENCE_STYLE);
    yaml_emitter_emit(&emitter, &event);

    yaml_scalar_event_initialize(&event, NULL, NULL, (yaml_char_t *)"a", 1, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);
    yaml_scalar_event_initialize(&event, NULL, NULL, (yaml_char_t *)"b", 1, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);

    yaml_sequence_end_event_initialize(&event);
    yaml_emitter_emit(&emitter, &event);

    yaml_document_end_event_initialize(&event, 1);
    yaml_emitter_emit(&emitter, &event);
    yaml_stream_end_event_initialize(&event);
    yaml_emitter_emit(&emitter, &event);

    yaml_emitter_delete(&emitter);

    ASSERT(output_contains_scalar(&out, "a"), "block seq: a");
    ASSERT(output_contains_scalar(&out, "b"), "block seq: b");
    buffer_free(&out);
}

static void test_e_flow_sequence(void) {
    struct output_buffer out;
    buffer_init(&out);

    yaml_emitter_t emitter;
    yaml_event_t event;
    yaml_emitter_initialize(&emitter);
    yaml_emitter_set_output(&emitter, buffer_write_handler, &out);

    yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
    yaml_emitter_emit(&emitter, &event);
    yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1);
    yaml_emitter_emit(&emitter, &event);

    yaml_sequence_start_event_initialize(&event, NULL, NULL, 1, YAML_FLOW_SEQUENCE_STYLE);
    yaml_emitter_emit(&emitter, &event);

    yaml_scalar_event_initialize(&event, NULL, NULL, (yaml_char_t *)"x", 1, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);
    yaml_scalar_event_initialize(&event, NULL, NULL, (yaml_char_t *)"y", 1, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);

    yaml_sequence_end_event_initialize(&event);
    yaml_emitter_emit(&emitter, &event);

    yaml_document_end_event_initialize(&event, 1);
    yaml_emitter_emit(&emitter, &event);
    yaml_stream_end_event_initialize(&event);
    yaml_emitter_emit(&emitter, &event);

    yaml_emitter_delete(&emitter);

    ASSERT(output_contains_scalar(&out, "x"), "flow seq: x");
    ASSERT(output_contains_scalar(&out, "y"), "flow seq: y");
    buffer_free(&out);
}

/* ========== Emitter settings ========== */

static void test_e_canonical_mode(void) {
    struct output_buffer out;
    buffer_init(&out);

    yaml_emitter_t emitter;
    yaml_event_t event;
    yaml_emitter_initialize(&emitter);
    yaml_emitter_set_output(&emitter, buffer_write_handler, &out);
    yaml_emitter_set_canonical(&emitter, 1);

    yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
    yaml_emitter_emit(&emitter, &event);
    yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1);
    yaml_emitter_emit(&emitter, &event);
    yaml_scalar_event_initialize(&event, NULL, NULL, (yaml_char_t *)"test", 4, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);
    yaml_document_end_event_initialize(&event, 1);
    yaml_emitter_emit(&emitter, &event);
    yaml_stream_end_event_initialize(&event);
    yaml_emitter_emit(&emitter, &event);

    yaml_emitter_delete(&emitter);
    ASSERT(output_contains_scalar(&out, "test"), "canonical rt");
    buffer_free(&out);
}

static void test_e_custom_indent(void) {
    struct output_buffer out;
    buffer_init(&out);

    yaml_emitter_t emitter;
    yaml_event_t event;
    yaml_emitter_initialize(&emitter);
    yaml_emitter_set_output(&emitter, buffer_write_handler, &out);
    yaml_emitter_set_indent(&emitter, 4);

    yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
    yaml_emitter_emit(&emitter, &event);
    yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1);
    yaml_emitter_emit(&emitter, &event);

    yaml_mapping_start_event_initialize(&event, NULL, NULL, 1, YAML_BLOCK_MAPPING_STYLE);
    yaml_emitter_emit(&emitter, &event);
    yaml_scalar_event_initialize(&event, NULL, NULL, (yaml_char_t *)"key", 3, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);
    yaml_scalar_event_initialize(&event, NULL, NULL, (yaml_char_t *)"val", 3, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);
    yaml_mapping_end_event_initialize(&event);
    yaml_emitter_emit(&emitter, &event);

    yaml_document_end_event_initialize(&event, 1);
    yaml_emitter_emit(&emitter, &event);
    yaml_stream_end_event_initialize(&event);
    yaml_emitter_emit(&emitter, &event);

    yaml_emitter_delete(&emitter);
    ASSERT(output_contains_scalar(&out, "val"), "indent 4 rt");
    buffer_free(&out);
}

/* ========== Anchor emission ========== */

static void test_e_anchor_scalar(void) {
    struct output_buffer out;
    buffer_init(&out);

    yaml_emitter_t emitter;
    yaml_event_t event;
    yaml_emitter_initialize(&emitter);
    yaml_emitter_set_output(&emitter, buffer_write_handler, &out);

    yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
    yaml_emitter_emit(&emitter, &event);
    yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1);
    yaml_emitter_emit(&emitter, &event);

    yaml_sequence_start_event_initialize(&event, NULL, NULL, 1, YAML_BLOCK_SEQUENCE_STYLE);
    yaml_emitter_emit(&emitter, &event);

    yaml_scalar_event_initialize(&event, (yaml_char_t *)"ref", NULL,
        (yaml_char_t *)"hello", 5, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);

    yaml_alias_event_initialize(&event, (yaml_char_t *)"ref");
    yaml_emitter_emit(&emitter, &event);

    yaml_sequence_end_event_initialize(&event);
    yaml_emitter_emit(&emitter, &event);
    yaml_document_end_event_initialize(&event, 1);
    yaml_emitter_emit(&emitter, &event);
    yaml_stream_end_event_initialize(&event);
    yaml_emitter_emit(&emitter, &event);

    yaml_emitter_delete(&emitter);
    ASSERT(output_contains_scalar(&out, "hello"), "anchor rt");
    buffer_free(&out);
}

/* ========== Tag emission ========== */

static void test_e_tagged_scalar(void) {
    struct output_buffer out;
    buffer_init(&out);

    yaml_emitter_t emitter;
    yaml_event_t event;
    yaml_emitter_initialize(&emitter);
    yaml_emitter_set_output(&emitter, buffer_write_handler, &out);

    yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
    yaml_emitter_emit(&emitter, &event);
    yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1);
    yaml_emitter_emit(&emitter, &event);

    yaml_scalar_event_initialize(&event, NULL,
        (yaml_char_t *)"tag:yaml.org,2002:str",
        (yaml_char_t *)"hello", 5, 0, 0, YAML_PLAIN_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);

    yaml_document_end_event_initialize(&event, 1);
    yaml_emitter_emit(&emitter, &event);
    yaml_stream_end_event_initialize(&event);
    yaml_emitter_emit(&emitter, &event);

    yaml_emitter_delete(&emitter);
    ASSERT(output_contains_scalar(&out, "hello"), "tagged rt");
    buffer_free(&out);
}

/* ========== Multiple documents ========== */

static void test_e_multiple_docs(void) {
    struct output_buffer out;
    buffer_init(&out);

    yaml_emitter_t emitter;
    yaml_event_t event;
    yaml_emitter_initialize(&emitter);
    yaml_emitter_set_output(&emitter, buffer_write_handler, &out);

    yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
    yaml_emitter_emit(&emitter, &event);

    for (int i = 0; i < 3; i++) {
        yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 0);
        yaml_emitter_emit(&emitter, &event);
        char buf[8];
        snprintf(buf, sizeof(buf), "doc%d", i);
        yaml_scalar_event_initialize(&event, NULL, NULL,
            (yaml_char_t *)buf, strlen(buf), 1, 1, YAML_PLAIN_SCALAR_STYLE);
        yaml_emitter_emit(&emitter, &event);
        yaml_document_end_event_initialize(&event, 0);
        yaml_emitter_emit(&emitter, &event);
    }

    yaml_stream_end_event_initialize(&event);
    yaml_emitter_emit(&emitter, &event);

    yaml_emitter_delete(&emitter);

    ASSERT(output_contains_scalar(&out, "doc0"), "multi doc: doc0");
    ASSERT(output_contains_scalar(&out, "doc1"), "multi doc: doc1");
    ASSERT(output_contains_scalar(&out, "doc2"), "multi doc: doc2");
    buffer_free(&out);
}

/* ========== Explicit document markers ========== */

static void test_e_explicit_doc_markers(void) {
    struct output_buffer out;
    buffer_init(&out);

    yaml_emitter_t emitter;
    yaml_event_t event;
    yaml_emitter_initialize(&emitter);
    yaml_emitter_set_output(&emitter, buffer_write_handler, &out);

    yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
    yaml_emitter_emit(&emitter, &event);

    yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 0); /* explicit */
    yaml_emitter_emit(&emitter, &event);
    yaml_scalar_event_initialize(&event, NULL, NULL, (yaml_char_t *)"x", 1, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);
    yaml_document_end_event_initialize(&event, 0); /* explicit */
    yaml_emitter_emit(&emitter, &event);

    yaml_stream_end_event_initialize(&event);
    yaml_emitter_emit(&emitter, &event);

    yaml_emitter_delete(&emitter);

    /* Output should contain --- and ... */
    ASSERT(out.len > 0, "explicit markers: output produced");
    ASSERT(output_contains_scalar(&out, "x"), "explicit markers: rt");
    buffer_free(&out);
}

/* ========== File output ========== */

static void test_e_file_output(void) {
    FILE *f = tmpfile();
    ASSERT_NOT_NULL(f, "tmpfile");
    if (!f) return;

    yaml_emitter_t emitter;
    yaml_event_t event;
    yaml_emitter_initialize(&emitter);
    yaml_emitter_set_output_file(&emitter, f);

    yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
    yaml_emitter_emit(&emitter, &event);
    yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1);
    yaml_emitter_emit(&emitter, &event);
    yaml_scalar_event_initialize(&event, NULL, NULL, (yaml_char_t *)"file_test", 9, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);
    yaml_document_end_event_initialize(&event, 1);
    yaml_emitter_emit(&emitter, &event);
    yaml_stream_end_event_initialize(&event);
    yaml_emitter_emit(&emitter, &event);

    yaml_emitter_delete(&emitter);

    /* Read back and parse */
    rewind(f);
    yaml_parser_t parser;
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_file(&parser, f);

    int found = 0;
    while (1) {
        yaml_event_t ev;
        if (!yaml_parser_parse(&parser, &ev)) break;
        if (ev.type == YAML_SCALAR_EVENT &&
            strcmp((const char *)ev.data.scalar.value, "file_test") == 0)
            found = 1;
        int done = (ev.type == YAML_STREAM_END_EVENT);
        yaml_event_delete(&ev);
        if (done) break;
    }
    yaml_parser_delete(&parser);
    fclose(f);
    ASSERT(found, "file output rt");
}

/* ========== UTF-8 emission ========== */

static void test_e_utf8_scalar(void) {
    struct output_buffer out;
    buffer_init(&out);
    ASSERT(emit_scalar("\xc3\xa9l\xc3\xa8ve", YAML_PLAIN_SCALAR_STYLE, &out), "emit utf8");
    ASSERT(output_contains_scalar(&out, "\xc3\xa9l\xc3\xa8ve"), "rt utf8");
    buffer_free(&out);
}

static void test_e_utf8_emoji(void) {
    struct output_buffer out;
    buffer_init(&out);
    ASSERT(emit_scalar("\xf0\x9f\x98\x80", YAML_DOUBLE_QUOTED_SCALAR_STYLE, &out), "emit emoji");
    ASSERT(output_contains_scalar(&out, "\xf0\x9f\x98\x80"), "rt emoji");
    buffer_free(&out);
}

int main(void) {
    TEST_SUITE_BEGIN("Emitter Comprehensive");

    /* Scalar styles */
    test_e_plain_scalar();
    test_e_single_quoted_scalar();
    test_e_double_quoted_scalar();
    test_e_literal_scalar();
    test_e_folded_scalar();
    test_e_any_scalar();

    /* Special scalar values */
    test_e_empty_scalar();
    test_e_scalar_with_newline();
    test_e_scalar_with_tab();
    test_e_scalar_with_special_chars();
    test_e_scalar_with_hash();
    test_e_scalar_with_quotes();
    test_e_scalar_with_backslash();
    test_e_scalar_number();
    test_e_scalar_bool_true();
    test_e_scalar_null();

    /* Mappings */
    test_e_block_mapping();
    test_e_flow_mapping();

    /* Sequences */
    test_e_block_sequence();
    test_e_flow_sequence();

    /* Settings */
    test_e_canonical_mode();
    test_e_custom_indent();

    /* Anchors */
    test_e_anchor_scalar();

    /* Tags */
    test_e_tagged_scalar();

    /* Multiple documents */
    test_e_multiple_docs();

    /* Explicit markers */
    test_e_explicit_doc_markers();

    /* File output */
    test_e_file_output();

    /* UTF-8 */
    test_e_utf8_scalar();
    test_e_utf8_emoji();

    TEST_SUITE_END();
}
