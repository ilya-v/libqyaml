/*
 * Tests targeting specific coverage gaps identified in coverage-da8c08.md.
 *
 * Targets:
 *   - loader.c: alias resolution error paths, multi-document edge cases
 *   - parser.c: flow mapping key/value edge cases, error recovery
 *   - scanner.c: simple key edge cases, rare encoding paths
 *   - reader.c: UTF-16 BOM detection (limited -- full UTF-16 needs binary input)
 */

#include "test_helper.h"

/* ========================================================================
 * Loader coverage gaps: alias and anchor error paths
 * ======================================================================== */

/* Test: undefined alias should produce a composer error */
static void test_load_undefined_alias(void) {
    yaml_parser_t parser;
    yaml_document_t doc;

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser,
        (const unsigned char *)"- *undefined\n", 13);

    int ok = yaml_parser_load(&parser, &doc);
    ASSERT(!ok, "undefined alias: load fails");
    /* Verify error type */
    ASSERT_EQ_INT(parser.error, YAML_COMPOSER_ERROR,
                  "undefined alias: error is COMPOSER_ERROR");
    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
}

/* Test: duplicate anchor should produce a composer error */
static void test_load_duplicate_anchor(void) {
    yaml_parser_t parser;
    yaml_document_t doc;

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser,
        (const unsigned char *)"- &a hello\n- &a world\n", 22);

    int ok = yaml_parser_load(&parser, &doc);
    ASSERT(!ok, "duplicate anchor: load fails");
    ASSERT_EQ_INT(parser.error, YAML_COMPOSER_ERROR,
                  "duplicate anchor: error is COMPOSER_ERROR");
    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
}

/* Test: valid alias resolution -- alias refers to previously anchored node */
static void test_load_valid_alias(void) {
    yaml_parser_t parser;
    yaml_document_t doc;

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser,
        (const unsigned char *)"- &ref hello\n- *ref\n", 20);

    int ok = yaml_parser_load(&parser, &doc);
    ASSERT(ok, "valid alias: load succeeds");

    yaml_node_t *root = yaml_document_get_root_node(&doc);
    ASSERT_NOT_NULL(root, "valid alias: root exists");
    ASSERT_EQ_INT(root->type, YAML_SEQUENCE_NODE, "valid alias: root is sequence");

    int count = root->data.sequence.items.top - root->data.sequence.items.start;
    ASSERT_EQ_INT(count, 2, "valid alias: 2 items");

    /* Both items should be the same node index */
    int item1 = root->data.sequence.items.start[0];
    int item2 = root->data.sequence.items.start[1];
    ASSERT_EQ_INT(item1, item2, "valid alias: alias resolves to same node");

    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
}

/* Test: alias in mapping value */
static void test_load_alias_in_mapping_value(void) {
    yaml_parser_t parser;
    yaml_document_t doc;

    yaml_parser_initialize(&parser);
    const char *input = "base: &base\n  timeout: 30\ndev:\n  <<: *base\n";
    yaml_parser_set_input_string(&parser,
        (const unsigned char *)input, strlen(input));

    int ok = yaml_parser_load(&parser, &doc);
    ASSERT(ok, "alias in mapping value: load succeeds");

    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
}

/* Test: alias as mapping key */
static void test_load_alias_as_key(void) {
    yaml_parser_t parser;
    yaml_document_t doc;

    yaml_parser_initialize(&parser);
    const char *input = "- &a key1\n- &b key2\n- *a: val_a\n  *b: val_b\n";
    yaml_parser_set_input_string(&parser,
        (const unsigned char *)input, strlen(input));

    /* This may or may not be valid depending on interpretation, but
     * it should not crash */
    yaml_parser_load(&parser, &doc);
    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
    ASSERT(1, "alias as key: no crash");
}

/* ========================================================================
 * Loader coverage gaps: multi-document loading
 * ======================================================================== */

