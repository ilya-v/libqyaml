#include "test_helper.h"

/* Test empty values in mappings */
static void test_empty_values(void) {
    yaml_event_type_t events[32];
    int count;

    /* Empty value */
    ASSERT(parse_string_events("key:\n", events, 32, &count), "empty val");
    int scalar_count = 0;
    for (int i = 0; i < count; i++) {
        if (events[i] == YAML_SCALAR_EVENT) scalar_count++;
    }
    ASSERT_EQ_INT(scalar_count, 2, "empty val: 2 scalars (key + empty)");
}

/* Test complex keys */
static void test_complex_keys(void) {
    yaml_event_type_t events[32];
    int count;

    /* Question mark key */
    ASSERT(parse_string_events("? key\n: value\n", events, 32, &count), "complex key");
    int found_map = 0;
    for (int i = 0; i < count; i++) {
        if (events[i] == YAML_MAPPING_START_EVENT) found_map = 1;
    }
    ASSERT(found_map, "complex key: has mapping");
}

/* Test nested flow collections */
static void test_nested_flow(void) {
    yaml_event_type_t events[64];
    int count;

    ASSERT(parse_string_events("[[1, 2], [3, 4]]", events, 64, &count),
           "nested flow seq");
    int seq_start = 0, seq_end = 0;
    for (int i = 0; i < count; i++) {
        if (events[i] == YAML_SEQUENCE_START_EVENT) seq_start++;
        if (events[i] == YAML_SEQUENCE_END_EVENT) seq_end++;
    }
    ASSERT_EQ_INT(seq_start, 3, "nested flow: 3 seq starts");
    ASSERT_EQ_INT(seq_end, 3, "nested flow: 3 seq ends");

    /* Nested flow mappings */
    ASSERT(parse_string_events("{a: {b: c}}", events, 64, &count),
           "nested flow map");
    int map_start = 0;
    for (int i = 0; i < count; i++) {
        if (events[i] == YAML_MAPPING_START_EVENT) map_start++;
    }
    ASSERT_EQ_INT(map_start, 2, "nested flow map: 2 map starts");
}

/* Test mixed flow and block */
static void test_mixed_styles(void) {
    yaml_event_type_t events[64];
    int count;

    const char *yaml =
        "block_key: [flow, sequence]\n"
        "another: {flow: mapping}\n";

    ASSERT(parse_string_events(yaml, events, 64, &count), "mixed styles");
    int seq_start = 0, map_start = 0;
    for (int i = 0; i < count; i++) {
        if (events[i] == YAML_SEQUENCE_START_EVENT) seq_start++;
        if (events[i] == YAML_MAPPING_START_EVENT) map_start++;
    }
    ASSERT(seq_start >= 1, "mixed: has sequence");
    ASSERT(map_start >= 2, "mixed: has 2+ mappings");
}

/* Test multiple anchors/aliases */
static void test_multiple_anchors(void) {
    const char *yaml =
        "a: &first hello\n"
        "b: &second world\n"
        "c: *first\n"
        "d: *second\n";

    yaml_event_type_t events[64];
    int count;

    ASSERT(parse_string_events(yaml, events, 64, &count), "multi anchor");
    int alias_count = 0;
    for (int i = 0; i < count; i++) {
        if (events[i] == YAML_ALIAS_EVENT) alias_count++;
    }
    ASSERT_EQ_INT(alias_count, 2, "multi anchor: 2 aliases");
}

/* Test document separators */
static void test_document_separators(void) {
    yaml_event_type_t events[64];
    int count;

    /* Three documents */
    const char *yaml = "---\na\n---\nb\n---\nc\n...\n";
    ASSERT(parse_string_events(yaml, events, 64, &count), "3 docs");
    int doc_start = 0;
    for (int i = 0; i < count; i++) {
        if (events[i] == YAML_DOCUMENT_START_EVENT) doc_start++;
    }
    ASSERT_EQ_INT(doc_start, 3, "3 docs: 3 starts");
}

/* Test indentation edge cases */
static void test_indentation(void) {
    yaml_event_type_t events[64];
    int count;

    /* Two-space indentation */
    const char *yaml1 =
        "a:\n"
        "  b:\n"
        "    c: d\n";
    ASSERT(parse_string_events(yaml1, events, 64, &count), "indent 2");

    /* Mixed indentation */
    const char *yaml2 =
        "a:\n"
        "  b: 1\n"
        "  c:\n"
        "    d: 2\n";
    ASSERT(parse_string_events(yaml2, events, 64, &count), "indent mixed");
}

