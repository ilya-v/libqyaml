#include "test_helper.h"

/* Test: many keys in a mapping */
static void test_many_keys(void) {
    char yaml[32768];
    int pos = 0;
    for (int i = 0; i < 200; i++) {
        pos += snprintf(yaml + pos, sizeof(yaml) - pos, "key%d: value%d\n", i, i);
    }

    yaml_event_type_t events[2048];
    int count;
    ASSERT(parse_string_events(yaml, events, 2048, &count), "many keys: parse");

    int scalar_count = 0;
    for (int i = 0; i < count; i++) {
        if (events[i] == YAML_SCALAR_EVENT) scalar_count++;
    }
    ASSERT_EQ_INT(scalar_count, 400, "many keys: 400 scalars");
}

/* Test: many items in a sequence */
static void test_many_items(void) {
    char yaml[16384];
    int pos = 0;
    for (int i = 0; i < 200; i++) {
        pos += snprintf(yaml + pos, sizeof(yaml) - pos, "- item%d\n", i);
    }

    yaml_event_type_t events[2048];
    int count;
    ASSERT(parse_string_events(yaml, events, 2048, &count), "many items: parse");

    int scalar_count = 0;
    for (int i = 0; i < count; i++) {
        if (events[i] == YAML_SCALAR_EVENT) scalar_count++;
    }
    ASSERT_EQ_INT(scalar_count, 200, "many items: 200 scalars");
}

/* Test: deeply nested mappings */
static void test_deep_nesting(void) {
    char yaml[4096];
    int pos = 0;
    int depth = 30;
    for (int i = 0; i < depth; i++) {
        for (int j = 0; j < i * 2; j++) yaml[pos++] = ' ';
        pos += snprintf(yaml + pos, sizeof(yaml) - pos, "l%d:\n", i);
    }
    for (int j = 0; j < depth * 2; j++) yaml[pos++] = ' ';
    pos += snprintf(yaml + pos, sizeof(yaml) - pos, "leaf: value\n");

    yaml_event_type_t events[512];
    int count;
    ASSERT(parse_string_events(yaml, events, 512, &count), "deep: parse");

    int map_start = 0;
    for (int i = 0; i < count; i++) {
        if (events[i] == YAML_MAPPING_START_EVENT) map_start++;
    }
    ASSERT(map_start >= depth, "deep: correct nesting depth");
}

/* Test: large flow sequence */
static void test_large_flow_seq(void) {
    char yaml[32768];
    int pos = 0;
    yaml[pos++] = '[';
    for (int i = 0; i < 200; i++) {
        if (i > 0) { yaml[pos++] = ','; yaml[pos++] = ' '; }
        pos += snprintf(yaml + pos, sizeof(yaml) - pos, "item%d", i);
    }
    yaml[pos++] = ']';
    yaml[pos] = '\0';

    yaml_event_type_t events[2048];
    int count;
    ASSERT(parse_string_events(yaml, events, 2048, &count), "large flow: parse");

    int scalar_count = 0;
    for (int i = 0; i < count; i++) {
        if (events[i] == YAML_SCALAR_EVENT) scalar_count++;
    }
    ASSERT_EQ_INT(scalar_count, 200, "large flow: 200 scalars");
}

/* Test: large flow mapping */
static void test_large_flow_map(void) {
    char yaml[32768];
    int pos = 0;
    yaml[pos++] = '{';
    for (int i = 0; i < 100; i++) {
        if (i > 0) { yaml[pos++] = ','; yaml[pos++] = ' '; }
        pos += snprintf(yaml + pos, sizeof(yaml) - pos, "k%d: v%d", i, i);
    }
    yaml[pos++] = '}';
    yaml[pos] = '\0';

    yaml_event_type_t events[2048];
    int count;
    ASSERT(parse_string_events(yaml, events, 2048, &count), "large flow map: parse");

    int scalar_count = 0;
    for (int i = 0; i < count; i++) {
        if (events[i] == YAML_SCALAR_EVENT) scalar_count++;
    }
    ASSERT_EQ_INT(scalar_count, 200, "large flow map: 200 scalars");
}

/* Test: repeated parse cycles */
static void test_repeated_parsing(void) {
    const char *yamls[] = {
        "a: 1\n",
        "- x\n- y\n",
        "[1, 2, 3]",
        "{a: b}",
        "---\nhello\n...\n",
        "'quoted'",
        "\"double\\nquoted\"",
        "key:\n  - item1\n  - item2\n",
        "outer:\n  inner: value\n",
        "- name: test\n  value: 42\n"
    };

    for (int cycle = 0; cycle < 50; cycle++) {
        for (int i = 0; i < 10; i++) {
            yaml_event_type_t events[64];
            int count;
            ASSERT(parse_string_events(yamls[i], events, 64, &count),
                   "repeated: parse");
        }
    }
    ASSERT(1, "repeated: 500 parses OK");
}

/* Test: token-level repeated scanning */
static void test_repeated_scanning(void) {
    const char *yamls[] = {
        "key: value\n",
        "- item\n",
        "[a, b]\n",
        "{x: y}\n",
        "!!str hello\n"
    };

    for (int cycle = 0; cycle < 50; cycle++) {
        for (int i = 0; i < 5; i++) {
            yaml_token_type_t tokens[32];
            int count;
            ASSERT(scan_string_tokens(yamls[i], tokens, 32, &count),
                   "scan repeated");
        }
    }
    ASSERT(1, "scan repeated: 250 scans OK");
}

