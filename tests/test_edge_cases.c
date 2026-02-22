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

    TEST_SUITE_END();
}