/* Test comments in various positions */
static void test_comments(void) {
    yaml_event_type_t events[64];
    int count;

    /* Comment on its own line */
    ASSERT(parse_string_events("# comment\nkey: value\n", events, 64, &count),
           "comment: own line");

    /* Inline comment */
    ASSERT(parse_string_events("key: value # comment\n", events, 64, &count),
           "comment: inline");

    /* Comment after document start */
    ASSERT(parse_string_events("---\n# comment\nkey: value\n", events, 64, &count),
           "comment: after doc start");

    /* Multiple comments */
    ASSERT(parse_string_events("# c1\n# c2\nkey: value\n", events, 64, &count),
           "comment: multiple");
}

/* Test parser init/delete cycles */
static void test_parser_lifecycle(void) {
    /* Multiple init/delete cycles */
    for (int i = 0; i < 100; i++) {
        yaml_parser_t parser;
        ASSERT(yaml_parser_initialize(&parser), "lifecycle: init");
        yaml_parser_set_input_string(&parser, (const unsigned char *)"test", 4);
        yaml_parser_delete(&parser);
    }
    ASSERT(1, "lifecycle: 100 cycles OK");
}

/* Test emitter init/delete cycles */
static void test_emitter_lifecycle(void) {
    for (int i = 0; i < 100; i++) {
        yaml_emitter_t emitter;
        ASSERT(yaml_emitter_initialize(&emitter), "emitter lifecycle: init");
        yaml_emitter_delete(&emitter);
    }
    ASSERT(1, "emitter lifecycle: 100 cycles OK");
}

/* Test various null/edge-case inputs */
static void test_null_edge_cases(void) {
    yaml_parser_t parser;
    yaml_event_t event;

    /* Single newline */
    ASSERT(yaml_parser_initialize(&parser), "null: newline init");
    yaml_parser_set_input_string(&parser, (const unsigned char *)"\n", 1);
    ASSERT(yaml_parser_parse(&parser, &event), "null: newline parse");
    yaml_event_delete(&event);
    yaml_parser_delete(&parser);

    /* Only whitespace */
    ASSERT(yaml_parser_initialize(&parser), "null: whitespace init");
    yaml_parser_set_input_string(&parser, (const unsigned char *)"   \n  \n", 7);
    ASSERT(yaml_parser_parse(&parser, &event), "null: whitespace parse");
    yaml_event_delete(&event);
    yaml_parser_delete(&parser);

    /* Only comments */
    ASSERT(yaml_parser_initialize(&parser), "null: comments init");
    yaml_parser_set_input_string(&parser, (const unsigned char *)"# comment\n", 10);
    ASSERT(yaml_parser_parse(&parser, &event), "null: comments parse");
    yaml_event_delete(&event);
    yaml_parser_delete(&parser);
}

/* Test sequences of mappings */
static void test_sequence_of_mappings(void) {
    const char *yaml =
        "- name: a\n"
        "  value: 1\n"
        "- name: b\n"
        "  value: 2\n";

    yaml_event_type_t events[64];
    int count;

    ASSERT(parse_string_events(yaml, events, 64, &count), "seq of maps");
    int seq = 0, map = 0;
    for (int i = 0; i < count; i++) {
        if (events[i] == YAML_SEQUENCE_START_EVENT) seq++;
        if (events[i] == YAML_MAPPING_START_EVENT) map++;
    }
    ASSERT_EQ_INT(seq, 1, "seq of maps: 1 sequence");
    ASSERT_EQ_INT(map, 2, "seq of maps: 2 mappings");
}

/* Test mapping of sequences */
static void test_mapping_of_sequences(void) {
    const char *yaml =
        "fruits:\n"
        "  - apple\n"
        "  - banana\n"
        "vegetables:\n"
        "  - carrot\n"
        "  - pea\n";

    yaml_event_type_t events[64];
    int count;

    ASSERT(parse_string_events(yaml, events, 64, &count), "map of seqs");
    int seq = 0, map = 0;
    for (int i = 0; i < count; i++) {
        if (events[i] == YAML_SEQUENCE_START_EVENT) seq++;
        if (events[i] == YAML_MAPPING_START_EVENT) map++;
    }
    ASSERT_EQ_INT(seq, 2, "map of seqs: 2 sequences");
    ASSERT_EQ_INT(map, 1, "map of seqs: 1 mapping");
}