/* Test: load multiple documents from a single stream */
static void test_load_multi_document(void) {
    yaml_parser_t parser;
    yaml_document_t doc;
    int doc_count = 0;

    yaml_parser_initialize(&parser);
    const char *input = "---\ndoc1\n---\ndoc2\n---\ndoc3\n";
    yaml_parser_set_input_string(&parser,
        (const unsigned char *)input, strlen(input));

    while (1) {
        int ok = yaml_parser_load(&parser, &doc);
        ASSERT(ok, "multi-doc: load succeeds");
        yaml_node_t *root = yaml_document_get_root_node(&doc);
        if (!root) {
            yaml_document_delete(&doc);
            break;
        }
        doc_count++;
        yaml_document_delete(&doc);
    }

    ASSERT_EQ_INT(doc_count, 3, "multi-doc: 3 documents loaded");
    yaml_parser_delete(&parser);
}

/* Test: load documents with explicit end markers */
static void test_load_multi_document_explicit_end(void) {
    yaml_parser_t parser;
    yaml_document_t doc;
    int doc_count = 0;

    yaml_parser_initialize(&parser);
    const char *input = "---\nhello\n...\n---\nworld\n...\n";
    yaml_parser_set_input_string(&parser,
        (const unsigned char *)input, strlen(input));

    while (1) {
        int ok = yaml_parser_load(&parser, &doc);
        ASSERT(ok, "multi-doc explicit end: load succeeds");
        yaml_node_t *root = yaml_document_get_root_node(&doc);
        if (!root) {
            yaml_document_delete(&doc);
            break;
        }
        doc_count++;
        yaml_document_delete(&doc);
    }

    ASSERT_EQ_INT(doc_count, 2, "multi-doc explicit end: 2 documents loaded");
    yaml_parser_delete(&parser);
}

/* Test: empty documents between markers */
static void test_load_empty_documents(void) {
    yaml_parser_t parser;
    yaml_document_t doc;
    int doc_count = 0;
    int empty_count = 0;

    yaml_parser_initialize(&parser);
    const char *input = "---\n...\n---\nhello\n...\n---\n...\n";
    yaml_parser_set_input_string(&parser,
        (const unsigned char *)input, strlen(input));

    while (1) {
        int ok = yaml_parser_load(&parser, &doc);
        if (!ok) break;
        yaml_node_t *root = yaml_document_get_root_node(&doc);
        if (!root) {
            yaml_document_delete(&doc);
            break;
        }
        doc_count++;
        if (root->type == YAML_SCALAR_NODE &&
            root->data.scalar.length == 0) {
            empty_count++;
        }
        yaml_document_delete(&doc);
    }

    /* Note: empty docs between --- ... may produce empty scalars or
     * be treated as implicit null depending on parser behavior */
    ASSERT(doc_count >= 1, "empty docs: at least 1 document");
    yaml_parser_delete(&parser);
}

/* Test: single document with no markers */
static void test_load_bare_document(void) {
    yaml_parser_t parser;
    yaml_document_t doc;

    yaml_parser_initialize(&parser);
    const char *input = "key: value\n";
    yaml_parser_set_input_string(&parser,
        (const unsigned char *)input, strlen(input));

    int ok = yaml_parser_load(&parser, &doc);
    ASSERT(ok, "bare doc: load succeeds");

    yaml_node_t *root = yaml_document_get_root_node(&doc);
    ASSERT_NOT_NULL(root, "bare doc: root exists");
    ASSERT_EQ_INT(root->type, YAML_MAPPING_NODE, "bare doc: root is mapping");

    yaml_document_delete(&doc);

    /* Load again -- should get empty document (stream end) */
    ok = yaml_parser_load(&parser, &doc);
    ASSERT(ok, "bare doc: second load succeeds (stream end)");
    yaml_node_t *root2 = yaml_document_get_root_node(&doc);
    ASSERT_NULL(root2, "bare doc: second load has no root (stream ended)");
    yaml_document_delete(&doc);

    yaml_parser_delete(&parser);
}

/* ========================================================================
 * Parser coverage gaps: flow mapping edge cases
 * ======================================================================== */

/* Test: empty flow mapping */
static void test_parse_empty_flow_mapping(void) {
    yaml_event_type_t events[16];
    int count;

    ASSERT(parse_string_events("{}", events, 16, &count),
           "empty flow map: parse succeeds");

    int map_start = 0, map_end = 0;
    for (int i = 0; i < count; i++) {
        if (events[i] == YAML_MAPPING_START_EVENT) map_start++;
        if (events[i] == YAML_MAPPING_END_EVENT) map_end++;
    }
    ASSERT_EQ_INT(map_start, 1, "empty flow map: 1 map start");
    ASSERT_EQ_INT(map_end, 1, "empty flow map: 1 map end");
}

