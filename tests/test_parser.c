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

/* Test empty values in mappings (exercises process_empty_scalar) */
static void test_empty_values(void) {
    yaml_parser_t parser;
    yaml_event_t event;

    /* Mapping with empty value */
    const char *yaml = "key:\nother: val\n";
    ASSERT(yaml_parser_initialize(&parser), "init empty values");
    yaml_parser_set_input_string(&parser, (const unsigned char *)yaml, strlen(yaml));

    int found_empty = 0;
    while (1) {
        ASSERT(yaml_parser_parse(&parser, &event), "parse empty value event");
        if (event.type == YAML_SCALAR_EVENT) {
            if (event.data.scalar.length == 0) {
                found_empty = 1;
            }
        }
        if (event.type == YAML_STREAM_END_EVENT) {
            yaml_event_delete(&event);
            break;
        }
        yaml_event_delete(&event);
    }
    ASSERT(found_empty, "empty values: should produce empty scalar");
    yaml_parser_delete(&parser);
}

/* Test block mapping with empty key */
static void test_empty_key(void) {
    yaml_parser_t parser;
    yaml_event_t event;

    /* Empty key in block mapping (: value) */
    const char *yaml = "? \n: value\n";
    ASSERT(yaml_parser_initialize(&parser), "init empty key");
    yaml_parser_set_input_string(&parser, (const unsigned char *)yaml, strlen(yaml));

    int scalar_count = 0;
    while (1) {
        ASSERT(yaml_parser_parse(&parser, &event), "parse empty key event");
        if (event.type == YAML_SCALAR_EVENT) scalar_count++;
        if (event.type == YAML_STREAM_END_EVENT) {
            yaml_event_delete(&event);
            break;
        }
        yaml_event_delete(&event);
    }
    ASSERT(scalar_count >= 2, "empty key: should have key and value scalars");
    yaml_parser_delete(&parser);
}

/* Test complex keys in flow sequences (exercises flow_sequence_entry_mapping states) */
static void test_flow_complex_keys(void) {
    yaml_parser_t parser;
    yaml_event_t event;

    /* Flow sequence with complex mapping key */
    const char *yaml = "[a : b, c : d]";
    ASSERT(yaml_parser_initialize(&parser), "init flow complex keys");
    yaml_parser_set_input_string(&parser, (const unsigned char *)yaml, strlen(yaml));

    int map_start = 0, map_end = 0, scalar_count = 0;
    while (1) {
        ASSERT(yaml_parser_parse(&parser, &event), "parse flow complex key event");
        if (event.type == YAML_MAPPING_START_EVENT) map_start++;
        if (event.type == YAML_MAPPING_END_EVENT) map_end++;
        if (event.type == YAML_SCALAR_EVENT) scalar_count++;
        if (event.type == YAML_STREAM_END_EVENT) {
            yaml_event_delete(&event);
            break;
        }
        yaml_event_delete(&event);
    }
    /* Flow sequence with implicit mappings creates MAPPING events */
    ASSERT(map_start == map_end, "flow complex: balanced mappings");
    ASSERT(scalar_count >= 4, "flow complex: 4+ scalars");
    yaml_parser_delete(&parser);
}

/* Test indentless sequence (sequence under a mapping key without extra indent) */
static void test_indentless_sequence(void) {
    yaml_parser_t parser;
    yaml_event_t event;

    const char *yaml = "key:\n- item1\n- item2\n";
    ASSERT(yaml_parser_initialize(&parser), "init indentless seq");
    yaml_parser_set_input_string(&parser, (const unsigned char *)yaml, strlen(yaml));

    int seq_start = 0, scalar_count = 0;
    while (1) {
        ASSERT(yaml_parser_parse(&parser, &event), "parse indentless seq event");
        if (event.type == YAML_SEQUENCE_START_EVENT) seq_start++;
        if (event.type == YAML_SCALAR_EVENT) scalar_count++;
        if (event.type == YAML_STREAM_END_EVENT) {
            yaml_event_delete(&event);
            break;
        }
        yaml_event_delete(&event);
    }
    ASSERT(seq_start >= 1, "indentless: has sequence");
    ASSERT(scalar_count >= 3, "indentless: key + 2 items");
    yaml_parser_delete(&parser);
}