/* Test token deletion */
static void test_token_delete(void) {
    yaml_token_t token;
    memset(&token, 0, sizeof(token));
    token.type = YAML_NO_TOKEN;
    yaml_token_delete(&token); /* Should not crash */
    ASSERT(1, "token delete: no crash on empty");
}

/* Test event deletion */
static void test_event_delete(void) {
    yaml_event_t event;
    memset(&event, 0, sizeof(event));
    event.type = YAML_NO_EVENT;
    yaml_event_delete(&event); /* Should not crash */
    ASSERT(1, "event delete: no crash on empty");
}

/* Test indentless sequences */
static void test_indentless_sequence(void) {
    const char *yaml =
        "key:\n"
        "- item1\n"
        "- item2\n";

    yaml_event_type_t events[32];
    int count;
    ASSERT(parse_string_events(yaml, events, 32, &count), "indentless seq");
    int seq_count = 0;
    for (int i = 0; i < count; i++) {
        if (events[i] == YAML_SEQUENCE_START_EVENT) seq_count++;
    }
    ASSERT(seq_count >= 1, "indentless seq: found sequence");
}

/* Test tab characters in certain positions */
static void test_tabs_handling(void) {
    /* Tabs are allowed in flow scalars */
    yaml_event_type_t events[32];
    int count;
    ASSERT(parse_string_events("\"hello\\tworld\"", events, 32, &count), "tabs: in dquote");
}

/* Test very long key name */
static void test_long_key(void) {
    char yaml[2048];
    memset(yaml, 'k', 1000);
    yaml[1000] = ':';
    yaml[1001] = ' ';
    yaml[1002] = 'v';
    yaml[1003] = '\n';
    yaml[1004] = '\0';

    yaml_event_type_t events[32];
    int count;
    ASSERT(parse_string_events(yaml, events, 32, &count), "long key: parse");
}

/* ==================================================================
 * Additional edge cases for coverage
 * ================================================================== */

static void test_empty_flow_collections(void) {
    yaml_event_type_t events[16];
    int count;

    ASSERT(parse_string_events("[]", events, 16, &count), "empty flow seq");
    int seq_start = 0, seq_end = 0;
    for (int i = 0; i < count; i++) {
        if (events[i] == YAML_SEQUENCE_START_EVENT) seq_start = 1;
        if (events[i] == YAML_SEQUENCE_END_EVENT) seq_end = 1;
    }
    ASSERT(seq_start && seq_end, "empty flow seq: found start+end");

    ASSERT(parse_string_events("{}", events, 16, &count), "empty flow map");
    int map_start = 0, map_end = 0;
    for (int i = 0; i < count; i++) {
        if (events[i] == YAML_MAPPING_START_EVENT) map_start = 1;
        if (events[i] == YAML_MAPPING_END_EVENT) map_end = 1;
    }
    ASSERT(map_start && map_end, "empty flow map: found start+end");
}

static void test_nested_empty_collections(void) {
    yaml_event_type_t events[32];
    int count;
    ASSERT(parse_string_events("[[], []]", events, 32, &count), "nested empty seqs");
    int seq_count = 0;
    for (int i = 0; i < count; i++)
        if (events[i] == YAML_SEQUENCE_START_EVENT) seq_count++;
    ASSERT_EQ_INT(seq_count, 3, "nested empty seqs: 3 seq starts");
}

static void test_deeply_nested_collections(void) {
    yaml_event_type_t events[64];
    int count;
    ASSERT(parse_string_events("[[[[a]]]]", events, 64, &count), "deep nested seqs");
    int seq_count = 0;
    for (int i = 0; i < count; i++)
        if (events[i] == YAML_SEQUENCE_START_EVENT) seq_count++;
    ASSERT_EQ_INT(seq_count, 4, "deep nested: 4 seq starts");
}

static void test_flow_map_nested_seq(void) {
    yaml_event_type_t events[32];
    int count;
    ASSERT(parse_string_events("{a: [1, 2], b: [3, 4]}", events, 32, &count),
           "flow map nested seq");
    int scalar_count = 0;
    for (int i = 0; i < count; i++)
        if (events[i] == YAML_SCALAR_EVENT) scalar_count++;
    ASSERT_EQ_INT(scalar_count, 6, "flow map nested seq: 6 scalars");
}