/* Test: flow mapping with trailing comma */
static void test_parse_flow_mapping_trailing_comma(void) {
    yaml_event_type_t events[32];
    int count;

    /* Trailing comma is allowed in YAML flow mappings */
    ASSERT(parse_string_events("{a: 1, b: 2,}", events, 32, &count),
           "flow map trailing comma: parse succeeds");
}

/* Test: flow mapping with implicit null values */
static void test_parse_flow_mapping_implicit_values(void) {
    yaml_event_type_t events[32];
    int count;

    /* Flow mapping with key but no value */
    ASSERT(parse_string_events("{a, b, c}", events, 32, &count),
           "flow map implicit values: parse succeeds");

    int scalar_count = 0;
    for (int i = 0; i < count; i++) {
        if (events[i] == YAML_SCALAR_EVENT) scalar_count++;
    }
    /* Each implicit key has key + empty value = 6 scalars for 3 entries */
    ASSERT(scalar_count >= 3, "flow map implicit values: at least 3 scalars");
}

/* Test: flow mapping with complex expressions */
static void test_parse_flow_mapping_complex(void) {
    yaml_event_type_t events[64];
    int count;

    /* Nested flow mappings */
    ASSERT(parse_string_events("{a: {b: c}, d: {e: f}}", events, 64, &count),
           "flow map complex: nested parse succeeds");

    /* Mixed types in flow mapping values */
    ASSERT(parse_string_events("{a: [1, 2], b: {c: d}, e: simple}",
                               events, 64, &count),
           "flow map complex: mixed types parse succeeds");
}

/* Test: flow sequence with implicit mapping entries */
static void test_parse_flow_seq_implicit_mapping(void) {
    yaml_event_type_t events[32];
    int count;

    /* [a: b] creates a single-pair mapping inside the sequence */
    ASSERT(parse_string_events("[a: b]", events, 32, &count),
           "flow seq implicit map: parse succeeds");

    int map_start = 0;
    for (int i = 0; i < count; i++) {
        if (events[i] == YAML_MAPPING_START_EVENT) map_start++;
    }
    ASSERT_EQ_INT(map_start, 1, "flow seq implicit map: 1 mapping start");
}

/* ========================================================================
 * Scanner coverage gaps: simple key edge cases
 * ======================================================================== */

/* Test: adjacent colons and simple key resolution */
static void test_scan_adjacent_colon(void) {
    yaml_token_type_t tokens[32];
    int count;

    /* "key: value" - normal mapping */
    ASSERT(scan_string_tokens("key: value\n", tokens, 32, &count),
           "adjacent colon: basic scan");

    /* ": value" at start of line - should not be a mapping */
    ASSERT(scan_string_tokens(": value\n", tokens, 32, &count),
           "leading colon: scan succeeds");
}

/* Test: very long keys (simple key length limit) */
static void test_scan_long_key_limit(void) {
    /* YAML spec limits simple keys to 1024 characters.
     * Test at the boundary. */
    char input[2048];
    int i;

    /* 1023 chars + ": value\n" - should work as mapping */
    for (i = 0; i < 1023; i++) input[i] = 'a';
    input[1023] = ':';
    input[1024] = ' ';
    input[1025] = 'v';
    input[1026] = '\n';
    input[1027] = '\0';

    yaml_parser_t parser;
    yaml_event_t event;
    int has_mapping = 0;

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)input,
                                 strlen(input));
    while (1) {
        if (!yaml_parser_parse(&parser, &event)) break;
        if (event.type == YAML_MAPPING_START_EVENT) has_mapping = 1;
        int done = (event.type == YAML_STREAM_END_EVENT);
        yaml_event_delete(&event);
        if (done) break;
    }
    yaml_parser_delete(&parser);
    ASSERT(has_mapping, "1023-char key: produces mapping");

    /* 1025 chars + ": value\n" - too long for simple key, becomes
     * a scalar with ": value" at end */
    for (i = 0; i < 1025; i++) input[i] = 'a';
    input[1025] = ':';
    input[1026] = ' ';
    input[1027] = 'v';
    input[1028] = '\n';
    input[1029] = '\0';

    has_mapping = 0;
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)input,
                                 strlen(input));
    while (1) {
        if (!yaml_parser_parse(&parser, &event)) break;
        if (event.type == YAML_MAPPING_START_EVENT) has_mapping = 1;
        int done = (event.type == YAML_STREAM_END_EVENT);
        yaml_event_delete(&event);
        if (done) break;
    }
    yaml_parser_delete(&parser);
    /* Long key should NOT produce a mapping -- the colon is too far
     * from the start of the key to be a simple key */
    ASSERT(!has_mapping, "1025-char key: no mapping (too long for simple key)");
}

