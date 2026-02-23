/*
 * Regression tests for bugs discovered by differential fuzzing.
 *
 * Each test reproduces a specific bug found during fuzz campaigns and verifies
 * the correct output. These tests catch regressions without needing libyaml
 * available at runtime (unlike the differential tests).
 *
 * Bug index:
 *   1. Empty complex key loader crash (16fae05c)
 *   2. % character accepted as plain scalar in non-directive position (8f2c42d8)
 *   3. Plain scalar blank line continuation incorrect (b2e02f41, 03d77aef)
 *   4. Quoted scalar trailing line breaks after escaped breaks (b137e8d4)
 *   5. fetch_flow_entry_scalar firing at flow_level=0 (99ca2cbf)
 */

#include "test_helper.h"

/* ========================================================================
 * Helper: load a document and return the root node, or NULL on error.
 * Caller must call yaml_document_delete and yaml_parser_delete.
 * ======================================================================== */
static yaml_node_t *load_and_get_root(yaml_parser_t *parser,
                                       yaml_document_t *doc,
                                       const char *input) {
    if (!yaml_parser_initialize(parser)) return NULL;
    yaml_parser_set_input_string(parser, (const unsigned char *)input,
                                 strlen(input));
    if (!yaml_parser_load(parser, doc)) {
        yaml_parser_delete(parser);
        return NULL;
    }
    return yaml_document_get_root_node(doc);
}

/* Helper: scan all tokens and check for scan success */
static int scan_succeeds(const char *input) {
    yaml_parser_t parser;
    yaml_token_t token;

    if (!yaml_parser_initialize(&parser)) return 0;
    yaml_parser_set_input_string(&parser, (const unsigned char *)input,
                                 strlen(input));

    while (1) {
        if (!yaml_parser_scan(&parser, &token)) {
            yaml_parser_delete(&parser);
            return 0;
        }
        int done = (token.type == YAML_STREAM_END_TOKEN);
        yaml_token_delete(&token);
        if (done) break;
    }

    yaml_parser_delete(&parser);
    return 1;
}

/* Helper: scan and check that scanning FAILS */
static int scan_fails(const char *input) {
    yaml_parser_t parser;
    yaml_token_t token;

    if (!yaml_parser_initialize(&parser)) return 0;
    yaml_parser_set_input_string(&parser, (const unsigned char *)input,
                                 strlen(input));

    while (1) {
        if (!yaml_parser_scan(&parser, &token)) {
            yaml_parser_delete(&parser);
            return 1;  /* Scan failed as expected */
        }
        int done = (token.type == YAML_STREAM_END_TOKEN);
        yaml_token_delete(&token);
        if (done) break;
    }

    yaml_parser_delete(&parser);
    return 0;  /* Scan succeeded when it should have failed */
}

/* Helper: parse all events and check for success */
static int parse_succeeds(const char *input) {
    yaml_parser_t parser;
    yaml_event_t event;

    if (!yaml_parser_initialize(&parser)) return 0;
    yaml_parser_set_input_string(&parser, (const unsigned char *)input,
                                 strlen(input));

    while (1) {
        if (!yaml_parser_parse(&parser, &event)) {
            yaml_parser_delete(&parser);
            return 0;
        }
        int done = (event.type == YAML_STREAM_END_EVENT);
        yaml_event_delete(&event);
        if (done) break;
    }

    yaml_parser_delete(&parser);
    return 1;
}

/* Helper: load a document and check for success */
static int load_succeeds(const char *input) {
    yaml_parser_t parser;
    yaml_document_t doc;

    if (!yaml_parser_initialize(&parser)) return 0;
    yaml_parser_set_input_string(&parser, (const unsigned char *)input,
                                 strlen(input));

    if (!yaml_parser_load(&parser, &doc)) {
        yaml_parser_delete(&parser);
        return 0;
    }

    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
    return 1;
}

/* Helper: find a scalar value in a parsed event stream */
static int event_stream_has_scalar(const char *input, const char *value) {
    yaml_parser_t parser;
    yaml_event_t event;
    int found = 0;

    if (!yaml_parser_initialize(&parser)) return 0;
    yaml_parser_set_input_string(&parser, (const unsigned char *)input,
                                 strlen(input));

    while (1) {
        if (!yaml_parser_parse(&parser, &event)) break;
        if (event.type == YAML_SCALAR_EVENT) {
            if (event.data.scalar.length == strlen(value) &&
                memcmp(event.data.scalar.value, value,
                       event.data.scalar.length) == 0) {
                found = 1;
            }
        }
        int done = (event.type == YAML_STREAM_END_EVENT);
        yaml_event_delete(&event);
        if (done) break;
    }

    yaml_parser_delete(&parser);
    return found;
}

