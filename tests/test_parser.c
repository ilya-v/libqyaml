#include "test_helper.h"

/* Test parsing empty stream */
static void test_empty_stream(void) {
    yaml_event_type_t events[16];
    int count;

    ASSERT(parse_string_events("", events, 16, &count), "parse empty");
    ASSERT_EQ_INT(count, 2, "empty: 2 events");
    ASSERT_EQ_INT(events[0], YAML_STREAM_START_EVENT, "empty: stream start");
    ASSERT_EQ_INT(events[1], YAML_STREAM_END_EVENT, "empty: stream end");
}

/* Test parsing simple scalar document */
static void test_simple_scalar(void) {
    yaml_event_type_t events[16];
    int count;

    ASSERT(parse_string_events("hello", events, 16, &count), "parse scalar");
    ASSERT(count >= 4, "scalar: at least 4 events");
    ASSERT_EQ_INT(events[0], YAML_STREAM_START_EVENT, "scalar: stream start");
    ASSERT_EQ_INT(events[1], YAML_DOCUMENT_START_EVENT, "scalar: doc start");
    ASSERT_EQ_INT(events[2], YAML_SCALAR_EVENT, "scalar: scalar event");
    ASSERT_EQ_INT(events[3], YAML_DOCUMENT_END_EVENT, "scalar: doc end");
}

/* Test parsing mapping */
static void test_mapping(void) {
    yaml_event_type_t events[32];
    int count;

    ASSERT(parse_string_events("key1: val1\nkey2: val2\n", events, 32, &count),
           "parse mapping");

    int found_map_start = 0, found_map_end = 0, scalar_count = 0;
    for (int i = 0; i < count; i++) {
        if (events[i] == YAML_MAPPING_START_EVENT) found_map_start = 1;
        if (events[i] == YAML_MAPPING_END_EVENT) found_map_end = 1;
        if (events[i] == YAML_SCALAR_EVENT) scalar_count++;
    }
    ASSERT(found_map_start, "mapping: start event");
    ASSERT(found_map_end, "mapping: end event");
    ASSERT_EQ_INT(scalar_count, 4, "mapping: 4 scalars");
}

/* Test parsing sequence */
static void test_sequence(void) {
    yaml_event_type_t events[32];
    int count;

    ASSERT(parse_string_events("- a\n- b\n- c\n", events, 32, &count),
           "parse sequence");

    int found_seq_start = 0, found_seq_end = 0, scalar_count = 0;
    for (int i = 0; i < count; i++) {
        if (events[i] == YAML_SEQUENCE_START_EVENT) found_seq_start = 1;
        if (events[i] == YAML_SEQUENCE_END_EVENT) found_seq_end = 1;
        if (events[i] == YAML_SCALAR_EVENT) scalar_count++;
    }
    ASSERT(found_seq_start, "sequence: start event");
    ASSERT(found_seq_end, "sequence: end event");
    ASSERT_EQ_INT(scalar_count, 3, "sequence: 3 scalars");
}

/* Test parsing flow sequence */
static void test_flow_sequence(void) {
    yaml_event_type_t events[32];
    int count;

    ASSERT(parse_string_events("[1, 2, 3]", events, 32, &count), "parse flow seq");

    int scalar_count = 0;
    for (int i = 0; i < count; i++) {
        if (events[i] == YAML_SCALAR_EVENT) scalar_count++;
    }
    ASSERT_EQ_INT(scalar_count, 3, "flow seq: 3 scalars");
}

/* Test parsing flow mapping */
static void test_flow_mapping(void) {
    yaml_event_type_t events[32];
    int count;

    ASSERT(parse_string_events("{a: 1, b: 2}", events, 32, &count), "parse flow map");

    int scalar_count = 0;
    for (int i = 0; i < count; i++) {
        if (events[i] == YAML_SCALAR_EVENT) scalar_count++;
    }
    ASSERT_EQ_INT(scalar_count, 4, "flow map: 4 scalars");
}

/* Test parsing nested structures */
static void test_nested(void) {
    const char *yaml =
        "root:\n"
        "  child1:\n"
        "    - a\n"
        "    - b\n"
        "  child2: value\n";

    yaml_event_type_t events[64];
    int count;

    ASSERT(parse_string_events(yaml, events, 64, &count), "parse nested");

    int map_start = 0, map_end = 0, seq_start = 0, seq_end = 0;
    for (int i = 0; i < count; i++) {
        if (events[i] == YAML_MAPPING_START_EVENT) map_start++;
        if (events[i] == YAML_MAPPING_END_EVENT) map_end++;
        if (events[i] == YAML_SEQUENCE_START_EVENT) seq_start++;
        if (events[i] == YAML_SEQUENCE_END_EVENT) seq_end++;
    }
    ASSERT_EQ_INT(map_start, map_end, "nested: balanced mappings");
    ASSERT_EQ_INT(seq_start, seq_end, "nested: balanced sequences");
    ASSERT(map_start >= 2, "nested: 2+ mapping levels");
    ASSERT(seq_start >= 1, "nested: has sequence");
}