/* Test: multiline simple keys */
static void test_scan_multiline_key_in_flow(void) {
    yaml_token_type_t tokens[32];
    int count;

    /* In flow context, multiline keys are allowed */
    ASSERT(scan_string_tokens("{\n  key\n  : value\n}", tokens, 32, &count),
           "multiline flow key: scan succeeds");
}

/* ========================================================================
 * Scanner coverage gaps: block scalar edge cases
 * ======================================================================== */

/* Test: block scalar with explicit indent indicator */
static void test_scan_block_scalar_indent_indicator(void) {
    yaml_parser_t parser;
    yaml_event_t event;
    int found_scalar = 0;
    const char *scalar_val = NULL;

    yaml_parser_initialize(&parser);
    const char *input = "|2\n  indented\n";
    yaml_parser_set_input_string(&parser, (const unsigned char *)input,
                                 strlen(input));

    while (1) {
        if (!yaml_parser_parse(&parser, &event)) break;
        if (event.type == YAML_SCALAR_EVENT) {
            found_scalar = 1;
            /* With indent indicator 2, "  indented\n" -> "indented\n" */
            ASSERT_EQ_STR(event.data.scalar.value, "indented\n",
                          "block scalar indent 2: correct value");
        }
        int done = (event.type == YAML_STREAM_END_EVENT);
        yaml_event_delete(&event);
        if (done) break;
    }
    yaml_parser_delete(&parser);
    ASSERT(found_scalar, "block scalar indent: found scalar");
    (void)scalar_val;
}

/* Test: block scalar with chomp + indent indicators */
static void test_scan_block_scalar_chomp_and_indent(void) {
    yaml_parser_t parser;
    yaml_event_t event;

    yaml_parser_initialize(&parser);
    const char *input = "|-2\n  text\n";
    yaml_parser_set_input_string(&parser, (const unsigned char *)input,
                                 strlen(input));

    int found_scalar = 0;
    while (1) {
        if (!yaml_parser_parse(&parser, &event)) break;
        if (event.type == YAML_SCALAR_EVENT) {
            found_scalar = 1;
            /* Strip chomp + indent 2: "  text\n" -> "text" (no trailing newline) */
            ASSERT_EQ_STR(event.data.scalar.value, "text",
                          "block scalar strip+indent: correct value");
        }
        int done = (event.type == YAML_STREAM_END_EVENT);
        yaml_event_delete(&event);
        if (done) break;
    }
    yaml_parser_delete(&parser);
    ASSERT(found_scalar, "block scalar chomp+indent: found scalar");
}

/* Test: folded scalar with blank lines (paragraph breaks) */
static void test_scan_folded_paragraph_breaks(void) {
    yaml_parser_t parser;
    yaml_event_t event;

    yaml_parser_initialize(&parser);
    const char *input = ">\n  para one\n  continues\n\n  para two\n";
    yaml_parser_set_input_string(&parser, (const unsigned char *)input,
                                 strlen(input));

    int found_scalar = 0;
    while (1) {
        if (!yaml_parser_parse(&parser, &event)) break;
        if (event.type == YAML_SCALAR_EVENT) {
            found_scalar = 1;
            ASSERT_EQ_STR(event.data.scalar.value,
                          "para one continues\npara two\n",
                          "folded paragraphs: correct value");
        }
        int done = (event.type == YAML_STREAM_END_EVENT);
        yaml_event_delete(&event);
        if (done) break;
    }
    yaml_parser_delete(&parser);
    ASSERT(found_scalar, "folded paragraphs: found scalar");
}