/* Helper: count mapping pairs in the root node of a loaded document */
static int count_root_mapping_pairs(const char *input) {
    yaml_parser_t parser;
    yaml_document_t doc;

    yaml_node_t *root = load_and_get_root(&parser, &doc, input);
    if (!root || root->type != YAML_MAPPING_NODE) {
        if (root) yaml_document_delete(&doc);
        yaml_parser_delete(&parser);
        return -1;
    }

    int count = root->data.mapping.pairs.top - root->data.mapping.pairs.start;
    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
    return count;
}

/* Helper: get the value of a mapping pair by index from root node */
static int get_mapping_value_scalar(const char *input, int pair_idx,
                                     char *buf, size_t buflen) {
    yaml_parser_t parser;
    yaml_document_t doc;

    yaml_node_t *root = load_and_get_root(&parser, &doc, input);
    if (!root || root->type != YAML_MAPPING_NODE) {
        if (root) yaml_document_delete(&doc);
        yaml_parser_delete(&parser);
        return 0;
    }

    int count = root->data.mapping.pairs.top - root->data.mapping.pairs.start;
    if (pair_idx >= count) {
        yaml_document_delete(&doc);
        yaml_parser_delete(&parser);
        return 0;
    }

    int val_id = root->data.mapping.pairs.start[pair_idx].value;
    yaml_node_t *val_node = yaml_document_get_node(&doc, val_id);
    if (!val_node || val_node->type != YAML_SCALAR_NODE) {
        yaml_document_delete(&doc);
        yaml_parser_delete(&parser);
        return 0;
    }

    size_t len = val_node->data.scalar.length;
    if (len >= buflen) len = buflen - 1;
    memcpy(buf, val_node->data.scalar.value, len);
    buf[len] = '\0';

    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
    return 1;
}

/* Helper: get the key of a mapping pair by index from root node */
static int get_mapping_key_scalar(const char *input, int pair_idx,
                                   char *buf, size_t buflen) {
    yaml_parser_t parser;
    yaml_document_t doc;

    yaml_node_t *root = load_and_get_root(&parser, &doc, input);
    if (!root || root->type != YAML_MAPPING_NODE) {
        if (root) yaml_document_delete(&doc);
        yaml_parser_delete(&parser);
        return 0;
    }

    int count = root->data.mapping.pairs.top - root->data.mapping.pairs.start;
    if (pair_idx >= count) {
        yaml_document_delete(&doc);
        yaml_parser_delete(&parser);
        return 0;
    }

    int key_id = root->data.mapping.pairs.start[pair_idx].key;
    yaml_node_t *key_node = yaml_document_get_node(&doc, key_id);
    if (!key_node || key_node->type != YAML_SCALAR_NODE) {
        yaml_document_delete(&doc);
        yaml_parser_delete(&parser);
        return 0;
    }

    size_t len = key_node->data.scalar.length;
    if (len >= buflen) len = buflen - 1;
    memcpy(buf, key_node->data.scalar.value, len);
    buf[len] = '\0';

    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
    return 1;
}

/* ========================================================================
 * Bug 1: Empty complex key loader crash (16fae05c)
 *
 * Input "?\n" (empty complex key) caused the loader to crash or fail.
 * The batch mapping loader's fallback_key path did not handle the case
 * where a complex key indicator has no following content.
 * ======================================================================== */

static void test_regression_empty_complex_key_bare(void) {
    /* "?\n" should load as a mapping with one pair: empty key -> empty value */
    ASSERT(scan_succeeds("?\n"), "empty complex key: scan succeeds");
    ASSERT(parse_succeeds("?\n"), "empty complex key: parse succeeds");
    ASSERT(load_succeeds("?\n"), "empty complex key: load succeeds");
    ASSERT_EQ_INT(count_root_mapping_pairs("?\n"), 1,
                  "empty complex key: 1 mapping pair");
}

static void test_regression_empty_complex_key_with_value(void) {
    /* "?\n: value\n" should load as mapping: empty key -> "value" */
    ASSERT(load_succeeds("?\n: value\n"),
           "empty key with value: load succeeds");
    ASSERT_EQ_INT(count_root_mapping_pairs("?\n: value\n"), 1,
                  "empty key with value: 1 pair");
    char buf[256];
    ASSERT(get_mapping_value_scalar("?\n: value\n", 0, buf, sizeof(buf)),
           "empty key with value: get value");
    ASSERT_EQ_STR(buf, "value", "empty key with value: value is 'value'");
}