static void test_flow_seq_nested_map(void) {
    yaml_event_type_t events[32];
    int count;
    ASSERT(parse_string_events("[{a: 1}, {b: 2}]", events, 32, &count),
           "flow seq nested map");
    int map_count = 0;
    for (int i = 0; i < count; i++)
        if (events[i] == YAML_MAPPING_START_EVENT) map_count++;
    ASSERT_EQ_INT(map_count, 2, "flow seq nested map: 2 maps");
}

static void test_multiline_flow_seq(void) {
    yaml_event_type_t events[32];
    int count;
    ASSERT(parse_string_events("[\n  a,\n  b,\n  c\n]", events, 32, &count),
           "multiline flow seq");
    int scalar_count = 0;
    for (int i = 0; i < count; i++)
        if (events[i] == YAML_SCALAR_EVENT) scalar_count++;
    ASSERT_EQ_INT(scalar_count, 3, "multiline flow seq: 3 scalars");
}

static void test_multiline_flow_map(void) {
    yaml_event_type_t events[32];
    int count;
    ASSERT(parse_string_events("{\n  a: 1,\n  b: 2\n}", events, 32, &count),
           "multiline flow map");
    int scalar_count = 0;
    for (int i = 0; i < count; i++)
        if (events[i] == YAML_SCALAR_EVENT) scalar_count++;
    ASSERT_EQ_INT(scalar_count, 4, "multiline flow map: 4 scalars");
}

static void test_trailing_comma_flow(void) {
    yaml_event_type_t events[32];
    int count;
    /* Trailing comma in flow sequence */
    ASSERT(parse_string_events("[a, b, ]", events, 32, &count), "trailing comma seq");
}

static void test_block_scalar_empty_lines(void) {
    yaml_parser_t parser;
    yaml_event_t event;
    const char *yaml = "|\n  line1\n\n  line2\n";

    ASSERT(yaml_parser_initialize(&parser), "block empty lines init");
    yaml_parser_set_input_string(&parser, (const unsigned char *)yaml, strlen(yaml));

    int found = 0;
    while (1) {
        if (!yaml_parser_parse(&parser, &event)) break;
        if (event.type == YAML_SCALAR_EVENT) {
            found = 1;
            ASSERT_EQ_STR((const char *)event.data.scalar.value,
                         "line1\n\nline2\n", "block empty lines: value");
        }
        if (event.type == YAML_STREAM_END_EVENT) { yaml_event_delete(&event); break; }
        yaml_event_delete(&event);
    }
    ASSERT(found, "block empty lines: found");
    yaml_parser_delete(&parser);
}

static void test_folded_scalar_content(void) {
    yaml_parser_t parser;
    yaml_event_t event;
    const char *yaml = ">\n  word1 word2\n  word3 word4\n";

    ASSERT(yaml_parser_initialize(&parser), "folded content init");
    yaml_parser_set_input_string(&parser, (const unsigned char *)yaml, strlen(yaml));

    int found = 0;
    while (1) {
        if (!yaml_parser_parse(&parser, &event)) break;
        if (event.type == YAML_SCALAR_EVENT) {
            found = 1;
            ASSERT_EQ_STR((const char *)event.data.scalar.value,
                         "word1 word2 word3 word4\n", "folded content: value");
        }
        if (event.type == YAML_STREAM_END_EVENT) { yaml_event_delete(&event); break; }
        yaml_event_delete(&event);
    }
    ASSERT(found, "folded content: found");
    yaml_parser_delete(&parser);
}

static void test_multiple_scalars_in_mapping(void) {
    yaml_parser_t parser;
    yaml_event_t event;
    const char *yaml = "a: 1\nb: 2\nc: 3\nd: 4\ne: 5\n";

    ASSERT(yaml_parser_initialize(&parser), "multi scalar map init");
    yaml_parser_set_input_string(&parser, (const unsigned char *)yaml, strlen(yaml));

    int scalar_count = 0;
    while (1) {
        if (!yaml_parser_parse(&parser, &event)) break;
        if (event.type == YAML_SCALAR_EVENT) scalar_count++;
        if (event.type == YAML_STREAM_END_EVENT) { yaml_event_delete(&event); break; }
        yaml_event_delete(&event);
    }
    ASSERT_EQ_INT(scalar_count, 10, "multi scalar map: 10 scalars");
    yaml_parser_delete(&parser);
}