/* Test flow mapping with empty value */
static void test_flow_empty_value(void) {
    yaml_parser_t parser;
    yaml_event_t event;

    const char *yaml = "{a: , b: c}";
    ASSERT(yaml_parser_initialize(&parser), "init flow empty val");
    yaml_parser_set_input_string(&parser, (const unsigned char *)yaml, strlen(yaml));

    int empty_scalars = 0;
    while (1) {
        ASSERT(yaml_parser_parse(&parser, &event), "parse flow empty val event");
        if (event.type == YAML_SCALAR_EVENT && event.data.scalar.length == 0) {
            empty_scalars++;
        }
        if (event.type == YAML_STREAM_END_EVENT) {
            yaml_event_delete(&event);
            break;
        }
        yaml_event_delete(&event);
    }
    ASSERT(empty_scalars >= 1, "flow empty: at least one empty scalar");
    yaml_parser_delete(&parser);
}

/* Test explicit key in flow mapping */
static void test_flow_explicit_key(void) {
    yaml_parser_t parser;
    yaml_event_t event;

    const char *yaml = "{? key : value}";
    ASSERT(yaml_parser_initialize(&parser), "init flow explicit key");
    yaml_parser_set_input_string(&parser, (const unsigned char *)yaml, strlen(yaml));

    int scalar_count = 0;
    while (1) {
        ASSERT(yaml_parser_parse(&parser, &event), "parse flow explicit key event");
        if (event.type == YAML_SCALAR_EVENT) scalar_count++;
        if (event.type == YAML_STREAM_END_EVENT) {
            yaml_event_delete(&event);
            break;
        }
        yaml_event_delete(&event);
    }
    ASSERT_EQ_INT(scalar_count, 2, "flow explicit key: key + value");
    yaml_parser_delete(&parser);
}

/* Test parsing tags with handles */
static void test_tag_handle_parsing(void) {
    yaml_parser_t parser;
    yaml_event_t event;

    const char *yaml = "%TAG !e! tag:example.com,2000:\n---\n!e!foo bar\n";
    ASSERT(yaml_parser_initialize(&parser), "init tag handle");
    yaml_parser_set_input_string(&parser, (const unsigned char *)yaml, strlen(yaml));

    int found_tagged = 0;
    while (1) {
        ASSERT(yaml_parser_parse(&parser, &event), "parse tag handle event");
        if (event.type == YAML_SCALAR_EVENT) {
            if (event.data.scalar.tag != NULL &&
                strstr((const char *)event.data.scalar.tag, "example.com") != NULL) {
                found_tagged = 1;
            }
        }
        if (event.type == YAML_STREAM_END_EVENT) {
            yaml_event_delete(&event);
            break;
        }
        yaml_event_delete(&event);
    }
    ASSERT(found_tagged, "tag handle: found scalar with resolved tag");
    yaml_parser_delete(&parser);
}

/* Test parser error on duplicate version directive */
static void test_duplicate_version_error(void) {
    yaml_parser_t parser;
    yaml_event_t event;

    const char *yaml = "%YAML 1.1\n%YAML 1.1\n---\nhello\n";
    ASSERT(yaml_parser_initialize(&parser), "init dup version");
    yaml_parser_set_input_string(&parser, (const unsigned char *)yaml, strlen(yaml));

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
    ASSERT(got_error, "duplicate version: should produce error");
    yaml_parser_delete(&parser);
}