/* Test: document load/delete cycles */
static void test_load_cycles(void) {
    const char *yamls[] = {
        "key: value\n",
        "- a\n- b\n- c\n",
        "{x: 1, y: 2}\n",
        "[1, 2, 3, 4, 5]\n",
        "nested:\n  key: val\n"
    };

    for (int cycle = 0; cycle < 50; cycle++) {
        for (int i = 0; i < 5; i++) {
            yaml_parser_t parser;
            yaml_document_t doc;

            ASSERT(yaml_parser_initialize(&parser), "load cycle: init");
            yaml_parser_set_input_string(&parser,
                (const unsigned char *)yamls[i], strlen(yamls[i]));

            ASSERT(yaml_parser_load(&parser, &doc), "load cycle: load");
            yaml_node_t *root = yaml_document_get_root_node(&doc);
            ASSERT_NOT_NULL(root, "load cycle: has root");

            yaml_document_delete(&doc);
            yaml_parser_delete(&parser);
        }
    }
    ASSERT(1, "load cycles: 250 loads OK");
}

/* Test: emitter stress */
static void test_emitter_stress(void) {
    for (int cycle = 0; cycle < 20; cycle++) {
        yaml_emitter_t emitter;
        yaml_event_t event;
        unsigned char output[8192];
        size_t written = 0;

        ASSERT(yaml_emitter_initialize(&emitter), "emit stress: init");
        yaml_emitter_set_output_string(&emitter, output, sizeof(output), &written);

        yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
        yaml_emitter_emit(&emitter, &event);

        yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1);
        yaml_emitter_emit(&emitter, &event);

        yaml_sequence_start_event_initialize(&event, NULL,
            (const yaml_char_t *)YAML_SEQ_TAG, 1, YAML_BLOCK_SEQUENCE_STYLE);
        yaml_emitter_emit(&emitter, &event);

        for (int i = 0; i < 50; i++) {
            char buf[32];
            snprintf(buf, sizeof(buf), "item%d", i);
            yaml_scalar_event_initialize(&event, NULL,
                (const yaml_char_t *)YAML_STR_TAG,
                (const yaml_char_t *)buf, (int)strlen(buf),
                1, 1, YAML_PLAIN_SCALAR_STYLE);
            yaml_emitter_emit(&emitter, &event);
        }

        yaml_sequence_end_event_initialize(&event);
        yaml_emitter_emit(&emitter, &event);

        yaml_document_end_event_initialize(&event, 1);
        yaml_emitter_emit(&emitter, &event);

        yaml_stream_end_event_initialize(&event);
        yaml_emitter_emit(&emitter, &event);

        ASSERT(written > 0, "emit stress: has output");
        yaml_emitter_delete(&emitter);
    }
    ASSERT(1, "emit stress: 20 cycles OK");
}

/* Test: round-trip stress */
static void test_roundtrip_stress(void) {
    const char *yamls[] = {
        "a: 1\nb: 2\nc: 3\n",
        "- x\n- y\n- z\n",
        "nested:\n  key: value\n  list:\n    - a\n    - b\n",
    };

    for (int i = 0; i < 3; i++) {
        /* Parse */
        yaml_parser_t parser;
        ASSERT(yaml_parser_initialize(&parser), "rt stress: parse init");
        yaml_parser_set_input_string(&parser,
            (const unsigned char *)yamls[i], strlen(yamls[i]));

        yaml_event_t events[128];
        int event_count = 0;
        while (event_count < 128) {
            ASSERT(yaml_parser_parse(&parser, &events[event_count]),
                   "rt stress: parse");
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
        ASSERT(yaml_emitter_initialize(&emitter), "rt stress: emit init");
        yaml_emitter_set_output_string(&emitter, output, sizeof(output), &written);

        for (int j = 0; j < event_count; j++) {
            ASSERT(yaml_emitter_emit(&emitter, &events[j]), "rt stress: emit");
        }
        yaml_emitter_delete(&emitter);

        ASSERT(written > 0, "rt stress: output");

        /* Re-parse the emitted output */
        ASSERT(yaml_parser_initialize(&parser), "rt stress: reparse init");
        yaml_parser_set_input_string(&parser, output, written);

        yaml_event_t event;
        int reparse_count = 0;
        while (1) {
            ASSERT(yaml_parser_parse(&parser, &event), "rt stress: reparse");
            reparse_count++;
            if (event.type == YAML_STREAM_END_EVENT) {
                yaml_event_delete(&event);
                break;
            }
            yaml_event_delete(&event);
        }
        ASSERT(reparse_count > 0, "rt stress: reparse events");
        yaml_parser_delete(&parser);
    }
}

int main(void) {
    TEST_SUITE_BEGIN("Stress");

    test_many_keys();
    test_many_items();
    test_deep_nesting();
    test_large_flow_seq();
    test_large_flow_map();
    test_repeated_parsing();
    test_repeated_scanning();
    test_load_cycles();
    test_emitter_stress();
    test_roundtrip_stress();

    TEST_SUITE_END();
}