static void test_parser_reuse(void) {
    /* Parse, delete, reinitialize, parse again */
    yaml_parser_t parser;
    yaml_event_t event;

    ASSERT(yaml_parser_initialize(&parser), "reuse init 1");
    yaml_parser_set_input_string(&parser, (const unsigned char *)"hello\n", 6);
    while (1) {
        if (!yaml_parser_parse(&parser, &event)) break;
        if (event.type == YAML_STREAM_END_EVENT) { yaml_event_delete(&event); break; }
        yaml_event_delete(&event);
    }
    yaml_parser_delete(&parser);

    ASSERT(yaml_parser_initialize(&parser), "reuse init 2");
    yaml_parser_set_input_string(&parser, (const unsigned char *)"world\n", 6);
    int found = 0;
    while (1) {
        if (!yaml_parser_parse(&parser, &event)) break;
        if (event.type == YAML_SCALAR_EVENT) found = 1;
        if (event.type == YAML_STREAM_END_EVENT) { yaml_event_delete(&event); break; }
        yaml_event_delete(&event);
    }
    ASSERT(found, "reuse: found scalar in second parse");
    yaml_parser_delete(&parser);
}

static void test_emitter_reuse(void) {
    yaml_emitter_t emitter;
    unsigned char buffer[1024];
    size_t written;

    /* First use */
    ASSERT(yaml_emitter_initialize(&emitter), "emitter reuse init 1");
    yaml_emitter_set_output_string(&emitter, buffer, sizeof(buffer), &written);
    yaml_emitter_set_canonical(&emitter, 0);

    yaml_event_t event;
    yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
    yaml_emitter_emit(&emitter, &event);
    yaml_stream_end_event_initialize(&event);
    yaml_emitter_emit(&emitter, &event);
    yaml_emitter_delete(&emitter);

    /* Second use */
    ASSERT(yaml_emitter_initialize(&emitter), "emitter reuse init 2");
    yaml_emitter_set_output_string(&emitter, buffer, sizeof(buffer), &written);
    yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
    yaml_emitter_emit(&emitter, &event);
    yaml_stream_end_event_initialize(&event);
    yaml_emitter_emit(&emitter, &event);
    yaml_emitter_delete(&emitter);

    ASSERT(1, "emitter reuse: no crash");
}

static void test_special_yaml_values(void) {
    /* Various YAML values that have special meaning */
    yaml_event_type_t events[16];
    int count;

    ASSERT(parse_string_events("true", events, 16, &count), "value: true");
    ASSERT(parse_string_events("false", events, 16, &count), "value: false");
    ASSERT(parse_string_events("null", events, 16, &count), "value: null");
    ASSERT(parse_string_events("~", events, 16, &count), "value: tilde");
    ASSERT(parse_string_events(".inf", events, 16, &count), "value: .inf");
    ASSERT(parse_string_events("-.inf", events, 16, &count), "value: -.inf");
    ASSERT(parse_string_events(".nan", events, 16, &count), "value: .nan");
    ASSERT(parse_string_events("0", events, 16, &count), "value: zero");
    ASSERT(parse_string_events("0x1A", events, 16, &count), "value: hex");
    ASSERT(parse_string_events("0o17", events, 16, &count), "value: octal");
    ASSERT(parse_string_events("1.23e4", events, 16, &count), "value: scientific");
}

static void test_whitespace_handling(void) {
    yaml_event_type_t events[32];
    int count;

    /* Trailing whitespace */
    ASSERT(parse_string_events("key: value   \n", events, 32, &count),
           "trailing whitespace");
    /* Leading blank lines */
    ASSERT(parse_string_events("\n\n\nkey: value\n", events, 32, &count),
           "leading blank lines");
    /* Only whitespace */
    ASSERT(parse_string_events("   \n   \n", events, 32, &count),
           "only whitespace");
}

static void test_unicode_scalars(void) {
    yaml_event_type_t events[16];
    int count;

    /* UTF-8 content */
    ASSERT(parse_string_events("key: \xc3\xa9l\xc3\xa8ve\n", events, 16, &count),
           "utf8 accented");
    ASSERT(parse_string_events("key: \xe4\xb8\xad\xe6\x96\x87\n", events, 16, &count),
           "utf8 chinese");
    ASSERT(parse_string_events("key: \xf0\x9f\x98\x80\n", events, 16, &count),
           "utf8 emoji");
}