/* Test parser error on duplicate tag directive */
static void test_duplicate_tag_error(void) {
    yaml_parser_t parser;
    yaml_event_t event;

    const char *yaml = "%TAG !e! tag:a.com,2000:\n%TAG !e! tag:b.com,2000:\n---\nhello\n";
    ASSERT(yaml_parser_initialize(&parser), "init dup tag");
    yaml_parser_set_input_string(&parser, (const unsigned char *)yaml, strlen(yaml));

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
    ASSERT(got_error, "duplicate tag: should produce error");
    yaml_parser_delete(&parser);
}

/* Test document end followed by new document */
static void test_document_end_then_new(void) {
    yaml_parser_t parser;
    yaml_event_t event;

    const char *yaml = "doc1\n...\n---\ndoc2\n";
    ASSERT(yaml_parser_initialize(&parser), "init doc end new");
    yaml_parser_set_input_string(&parser, (const unsigned char *)yaml, strlen(yaml));

    int doc_start = 0, doc_end = 0;
    while (1) {
        ASSERT(yaml_parser_parse(&parser, &event), "parse doc end new");
        if (event.type == YAML_DOCUMENT_START_EVENT) doc_start++;
        if (event.type == YAML_DOCUMENT_END_EVENT) doc_end++;
        if (event.type == YAML_STREAM_END_EVENT) {
            yaml_event_delete(&event);
            break;
        }
        yaml_event_delete(&event);
    }
    ASSERT_EQ_INT(doc_start, 2, "doc end new: 2 doc starts");
    ASSERT_EQ_INT(doc_end, 2, "doc end new: 2 doc ends");
    yaml_parser_delete(&parser);
}

/* Test flow node (standalone flow in block context) */
static void test_flow_in_block(void) {
    yaml_parser_t parser;
    yaml_event_t event;

    const char *yaml = "- [a, b]\n- {x: 1}\n";
    ASSERT(yaml_parser_initialize(&parser), "init flow in block");
    yaml_parser_set_input_string(&parser, (const unsigned char *)yaml, strlen(yaml));

    int seq_start = 0, map_start = 0;
    while (1) {
        ASSERT(yaml_parser_parse(&parser, &event), "parse flow in block");
        if (event.type == YAML_SEQUENCE_START_EVENT) seq_start++;
        if (event.type == YAML_MAPPING_START_EVENT) map_start++;
        if (event.type == YAML_STREAM_END_EVENT) {
            yaml_event_delete(&event);
            break;
        }
        yaml_event_delete(&event);
    }
    ASSERT(seq_start >= 2, "flow in block: outer seq + inner flow seq");
    ASSERT(map_start >= 1, "flow in block: inner flow map");
    yaml_parser_delete(&parser);
}

/* Test block sequence with explicit key */
static void test_block_explicit_key(void) {
    yaml_parser_t parser;
    yaml_event_t event;

    const char *yaml = "? key1\n: val1\n? key2\n: val2\n";
    ASSERT(yaml_parser_initialize(&parser), "init block explicit key");
    yaml_parser_set_input_string(&parser, (const unsigned char *)yaml, strlen(yaml));

    int scalar_count = 0;
    while (1) {
        ASSERT(yaml_parser_parse(&parser, &event), "parse block explicit key");
        if (event.type == YAML_SCALAR_EVENT) scalar_count++;
        if (event.type == YAML_STREAM_END_EVENT) {
            yaml_event_delete(&event);
            break;
        }
        yaml_event_delete(&event);
    }
    ASSERT_EQ_INT(scalar_count, 4, "block explicit: 4 scalars");
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
    test_empty_values();
    test_empty_key();
    test_flow_complex_keys();
    test_indentless_sequence();
    test_flow_empty_value();
    test_flow_explicit_key();
    test_tag_handle_parsing();
    test_duplicate_version_error();
    test_duplicate_tag_error();
    test_document_end_then_new();
    test_flow_in_block();
    test_block_explicit_key();

    TEST_SUITE_END();
}
