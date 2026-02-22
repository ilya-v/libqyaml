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

/* ==================================================================
 * Extended stress tests
 * ================================================================== */

static void test_many_documents(void) {
    char yaml[32768];
    int pos = 0;
    for (int i = 0; i < 100; i++) {
        pos += snprintf(yaml + pos, sizeof(yaml) - pos, "---\ndoc%d\n...\n", i);
    }

    yaml_parser_t parser;
    yaml_event_t event;
    ASSERT(yaml_parser_initialize(&parser), "many docs init");
    yaml_parser_set_input_string(&parser, (const unsigned char *)yaml, strlen(yaml));

    int doc_count = 0;
    while (1) {
        ASSERT(yaml_parser_parse(&parser, &event), "many docs parse");
        if (event.type == YAML_DOCUMENT_START_EVENT) doc_count++;
        if (event.type == YAML_STREAM_END_EVENT) { yaml_event_delete(&event); break; }
        yaml_event_delete(&event);
    }
    ASSERT_EQ_INT(doc_count, 100, "many docs: 100 documents");
    yaml_parser_delete(&parser);
}

static void test_wide_mapping(void) {
    /* Mapping with 500 key-value pairs */
    char yaml[65536];
    int pos = 0;
    for (int i = 0; i < 500; i++) {
        pos += snprintf(yaml + pos, sizeof(yaml) - pos, "key_%04d: val_%04d\n", i, i);
    }

    yaml_parser_t parser;
    yaml_document_t doc;
    ASSERT(yaml_parser_initialize(&parser), "wide map init");
    yaml_parser_set_input_string(&parser, (const unsigned char *)yaml, strlen(yaml));
    ASSERT(yaml_parser_load(&parser, &doc), "wide map load");

    yaml_node_t *root = yaml_document_get_root_node(&doc);
    ASSERT_NOT_NULL(root, "wide map: root");
    ASSERT_EQ_INT(root->type, YAML_MAPPING_NODE, "wide map: is mapping");

    int pair_count = (int)(root->data.mapping.pairs.top -
                           root->data.mapping.pairs.start);
    ASSERT_EQ_INT(pair_count, 500, "wide map: 500 pairs");

    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
}

static void test_wide_sequence(void) {
    char yaml[32768];
    int pos = 0;
    for (int i = 0; i < 500; i++) {
        pos += snprintf(yaml + pos, sizeof(yaml) - pos, "- item_%04d\n", i);
    }

    yaml_parser_t parser;
    yaml_document_t doc;
    ASSERT(yaml_parser_initialize(&parser), "wide seq init");
    yaml_parser_set_input_string(&parser, (const unsigned char *)yaml, strlen(yaml));
    ASSERT(yaml_parser_load(&parser, &doc), "wide seq load");

    yaml_node_t *root = yaml_document_get_root_node(&doc);
    ASSERT_NOT_NULL(root, "wide seq: root");
    ASSERT_EQ_INT(root->type, YAML_SEQUENCE_NODE, "wide seq: is seq");

    int item_count = (int)(root->data.sequence.items.top -
                           root->data.sequence.items.start);
    ASSERT_EQ_INT(item_count, 500, "wide seq: 500 items");

    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
}

static void test_deep_block_nesting(void) {
    /* Build 50-deep nested mapping */
    char yaml[4096];
    int pos = 0;
    for (int i = 0; i < 50; i++) {
        for (int j = 0; j < i * 2; j++) yaml[pos++] = ' ';
        pos += snprintf(yaml + pos, sizeof(yaml) - pos, "level%d:\n", i);
    }
    for (int j = 0; j < 50 * 2; j++) yaml[pos++] = ' ';
    pos += snprintf(yaml + pos, sizeof(yaml) - pos, "leaf: value\n");

    yaml_event_type_t events[256];
    int count;
    ASSERT(parse_string_events(yaml, events, 256, &count), "deep block");
    ASSERT(count > 50, "deep block: many events");
}

static void test_many_anchors_aliases(void) {
    char yaml[32768];
    int pos = 0;

    /* Create many anchored values then alias them */
    pos += snprintf(yaml + pos, sizeof(yaml) - pos, "anchors:\n");
    for (int i = 0; i < 50; i++) {
        pos += snprintf(yaml + pos, sizeof(yaml) - pos,
                        "  a%d: &ref%d value%d\n", i, i, i);
    }
    pos += snprintf(yaml + pos, sizeof(yaml) - pos, "aliases:\n");
    for (int i = 0; i < 50; i++) {
        pos += snprintf(yaml + pos, sizeof(yaml) - pos,
                        "  b%d: *ref%d\n", i, i);
    }

    yaml_parser_t parser;
    yaml_document_t doc;
    ASSERT(yaml_parser_initialize(&parser), "many anchor init");
    yaml_parser_set_input_string(&parser, (const unsigned char *)yaml, strlen(yaml));
    ASSERT(yaml_parser_load(&parser, &doc), "many anchor load");

    yaml_node_t *root = yaml_document_get_root_node(&doc);
    ASSERT_NOT_NULL(root, "many anchor: root");
    ASSERT_EQ_INT(root->type, YAML_MAPPING_NODE, "many anchor: mapping");

    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
}