static void test_large_sequence(void) {
    /* Build a YAML sequence with 50 items */
    char yaml[4096];
    int pos = 0;
    for (int i = 0; i < 50; i++) {
        pos += snprintf(yaml + pos, sizeof(yaml) - pos, "- item%d\n", i);
    }
    yaml_event_type_t events[256];
    int count;
    ASSERT(parse_string_events(yaml, events, 256, &count), "large seq");
    int seq_count = 0;
    for (int i = 0; i < count; i++)
        if (events[i] == YAML_SCALAR_EVENT) seq_count++;
    ASSERT_EQ_INT(seq_count, 50, "large seq: 50 items");
}

static void test_large_mapping(void) {
    char yaml[4096];
    int pos = 0;
    for (int i = 0; i < 30; i++) {
        pos += snprintf(yaml + pos, sizeof(yaml) - pos, "key%d: val%d\n", i, i);
    }
    yaml_event_type_t events[256];
    int count;
    ASSERT(parse_string_events(yaml, events, 256, &count), "large map");
    int scalar_count = 0;
    for (int i = 0; i < count; i++)
        if (events[i] == YAML_SCALAR_EVENT) scalar_count++;
    ASSERT_EQ_INT(scalar_count, 60, "large map: 60 scalars");
}

/* Regression: BOM (U+FEFF, EF BB BF) inside a comment caused infinite loop.
 * The batch-skip in yaml_parser_scan_to_next_token was counting bytes instead
 * of characters for the unread decrement, causing underflow on multi-byte chars. */
static void test_comment_bom_regression(void) {
    /* Original fuzz input: v1\nusY\r\r\riv1\n#k<BOM>\n */
    unsigned char data1[] = {0x76,0x31,0x0a,0x75,0x73,0x59,0x0d,0x0d,
                             0x0d,0x69,0x76,0x31,0x0a,0x23,0x6b,0xef,
                             0xbb,0xbf,0x0a};
    yaml_parser_t parser;
    yaml_event_t event;

    ASSERT(yaml_parser_initialize(&parser), "init");
    yaml_parser_set_input_string(&parser, data1, sizeof(data1));
    int count = 0;
    while (1) {
        int ok = yaml_parser_parse(&parser, &event);
        if (!ok) break;
        int type = event.type;
        yaml_event_delete(&event);
        if (type == YAML_STREAM_END_EVENT) break;
        count++;
        ASSERT(count < 100, "must not loop forever");
    }
    yaml_parser_delete(&parser);
    ASSERT(count > 0, "produced some events");

    /* Simpler case: comment with BOM directly */
    unsigned char data2[] = {'#', 'Y', 0xef, 0xbb, 0xbf, 'A'};
    ASSERT(yaml_parser_initialize(&parser), "init2");
    yaml_parser_set_input_string(&parser, data2, sizeof(data2));
    count = 0;
    while (1) {
        int ok = yaml_parser_parse(&parser, &event);
        if (!ok) break;
        int type = event.type;
        yaml_event_delete(&event);
        if (type == YAML_STREAM_END_EVENT) break;
        count++;
        ASSERT(count < 100, "must not loop forever (case 2)");
    }
    yaml_parser_delete(&parser);
}

int main(void) {
    TEST_SUITE_BEGIN("Edge Cases");

    test_empty_values();
    test_complex_keys();
    test_nested_flow();
    test_mixed_styles();
    test_multiple_anchors();
    test_document_separators();
    test_indentation();
    test_comments();
    test_parser_lifecycle();
    test_emitter_lifecycle();
    test_null_edge_cases();
    test_sequence_of_mappings();
    test_mapping_of_sequences();
    test_token_delete();
    test_event_delete();
    test_indentless_sequence();
    test_tabs_handling();
    test_long_key();

    /* Extended edge cases */
    test_empty_flow_collections();
    test_nested_empty_collections();
    test_deeply_nested_collections();
    test_flow_map_nested_seq();
    test_flow_seq_nested_map();
    test_multiline_flow_seq();
    test_multiline_flow_map();
    test_trailing_comma_flow();
    test_block_scalar_empty_lines();
    test_folded_scalar_content();
    test_multiple_scalars_in_mapping();
    test_parser_reuse();
    test_emitter_reuse();
    test_special_yaml_values();
    test_whitespace_handling();
    test_unicode_scalars();
    test_large_sequence();
    test_large_mapping();

    /* Fuzz regression: BOM in comment caused infinite loop */
    test_comment_bom_regression();

    TEST_SUITE_END();
}