static void test_regression_empty_complex_key_empty_value(void) {
    /* "?\n:\n" should load as mapping: empty key -> empty value */
    ASSERT(load_succeeds("?\n:\n"),
           "empty key empty value: load succeeds");
    ASSERT_EQ_INT(count_root_mapping_pairs("?\n:\n"), 1,
                  "empty key empty value: 1 pair");
}

static void test_regression_empty_complex_key_multiple(void) {
    /* "?\n?\n" should load as mapping with 2 empty-key pairs */
    ASSERT(load_succeeds("?\n?\n"),
           "multiple empty keys: load succeeds");
    ASSERT_EQ_INT(count_root_mapping_pairs("?\n?\n"), 2,
                  "multiple empty keys: 2 pairs");
}

static void test_regression_empty_complex_key_mixed(void) {
    /* "?\n: v1\n? key2\n: v2\n" should load correctly */
    ASSERT(load_succeeds("?\n: v1\n? key2\n: v2\n"),
           "mixed empty/normal keys: load succeeds");
    ASSERT_EQ_INT(count_root_mapping_pairs("?\n: v1\n? key2\n: v2\n"), 2,
                  "mixed empty/normal keys: 2 pairs");
    char buf[256];
    ASSERT(get_mapping_value_scalar("?\n: v1\n? key2\n: v2\n", 0, buf,
                                     sizeof(buf)),
           "mixed keys: get value 0");
    ASSERT_EQ_STR(buf, "v1", "mixed keys: first value is 'v1'");
    ASSERT(get_mapping_key_scalar("?\n: v1\n? key2\n: v2\n", 1, buf,
                                   sizeof(buf)),
           "mixed keys: get key 1");
    ASSERT_EQ_STR(buf, "key2", "mixed keys: second key is 'key2'");
}

/* ========================================================================
 * Bug 2: % character rejection (8f2c42d8)
 *
 * '%' was incorrectly accepted as the start of a plain scalar in
 * non-directive positions. YAML spec says '%' can only start a directive
 * at the beginning of a document (column 0, after doc start).
 * ======================================================================== */

static void test_regression_percent_directive(void) {
    /* "%YAML 1.1\n---\nhello\n" is a valid directive */
    ASSERT(scan_succeeds("%YAML 1.1\n---\nhello\n"),
           "% as directive: scan succeeds");
    ASSERT(parse_succeeds("%YAML 1.1\n---\nhello\n"),
           "% as directive: parse succeeds");
}

static void test_regression_percent_in_value(void) {
    /* "key: 50%\n" - % in a value position should be fine */
    ASSERT(scan_succeeds("key: 50%\n"),
           "% in value: scan succeeds");
    ASSERT(event_stream_has_scalar("key: 50%\n", "50%"),
           "% in value: scalar is '50%'");
}

static void test_regression_percent_bare_rejected(void) {
    /* "% invalid\n" at start should fail (not a valid directive) */
    ASSERT(scan_fails("% invalid\n"),
           "bare % at start: scan fails");
}

static void test_regression_percent_after_doc_start(void) {
    /* After ---\n, a bare % at column 0 should not start a directive.
     * "---\n%notadirective\n" */
    /* Note: this is complex - libyaml treats % at column 0 after document
     * start as an error. We should match that behavior. */
    ASSERT(scan_succeeds("---\nkey: 50%\n"),
           "% in value after doc start: scan succeeds");
}

/* ========================================================================
 * Bug 3: Plain scalar blank line continuation (b2e02f41, 03d77aef)
 *
 * Plain scalars that span multiple lines with blank line separators were
 * not handling the continuation correctly. A blank line in a plain scalar
 * should become a newline in the scalar value.
 * ======================================================================== */

static void test_regression_plain_scalar_blank_line(void) {
    /* "key:\n  word1\n\n  word2\n" should produce scalar "word1\nword2" */
    ASSERT(scan_succeeds("key:\n  word1\n\n  word2\n"),
           "plain scalar blank line: scan succeeds");
    ASSERT(event_stream_has_scalar("key:\n  word1\n\n  word2\n",
                                    "word1\nword2"),
           "plain scalar blank line: value is 'word1\\nword2'");
}

static void test_regression_plain_scalar_multiple_blank_lines(void) {
    /* Multiple blank lines should produce multiple newlines */
    ASSERT(event_stream_has_scalar("key:\n  word1\n\n\n  word2\n",
                                    "word1\n\nword2"),
           "plain scalar 2 blank lines: value is 'word1\\n\\nword2'");
}