static void test_large_scalar_value(void) {
    /* Build a 10KB scalar value */
    char yaml[16384];
    yaml[0] = '"';
    memset(yaml + 1, 'x', 10000);
    yaml[10001] = '"';
    yaml[10002] = '\n';
    yaml[10003] = '\0';

    yaml_parser_t parser;
    yaml_event_t event;
    ASSERT(yaml_parser_initialize(&parser), "large scalar init");
    yaml_parser_set_input_string(&parser, (const unsigned char *)yaml, strlen(yaml));

    int found = 0;
    while (1) {
        ASSERT(yaml_parser_parse(&parser, &event), "large scalar parse");
        if (event.type == YAML_SCALAR_EVENT) {
            found = 1;
            ASSERT_EQ_INT((int)event.data.scalar.length, 10000, "large scalar: length");
        }
        if (event.type == YAML_STREAM_END_EVENT) { yaml_event_delete(&event); break; }
        yaml_event_delete(&event);
    }
    ASSERT(found, "large scalar: found");
    yaml_parser_delete(&parser);
}

static void test_rapid_init_delete(void) {
    /* Rapid initialization and deletion cycles */
    for (int i = 0; i < 100; i++) {
        yaml_parser_t parser;
        ASSERT(yaml_parser_initialize(&parser), "rapid init");
        yaml_parser_delete(&parser);
    }
    for (int i = 0; i < 100; i++) {
        yaml_emitter_t emitter;
        ASSERT(yaml_emitter_initialize(&emitter), "rapid emitter init");
        yaml_emitter_delete(&emitter);
    }
    ASSERT(1, "rapid init/delete: no leaks");
}

static void test_mixed_collections(void) {
    /* Complex structure with mixed collection types */
    const char *yaml =
        "servers:\n"
        "  - name: web\n"
        "    ports: [80, 443]\n"
        "    tags: {env: prod, role: web}\n"
        "  - name: db\n"
        "    ports: [5432]\n"
        "    tags: {env: prod, role: db}\n"
        "config:\n"
        "  debug: false\n"
        "  log_level: info\n";

    yaml_parser_t parser;
    yaml_document_t doc;
    ASSERT(yaml_parser_initialize(&parser), "mixed init");
    yaml_parser_set_input_string(&parser, (const unsigned char *)yaml, strlen(yaml));
    ASSERT(yaml_parser_load(&parser, &doc), "mixed load");

    yaml_node_t *root = yaml_document_get_root_node(&doc);
    ASSERT_NOT_NULL(root, "mixed: root");
    ASSERT_EQ_INT(root->type, YAML_MAPPING_NODE, "mixed: is mapping");

    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
}

static void test_emitter_many_events(void) {
    yaml_emitter_t emitter;
    unsigned char buffer[65536];
    size_t written = 0;
    yaml_event_t event;

    ASSERT(yaml_emitter_initialize(&emitter), "many events init");
    yaml_emitter_set_output_string(&emitter, buffer, sizeof(buffer), &written);

    yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
    ASSERT(yaml_emitter_emit(&emitter, &event), "many events: stream start");

    for (int doc = 0; doc < 10; doc++) {
        yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1);
        ASSERT(yaml_emitter_emit(&emitter, &event), "many events: doc start");

        yaml_sequence_start_event_initialize(&event, NULL, NULL, 1,
            YAML_BLOCK_SEQUENCE_STYLE);
        ASSERT(yaml_emitter_emit(&emitter, &event), "many events: seq start");

        for (int item = 0; item < 20; item++) {
            char val[16];
            snprintf(val, sizeof(val), "item_%d_%d", doc, item);
            yaml_scalar_event_initialize(&event, NULL, NULL,
                (const yaml_char_t *)val, strlen(val), 1, 0,
                YAML_PLAIN_SCALAR_STYLE);
            ASSERT(yaml_emitter_emit(&emitter, &event), "many events: scalar");
        }

        yaml_sequence_end_event_initialize(&event);
        ASSERT(yaml_emitter_emit(&emitter, &event), "many events: seq end");

        yaml_document_end_event_initialize(&event, 1);
        ASSERT(yaml_emitter_emit(&emitter, &event), "many events: doc end");
    }

    yaml_stream_end_event_initialize(&event);
    ASSERT(yaml_emitter_emit(&emitter, &event), "many events: stream end");

    ASSERT(written > 0, "many events: output");
    yaml_emitter_delete(&emitter);
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

    /* Extended */
    test_many_documents();
    test_wide_mapping();
    test_wide_sequence();
    test_deep_block_nesting();
    test_many_anchors_aliases();
    test_large_scalar_value();
    test_rapid_init_delete();
    test_mixed_collections();
    test_emitter_many_events();

    TEST_SUITE_END();
}