/* Test parsing anchors and aliases */
static void test_anchors_aliases(void) {
    const char *yaml = "a: &anchor value\nb: *anchor\n";
    yaml_event_type_t events[32];
    int count;

    ASSERT(parse_string_events(yaml, events, 32, &count), "parse anchors");

    int found_alias = 0;
    for (int i = 0; i < count; i++) {
        if (events[i] == YAML_ALIAS_EVENT) found_alias = 1;
    }
    ASSERT(found_alias, "anchors: found alias event");
}

/* Test parsing multiple documents */
static void test_multiple_documents(void) {
    const char *yaml = "---\ndoc1\n---\ndoc2\n...\n";
    yaml_event_type_t events[32];
    int count;

    ASSERT(parse_string_events(yaml, events, 32, &count), "parse multi-doc");

    int doc_start = 0, doc_end = 0;
    for (int i = 0; i < count; i++) {
        if (events[i] == YAML_DOCUMENT_START_EVENT) doc_start++;
        if (events[i] == YAML_DOCUMENT_END_EVENT) doc_end++;
    }
    ASSERT_EQ_INT(doc_start, 2, "multi-doc: 2 doc starts");
    ASSERT_EQ_INT(doc_end, 2, "multi-doc: 2 doc ends");
}

/* Test parsing with version directive */
static void test_version_directive(void) {
    const char *yaml = "%YAML 1.1\n---\nhello\n";

    yaml_parser_t parser;
    yaml_event_t event;

    ASSERT(yaml_parser_initialize(&parser), "init version directive");
    yaml_parser_set_input_string(&parser, (const unsigned char *)yaml, strlen(yaml));

    ASSERT(yaml_parser_parse(&parser, &event), "parse stream start");
    ASSERT_EQ_INT(event.type, YAML_STREAM_START_EVENT, "version: stream start");
    yaml_event_delete(&event);

    ASSERT(yaml_parser_parse(&parser, &event), "parse doc start");
    ASSERT_EQ_INT(event.type, YAML_DOCUMENT_START_EVENT, "version: doc start");
    ASSERT_NOT_NULL(event.data.document_start.version_directive, "version: has directive");
    if (event.data.document_start.version_directive) {
        ASSERT_EQ_INT(event.data.document_start.version_directive->major, 1, "version: major=1");
        ASSERT_EQ_INT(event.data.document_start.version_directive->minor, 1, "version: minor=1");
    }
    yaml_event_delete(&event);

    yaml_parser_delete(&parser);
}

/* Test parsing tag directives */
static void test_tag_directive(void) {
    const char *yaml = "%TAG !t! tag:example.com,2000:\n---\n!t!foo bar\n";

    yaml_parser_t parser;
    yaml_event_t event;

    ASSERT(yaml_parser_initialize(&parser), "init tag directive");
    yaml_parser_set_input_string(&parser, (const unsigned char *)yaml, strlen(yaml));

    /* stream start */
    ASSERT(yaml_parser_parse(&parser, &event), "tag: stream start");
    yaml_event_delete(&event);

    /* document start with tag directives */
    ASSERT(yaml_parser_parse(&parser, &event), "tag: doc start");
    ASSERT_EQ_INT(event.type, YAML_DOCUMENT_START_EVENT, "tag: is doc start");
    yaml_event_delete(&event);

    yaml_parser_delete(&parser);
}

/* Test scalar values and styles */
static void test_scalar_values(void) {
    yaml_parser_t parser;
    yaml_event_t event;

    /* Plain scalar */
    const char *yaml = "hello world";
    ASSERT(yaml_parser_initialize(&parser), "init scalar values");
    yaml_parser_set_input_string(&parser, (const unsigned char *)yaml, strlen(yaml));

    /* Skip to scalar event */
    while (1) {
        ASSERT(yaml_parser_parse(&parser, &event), "parse scalar value event");
        if (event.type == YAML_SCALAR_EVENT) {
            ASSERT_EQ_STR((const char *)event.data.scalar.value, "hello world",
                         "scalar: plain value");
            ASSERT_EQ_INT(event.data.scalar.style, YAML_PLAIN_SCALAR_STYLE,
                         "scalar: plain style");
            ASSERT_EQ_INT((int)event.data.scalar.length, 11, "scalar: length=11");
            yaml_event_delete(&event);
            break;
        }
        yaml_event_delete(&event);
    }
    yaml_parser_delete(&parser);
}