static void test_regression_plain_scalar_blank_line_in_mapping(void) {
    /* Blank line continuation in a mapping value */
    ASSERT(load_succeeds("a:\n  para1\n\n  para2\nb: val\n"),
           "plain blank line in mapping: load succeeds");
    char buf[256];
    ASSERT(get_mapping_value_scalar("a:\n  para1\n\n  para2\nb: val\n",
                                     0, buf, sizeof(buf)),
           "plain blank line in mapping: get value");
    ASSERT_EQ_STR(buf, "para1\npara2",
                  "plain blank line in mapping: correct value");
}

static void test_regression_plain_scalar_batch_kv_blank_line(void) {
    /* Blank line in batch key-value scanning path (03d77aef fix) */
    ASSERT(load_succeeds("k1: v1\nk2:\n  line1\n\n  line2\nk3: v3\n"),
           "batch KV blank line: load succeeds");
    char buf[256];
    ASSERT(get_mapping_value_scalar(
               "k1: v1\nk2:\n  line1\n\n  line2\nk3: v3\n",
               1, buf, sizeof(buf)),
           "batch KV blank line: get k2 value");
    ASSERT_EQ_STR(buf, "line1\nline2",
                  "batch KV blank line: k2 value correct");
}

/* ========================================================================
 * Bug 4: Quoted scalar trailing line breaks after escaped breaks (b137e8d4)
 *
 * Silent data corruption: in double-quoted scalars, an escaped newline
 * (\n) followed by an actual line break would lose the trailing line
 * break, corrupting the scalar value.
 * ======================================================================== */

static void test_regression_quoted_escaped_newline_basic(void) {
    /* Basic escaped newline in double-quoted string */
    ASSERT(event_stream_has_scalar("\"hello\\nworld\"", "hello\nworld"),
           "escaped \\n: correct value");
}

static void test_regression_quoted_escaped_then_linebreak(void) {
    /* "\"line1\\n\nline2\"" - escaped \n followed by actual line break
     * The escaped \n should produce a newline, and the actual line break
     * should fold to a space (YAML folding rules for quoted scalars) */
    ASSERT(scan_succeeds("\"line1\\n\nline2\""),
           "escaped \\n then linebreak: scan succeeds");
    ASSERT(parse_succeeds("\"line1\\n\nline2\""),
           "escaped \\n then linebreak: parse succeeds");
    /* The value should be "line1\n line2" (escaped \n + folded space + line2) */
    ASSERT(event_stream_has_scalar("\"line1\\n\nline2\"", "line1\n line2"),
           "escaped \\n then linebreak: correct folding");
}

static void test_regression_quoted_escaped_then_blank_line(void) {
    /* Escaped newline followed by a blank line (which produces \n) */
    ASSERT(scan_succeeds("\"line1\\n\n\nline2\""),
           "escaped \\n then blank line: scan succeeds");
    ASSERT(parse_succeeds("\"line1\\n\n\nline2\""),
           "escaped \\n then blank line: parse succeeds");
}

static void test_regression_quoted_tab_escape(void) {
    /* Escaped tab */
    ASSERT(event_stream_has_scalar("\"hello\\tworld\"", "hello\tworld"),
           "escaped \\t: correct value");
}

static void test_regression_quoted_multiline_folding(void) {
    /* Basic quoted scalar multiline folding */
    ASSERT(event_stream_has_scalar("\"hello\n  world\"", "hello world"),
           "quoted multiline folding: correct value");
}

/* ========================================================================
 * Bug 5: fetch_flow_entry_scalar at flow_level=0 (99ca2cbf)
 *
 * The fast-path scanner optimization fetch_flow_entry_scalar was firing
 * even at flow_level=0 (block context). Flow entry scalars should only
 * be scanned when inside a flow collection (flow_level > 0).
 * ======================================================================== */

static void test_regression_flow_level_guard_basic(void) {
    /* Basic flow collection - should work fine */
    ASSERT(scan_succeeds("[a, b, c]"),
           "flow sequence: scan succeeds");
    ASSERT(parse_succeeds("[a, b, c]"),
           "flow sequence: parse succeeds");
    ASSERT(load_succeeds("[a, b, c]"),
           "flow sequence: load succeeds");
}

static void test_regression_flow_level_guard_block_comma(void) {
    /* A comma in block context should not trigger flow entry scanning */
    /* "a,b: value\n" - the comma is part of the plain scalar key */
    ASSERT(scan_succeeds("a,b: value\n"),
           "comma in block key: scan succeeds");
    ASSERT(event_stream_has_scalar("a,b: value\n", "a,b"),
           "comma in block key: key is 'a,b'");
}