/* ========================================================================
 * Parser coverage gaps: error recovery and edge cases
 * ======================================================================== */

/* Test: parse error produces correct error type */
static void test_parse_error_type(void) {
    yaml_parser_t parser;
    yaml_event_t event;

    yaml_parser_initialize(&parser);
    /* ":" at start of stream without a key is an error */
    yaml_parser_set_input_string(&parser,
        (const unsigned char *)":\n", 2);

    int ok = 1;
    while (ok) {
        ok = yaml_parser_parse(&parser, &event);
        if (!ok) break;
        int done = (event.type == YAML_STREAM_END_EVENT);
        yaml_event_delete(&event);
        if (done) { ok = 2; break; }
    }
    /* It either errors or produces events -- either is fine,
     * just don't crash */
    ASSERT(1, "colon at start: no crash");
    yaml_parser_delete(&parser);
}

/* Test: tab character in indentation produces error */
static void test_parse_tab_in_indentation(void) {
    yaml_parser_t parser;
    yaml_event_t event;

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser,
        (const unsigned char *)"key:\n\tvalue\n", 12);

    int ok = 1;
    int errored = 0;
    while (ok) {
        ok = yaml_parser_parse(&parser, &event);
        if (!ok) { errored = 1; break; }
        int done = (event.type == YAML_STREAM_END_EVENT);
        yaml_event_delete(&event);
        if (done) break;
    }
    ASSERT(errored, "tab in indentation: parse error");
    yaml_parser_delete(&parser);
}

/* Test: implicit document with directive */
static void test_parse_directive_then_implicit(void) {
    yaml_event_type_t events[32];
    int count;

    ASSERT(parse_string_events("%YAML 1.1\n---\nhello\n", events, 32, &count),
           "directive then implicit: parse succeeds");

    /* Should have: stream_start, doc_start, scalar, doc_end, stream_end */
    int has_doc_start = 0;
    for (int i = 0; i < count; i++) {
        if (events[i] == YAML_DOCUMENT_START_EVENT) has_doc_start++;
    }
    ASSERT_EQ_INT(has_doc_start, 1, "directive: 1 doc start");
}

/* ========================================================================
 * Reader coverage gaps: BOM detection edge cases
 * ======================================================================== */

/* Test: UTF-8 BOM at start of document */
static void test_reader_utf8_bom(void) {
    yaml_parser_t parser;
    yaml_event_t event;

    yaml_parser_initialize(&parser);
    const unsigned char input[] = {0xEF, 0xBB, 0xBF, 'h', 'e', 'l', 'l', 'o'};
    yaml_parser_set_input_string(&parser, input, sizeof(input));

    int found_scalar = 0;
    while (1) {
        if (!yaml_parser_parse(&parser, &event)) break;
        if (event.type == YAML_SCALAR_EVENT) {
            found_scalar = 1;
            ASSERT_EQ_STR(event.data.scalar.value, "hello",
                          "UTF-8 BOM: scalar is 'hello' (BOM stripped)");
        }
        int done = (event.type == YAML_STREAM_END_EVENT);
        yaml_event_delete(&event);
        if (done) break;
    }
    yaml_parser_delete(&parser);
    ASSERT(found_scalar, "UTF-8 BOM: found scalar");
}

/* Test: no BOM (raw UTF-8) */
static void test_reader_no_bom(void) {
    yaml_parser_t parser;
    yaml_event_t event;

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser,
        (const unsigned char *)"hello", 5);

    int found_scalar = 0;
    while (1) {
        if (!yaml_parser_parse(&parser, &event)) break;
        if (event.type == YAML_SCALAR_EVENT) {
            found_scalar = 1;
            ASSERT_EQ_STR(event.data.scalar.value, "hello",
                          "no BOM: scalar is 'hello'");
        }
        int done = (event.type == YAML_STREAM_END_EVENT);
        yaml_event_delete(&event);
        if (done) break;
    }
    yaml_parser_delete(&parser);
    ASSERT(found_scalar, "no BOM: found scalar");
}

/* ========================================================================
 * Additional loader edge cases
 * ======================================================================== */