/* Test implicit/explicit document indicators */
static void test_implicit_explicit(void) {
    yaml_parser_t parser;
    yaml_event_t event;

    /* Implicit document */
    ASSERT(yaml_parser_initialize(&parser), "init implicit doc");
    yaml_parser_set_input_string(&parser, (const unsigned char *)"hello", 5);
    yaml_parser_parse(&parser, &event); yaml_event_delete(&event); /* stream start */
    ASSERT(yaml_parser_parse(&parser, &event), "parse implicit doc start");
    ASSERT_EQ_INT(event.type, YAML_DOCUMENT_START_EVENT, "implicit: doc start");
    ASSERT(event.data.document_start.implicit, "implicit: is implicit");
    yaml_event_delete(&event);
    yaml_parser_delete(&parser);

    /* Explicit document */
    ASSERT(yaml_parser_initialize(&parser), "init explicit doc");
    yaml_parser_set_input_string(&parser, (const unsigned char *)"---\nhello", 9);
    yaml_parser_parse(&parser, &event); yaml_event_delete(&event); /* stream start */
    ASSERT(yaml_parser_parse(&parser, &event), "parse explicit doc start");
    ASSERT_EQ_INT(event.type, YAML_DOCUMENT_START_EVENT, "explicit: doc start");
    ASSERT(!event.data.document_start.implicit, "explicit: is not implicit");
    yaml_event_delete(&event);
    yaml_parser_delete(&parser);
}

/* Test parsing deeply nested structures */
static void test_deep_nesting(void) {
    /* Build a deeply nested mapping */
    char yaml[2048];
    int pos = 0;
    for (int i = 0; i < 20; i++) {
        for (int j = 0; j < i * 2; j++) yaml[pos++] = ' ';
        pos += snprintf(yaml + pos, sizeof(yaml) - pos, "level%d:\n", i);
    }
    for (int j = 0; j < 20 * 2; j++) yaml[pos++] = ' ';
    pos += snprintf(yaml + pos, sizeof(yaml) - pos, "leaf: value\n");
    yaml[pos] = '\0';

    yaml_event_type_t events[256];
    int count;
    ASSERT(parse_string_events(yaml, events, 256, &count), "parse deep nesting");
    ASSERT(count > 40, "deep: many events");
}

/* Test parsing error handling */
static void test_parse_errors(void) {
    yaml_parser_t parser;
    yaml_event_t event;

    /* Unmatched flow indicators */
    ASSERT(yaml_parser_initialize(&parser), "init error test");
    yaml_parser_set_input_string(&parser, (const unsigned char *)"[a, b", 5);
    int got_error = 0;
    while (1) {
        if (!yaml_parser_parse(&parser, &event)) {
            got_error = 1;
            break;
        }
        if (event.type == YAML_STREAM_END_EVENT) {
            yaml_event_delete(&event);
            break;
        }
        yaml_event_delete(&event);
    }
    ASSERT(got_error, "error: unmatched [ detected");
    ASSERT(parser.error != YAML_NO_ERROR, "error: error type set");
    ASSERT_NOT_NULL(parser.problem, "error: problem description set");
    yaml_parser_delete(&parser);
}

/* Test event marks */
static void test_event_marks(void) {
    yaml_parser_t parser;
    yaml_event_t event;
    const char *yaml = "key: value\n";

    ASSERT(yaml_parser_initialize(&parser), "init event marks");
    yaml_parser_set_input_string(&parser, (const unsigned char *)yaml, strlen(yaml));

    ASSERT(yaml_parser_parse(&parser, &event), "parse event mark");
    ASSERT_EQ_INT((int)event.start_mark.index, 0, "mark: stream start at 0");
    yaml_event_delete(&event);

    yaml_parser_delete(&parser);
}

int main(void) {
    TEST_SUITE_BEGIN("Parser");

    test_empty_stream();
    test_simple_scalar();
    test_mapping();
    test_sequence();
    test_flow_sequence();
    test_flow_mapping();
    test_nested();
    test_anchors_aliases();
    test_multiple_documents();
    test_version_directive();
    test_tag_directive();
    test_scalar_values();
    test_implicit_explicit();
    test_deep_nesting();
    test_parse_errors();
    test_event_marks();

    TEST_SUITE_END();
}