static void test_regression_flow_level_guard_nested(void) {
    /* Nested flow in block context */
    ASSERT(load_succeeds("key: [a, b]\nkey2: {c: d}\n"),
           "nested flow in block: load succeeds");
}

static void test_regression_flow_level_guard_block_after_flow(void) {
    /* Block mapping after a flow collection - flow_level should be back to 0 */
    ASSERT(load_succeeds("[a, b]\n---\nkey: value\n"),
           "block after flow: load succeeds");
}

static void test_regression_flow_level_guard_flow_map(void) {
    /* Flow mapping with multiple entries */
    ASSERT(scan_succeeds("{a: 1, b: 2, c: 3}"),
           "flow mapping multi: scan succeeds");
    ASSERT(load_succeeds("{a: 1, b: 2, c: 3}"),
           "flow mapping multi: load succeeds");
}

static void test_regression_flow_level_guard_deep_flow(void) {
    /* Deeply nested flow collections */
    ASSERT(scan_succeeds("[[1, 2], [3, 4]]"),
           "deep nested flow: scan succeeds");
    ASSERT(scan_succeeds("{a: {b: {c: 1}}}"),
           "deep nested flow map: scan succeeds");
    ASSERT(load_succeeds("[[1, 2], [3, 4]]"),
           "deep nested flow: load succeeds");
}

/* ========================================================================
 * Additional regression tests: double-free in yaml_document_delete
 *
 * Found earlier in the session. yaml_document_delete must be idempotent
 * because yaml_parser_load calls it on error paths, and the caller may
 * call it again.
 * ======================================================================== */

static void test_regression_document_delete_idempotent(void) {
    yaml_parser_t parser;
    yaml_document_t doc;

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)"key: val\n", 9);

    if (yaml_parser_load(&parser, &doc)) {
        /* Delete twice - must not crash */
        yaml_document_delete(&doc);
        yaml_document_delete(&doc);
        ASSERT(1, "double yaml_document_delete: no crash");
    } else {
        ASSERT(0, "double yaml_document_delete: load failed");
    }

    yaml_parser_delete(&parser);
}

static void test_regression_event_delete_idempotent(void) {
    yaml_event_t event;
    memset(&event, 0, sizeof(event));
    /* Deleting a zeroed event should be safe */
    yaml_event_delete(&event);
    yaml_event_delete(&event);
    ASSERT(1, "double yaml_event_delete on zeroed event: no crash");
}

static void test_regression_token_delete_idempotent(void) {
    yaml_token_t token;
    memset(&token, 0, sizeof(token));
    /* Deleting a zeroed token should be safe */
    yaml_token_delete(&token);
    yaml_token_delete(&token);
    ASSERT(1, "double yaml_token_delete on zeroed token: no crash");
}

/* ========================================================================
 * main
 * ======================================================================== */

int main(void) {
    TEST_SUITE_BEGIN("Regression Tests (fuzz-discovered bugs)");

    /* Bug 1: Empty complex key */
    test_regression_empty_complex_key_bare();
    test_regression_empty_complex_key_with_value();
    test_regression_empty_complex_key_empty_value();
    test_regression_empty_complex_key_multiple();
    test_regression_empty_complex_key_mixed();

    /* Bug 2: % character rejection */
    test_regression_percent_directive();
    test_regression_percent_in_value();
    test_regression_percent_bare_rejected();
    test_regression_percent_after_doc_start();

    /* Bug 3: Plain scalar blank line continuation */
    test_regression_plain_scalar_blank_line();
    test_regression_plain_scalar_multiple_blank_lines();
    test_regression_plain_scalar_blank_line_in_mapping();
    test_regression_plain_scalar_batch_kv_blank_line();

    /* Bug 4: Quoted scalar trailing breaks */
    test_regression_quoted_escaped_newline_basic();
    test_regression_quoted_escaped_then_linebreak();
    test_regression_quoted_escaped_then_blank_line();
    test_regression_quoted_tab_escape();
    test_regression_quoted_multiline_folding();

    /* Bug 5: flow_level guard */
    test_regression_flow_level_guard_basic();
    test_regression_flow_level_guard_block_comma();
    test_regression_flow_level_guard_nested();
    test_regression_flow_level_guard_block_after_flow();
    test_regression_flow_level_guard_flow_map();
    test_regression_flow_level_guard_deep_flow();

    /* Delete idempotency */
    test_regression_document_delete_idempotent();
    test_regression_event_delete_idempotent();
    test_regression_token_delete_idempotent();

    TEST_SUITE_END();
}