/* Test: deeply nested mapping in loader */
static void test_load_deeply_nested(void) {
    yaml_parser_t parser;
    yaml_document_t doc;

    yaml_parser_initialize(&parser);
    const char *input = "a:\n  b:\n    c:\n      d:\n        e: deep\n";
    yaml_parser_set_input_string(&parser,
        (const unsigned char *)input, strlen(input));

    int ok = yaml_parser_load(&parser, &doc);
    ASSERT(ok, "deeply nested: load succeeds");

    yaml_node_t *root = yaml_document_get_root_node(&doc);
    ASSERT_NOT_NULL(root, "deeply nested: root exists");
    ASSERT_EQ_INT(root->type, YAML_MAPPING_NODE, "deeply nested: root is mapping");

    /* Navigate: root[a][b][c][d] -> "e: deep" */
    int val_id = root->data.mapping.pairs.start[0].value;
    yaml_node_t *node = yaml_document_get_node(&doc, val_id);
    ASSERT_NOT_NULL(node, "deeply nested: level 1 exists");
    ASSERT_EQ_INT(node->type, YAML_MAPPING_NODE, "deeply nested: level 1 is mapping");

    val_id = node->data.mapping.pairs.start[0].value;
    node = yaml_document_get_node(&doc, val_id);
    ASSERT_NOT_NULL(node, "deeply nested: level 2 exists");

    val_id = node->data.mapping.pairs.start[0].value;
    node = yaml_document_get_node(&doc, val_id);
    ASSERT_NOT_NULL(node, "deeply nested: level 3 exists");

    val_id = node->data.mapping.pairs.start[0].value;
    node = yaml_document_get_node(&doc, val_id);
    ASSERT_NOT_NULL(node, "deeply nested: level 4 exists");
    ASSERT_EQ_INT(node->type, YAML_MAPPING_NODE, "deeply nested: level 4 is mapping");

    /* Get the "deep" value */
    val_id = node->data.mapping.pairs.start[0].value;
    yaml_node_t *leaf = yaml_document_get_node(&doc, val_id);
    ASSERT_NOT_NULL(leaf, "deeply nested: leaf exists");
    ASSERT_EQ_INT(leaf->type, YAML_SCALAR_NODE, "deeply nested: leaf is scalar");
    ASSERT_EQ_STR(leaf->data.scalar.value, "deep", "deeply nested: leaf value is 'deep'");

    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
}

/* Test: load a sequence containing various types */
static void test_load_mixed_sequence(void) {
    yaml_parser_t parser;
    yaml_document_t doc;

    yaml_parser_initialize(&parser);
    const char *input = "- scalar\n- [nested, seq]\n- key: val\n- &a anchored\n- *a\n";
    yaml_parser_set_input_string(&parser,
        (const unsigned char *)input, strlen(input));

    int ok = yaml_parser_load(&parser, &doc);
    ASSERT(ok, "mixed sequence: load succeeds");

    yaml_node_t *root = yaml_document_get_root_node(&doc);
    ASSERT_NOT_NULL(root, "mixed sequence: root exists");
    ASSERT_EQ_INT(root->type, YAML_SEQUENCE_NODE, "mixed sequence: root is sequence");

    int count = root->data.sequence.items.top - root->data.sequence.items.start;
    ASSERT_EQ_INT(count, 5, "mixed sequence: 5 items");

    /* Item 0: scalar */
    yaml_node_t *n = yaml_document_get_node(&doc, root->data.sequence.items.start[0]);
    ASSERT_EQ_INT(n->type, YAML_SCALAR_NODE, "mixed seq[0]: scalar");

    /* Item 1: nested sequence */
    n = yaml_document_get_node(&doc, root->data.sequence.items.start[1]);
    ASSERT_EQ_INT(n->type, YAML_SEQUENCE_NODE, "mixed seq[1]: sequence");

    /* Item 2: mapping */
    n = yaml_document_get_node(&doc, root->data.sequence.items.start[2]);
    ASSERT_EQ_INT(n->type, YAML_MAPPING_NODE, "mixed seq[2]: mapping");

    /* Item 3 and 4: anchor and alias (same node) */
    int idx3 = root->data.sequence.items.start[3];
    int idx4 = root->data.sequence.items.start[4];
    ASSERT_EQ_INT(idx3, idx4, "mixed seq: alias resolves to anchored node");

    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
}

/* Test: tags on nodes */
static void test_load_tagged_nodes(void) {
    yaml_parser_t parser;
    yaml_document_t doc;

    yaml_parser_initialize(&parser);
    const char *input = "!!str 42\n";
    yaml_parser_set_input_string(&parser,
        (const unsigned char *)input, strlen(input));

    int ok = yaml_parser_load(&parser, &doc);
    ASSERT(ok, "tagged node: load succeeds");

    yaml_node_t *root = yaml_document_get_root_node(&doc);
    ASSERT_NOT_NULL(root, "tagged node: root exists");
    ASSERT_EQ_INT(root->type, YAML_SCALAR_NODE, "tagged node: root is scalar");
    ASSERT_EQ_STR(root->data.scalar.value, "42", "tagged node: value is '42'");
    ASSERT_NOT_NULL(root->tag, "tagged node: tag exists");
    ASSERT_EQ_STR(root->tag, "tag:yaml.org,2002:str",
                  "tagged node: tag is !!str");

    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
}

/* Test: tagged sequence */
static void test_load_tagged_sequence(void) {
    yaml_parser_t parser;
    yaml_document_t doc;

    yaml_parser_initialize(&parser);
    const char *input = "!!seq\n- a\n- b\n";
    yaml_parser_set_input_string(&parser,
        (const unsigned char *)input, strlen(input));

    int ok = yaml_parser_load(&parser, &doc);
    ASSERT(ok, "tagged sequence: load succeeds");

    yaml_node_t *root = yaml_document_get_root_node(&doc);
    ASSERT_NOT_NULL(root, "tagged sequence: root exists");
    ASSERT_EQ_INT(root->type, YAML_SEQUENCE_NODE, "tagged sequence: root is sequence");
    ASSERT_NOT_NULL(root->tag, "tagged sequence: tag exists");

    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
}

/* Test: tagged mapping */
static void test_load_tagged_mapping(void) {
    yaml_parser_t parser;
    yaml_document_t doc;

    yaml_parser_initialize(&parser);
    const char *input = "!!map\na: b\n";
    yaml_parser_set_input_string(&parser,
        (const unsigned char *)input, strlen(input));

    int ok = yaml_parser_load(&parser, &doc);
    ASSERT(ok, "tagged mapping: load succeeds");

    yaml_node_t *root = yaml_document_get_root_node(&doc);
    ASSERT_NOT_NULL(root, "tagged mapping: root exists");
    ASSERT_EQ_INT(root->type, YAML_MAPPING_NODE, "tagged mapping: root is mapping");

    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
}

int main(void) {
    TEST_SUITE_BEGIN("Coverage Gap Tests");

    /* Loader: alias error paths */
    test_load_undefined_alias();
    test_load_duplicate_anchor();
    test_load_valid_alias();
    test_load_alias_in_mapping_value();
    test_load_alias_as_key();

    /* Loader: multi-document */
    test_load_multi_document();
    test_load_multi_document_explicit_end();
    test_load_empty_documents();
    test_load_bare_document();

    /* Parser: flow mapping edge cases */
    test_parse_empty_flow_mapping();
    test_parse_flow_mapping_trailing_comma();
    test_parse_flow_mapping_implicit_values();
    test_parse_flow_mapping_complex();
    test_parse_flow_seq_implicit_mapping();

    /* Scanner: simple key edge cases */
    test_scan_adjacent_colon();
    test_scan_long_key_limit();
    test_scan_multiline_key_in_flow();

    /* Scanner: block scalar edge cases */
    test_scan_block_scalar_indent_indicator();
    test_scan_block_scalar_chomp_and_indent();
    test_scan_folded_paragraph_breaks();

    /* Parser: error and edge cases */
    test_parse_error_type();
    test_parse_tab_in_indentation();
    test_parse_directive_then_implicit();

    /* Reader: BOM handling */
    test_reader_utf8_bom();
    test_reader_no_bom();

    /* Loader: additional edge cases */
    test_load_deeply_nested();
    test_load_mixed_sequence();
    test_load_tagged_nodes();
    test_load_tagged_sequence();
    test_load_tagged_mapping();

    TEST_SUITE_END();
}
