/*
 * Tests targeting the batch loader optimization paths in loader.c.
 *
 * The batch loader has 3 fast paths:
 *   1. Block mapping pairs (yaml_parser_load_mapping_pairs_batch)
 *   2. Block sequence items (yaml_parser_load_sequence_items_batch)
 *   3. Flow sequence items (yaml_parser_load_flow_sequence_items_batch)
 *
 * Each fast path handles plain scalar items directly without going through
 * the full parser state machine. Non-plain-scalar items cause a fallback
 * to the normal loading path. These tests exercise:
 *   - Pure batch paths (all plain scalars)
 *   - Mixed batch/fallback (some plain scalars, some complex items)
 *   - Edge cases at batch/normal transitions
 *   - Error handling in batch paths
 */

#include "test_helper.h"

/* ========== Helpers ========== */

/* Load document and return root node type, or YAML_NO_NODE on failure */
static yaml_node_type_t load_root_type(const char *input) {
    yaml_parser_t parser;
    yaml_document_t doc;
    yaml_node_type_t type = YAML_NO_NODE;
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)input,
                                 strlen(input));
    if (yaml_parser_load(&parser, &doc)) {
        yaml_node_t *root = yaml_document_get_root_node(&doc);
        if (root) type = root->type;
        yaml_document_delete(&doc);
    }
    yaml_parser_delete(&parser);
    return type;
}

/* Load and count mapping pairs in root */
static int load_mapping_count(const char *input) {
    yaml_parser_t parser;
    yaml_document_t doc;
    int count = -1;
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)input,
                                 strlen(input));
    if (yaml_parser_load(&parser, &doc)) {
        yaml_node_t *root = yaml_document_get_root_node(&doc);
        if (root && root->type == YAML_MAPPING_NODE)
            count = (int)(root->data.mapping.pairs.top -
                          root->data.mapping.pairs.start);
        yaml_document_delete(&doc);
    }
    yaml_parser_delete(&parser);
    return count;
}

/* Load and count sequence items in root */
static int load_seq_count(const char *input) {
    yaml_parser_t parser;
    yaml_document_t doc;
    int count = -1;
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)input,
                                 strlen(input));
    if (yaml_parser_load(&parser, &doc)) {
        yaml_node_t *root = yaml_document_get_root_node(&doc);
        if (root && root->type == YAML_SEQUENCE_NODE)
            count = (int)(root->data.sequence.items.top -
                          root->data.sequence.items.start);
        yaml_document_delete(&doc);
    }
    yaml_parser_delete(&parser);
    return count;
}

/* Load and get nth mapping value as scalar string (0-indexed) */
static int load_mapping_value(const char *input, int idx,
                               char *buf, size_t buflen) {
    yaml_parser_t parser;
    yaml_document_t doc;
    int ok = 0;
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)input,
                                 strlen(input));
    if (yaml_parser_load(&parser, &doc)) {
        yaml_node_t *root = yaml_document_get_root_node(&doc);
        if (root && root->type == YAML_MAPPING_NODE) {
            int count = (int)(root->data.mapping.pairs.top -
                              root->data.mapping.pairs.start);
            if (idx < count) {
                int vid = root->data.mapping.pairs.start[idx].value;
                yaml_node_t *v = yaml_document_get_node(&doc, vid);
                if (v && v->type == YAML_SCALAR_NODE) {
                    size_t len = v->data.scalar.length;
                    if (len >= buflen) len = buflen - 1;
                    memcpy(buf, v->data.scalar.value, len);
                    buf[len] = '\0';
                    ok = 1;
                }
            }
        }
        yaml_document_delete(&doc);
    }
    yaml_parser_delete(&parser);
    return ok;
}

/* Load and get nth mapping key as scalar string */
static int load_mapping_key(const char *input, int idx,
                              char *buf, size_t buflen) {
    yaml_parser_t parser;
    yaml_document_t doc;
    int ok = 0;
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)input,
                                 strlen(input));
    if (yaml_parser_load(&parser, &doc)) {
        yaml_node_t *root = yaml_document_get_root_node(&doc);
        if (root && root->type == YAML_MAPPING_NODE) {
            int count = (int)(root->data.mapping.pairs.top -
                              root->data.mapping.pairs.start);
            if (idx < count) {
                int kid = root->data.mapping.pairs.start[idx].key;
                yaml_node_t *k = yaml_document_get_node(&doc, kid);
                if (k && k->type == YAML_SCALAR_NODE) {
                    size_t len = k->data.scalar.length;
                    if (len >= buflen) len = buflen - 1;
                    memcpy(buf, k->data.scalar.value, len);
                    buf[len] = '\0';
                    ok = 1;
                }
            }
        }
        yaml_document_delete(&doc);
    }
    yaml_parser_delete(&parser);
    return ok;
}

/* Load and get nth sequence item as scalar string */
static int load_seq_item(const char *input, int idx,
                           char *buf, size_t buflen) {
    yaml_parser_t parser;
    yaml_document_t doc;
    int ok = 0;
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)input,
                                 strlen(input));
    if (yaml_parser_load(&parser, &doc)) {
        yaml_node_t *root = yaml_document_get_root_node(&doc);
        if (root && root->type == YAML_SEQUENCE_NODE) {
            int count = (int)(root->data.sequence.items.top -
                              root->data.sequence.items.start);
            if (idx < count) {
                int nid = root->data.sequence.items.start[idx];
                yaml_node_t *n = yaml_document_get_node(&doc, nid);
                if (n && n->type == YAML_SCALAR_NODE) {
                    size_t len = n->data.scalar.length;
                    if (len >= buflen) len = buflen - 1;
                    memcpy(buf, n->data.scalar.value, len);
                    buf[len] = '\0';
                    ok = 1;
                }
            }
        }
        yaml_document_delete(&doc);
    }
    yaml_parser_delete(&parser);
    return ok;
}

/* Load and get nth mapping value's node type */
static yaml_node_type_t load_mapping_value_type(const char *input, int idx) {
    yaml_parser_t parser;
    yaml_document_t doc;
    yaml_node_type_t type = YAML_NO_NODE;
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)input,
                                 strlen(input));
    if (yaml_parser_load(&parser, &doc)) {
        yaml_node_t *root = yaml_document_get_root_node(&doc);
        if (root && root->type == YAML_MAPPING_NODE) {
            int count = (int)(root->data.mapping.pairs.top -
                              root->data.mapping.pairs.start);
            if (idx < count) {
                int vid = root->data.mapping.pairs.start[idx].value;
                yaml_node_t *v = yaml_document_get_node(&doc, vid);
                if (v) type = v->type;
            }
        }
        yaml_document_delete(&doc);
    }
    yaml_parser_delete(&parser);
    return type;
}

/* Load and check if load succeeds */
static int load_ok(const char *input) {
    yaml_parser_t parser;
    yaml_document_t doc;
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)input,
                                 strlen(input));
    int ok = yaml_parser_load(&parser, &doc);
    if (ok) yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
    return ok;
}

/* Load and check that load FAILS */
static int load_fails(const char *input) {
    yaml_parser_t parser;
    yaml_document_t doc;
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)input,
                                 strlen(input));
    int ok = yaml_parser_load(&parser, &doc);
    if (ok) {
        yaml_document_delete(&doc);
        yaml_parser_delete(&parser);
        return 0;
    }
    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
    return 1;
}

/* ========== Block Mapping Batch Tests ========== */

static void test_batch_mapping_simple(void) {
    /* Pure batch path: all plain scalar key-value pairs */
    const char *input = "a: 1\nb: 2\nc: 3\n";
    ASSERT_EQ_INT(load_mapping_count(input), 3,
                  "simple batch mapping: 3 pairs");
    char buf[256];
    ASSERT(load_mapping_key(input, 0, buf, sizeof(buf)), "key 0 exists");
    ASSERT_EQ_STR(buf, "a", "key 0 = a");
    ASSERT(load_mapping_value(input, 0, buf, sizeof(buf)), "val 0 exists");
    ASSERT_EQ_STR(buf, "1", "val 0 = 1");
    ASSERT(load_mapping_value(input, 2, buf, sizeof(buf)), "val 2 exists");
    ASSERT_EQ_STR(buf, "3", "val 2 = 3");
}

static void test_batch_mapping_many_pairs(void) {
    /* Many pairs to stress batch allocation */
    char input[4096];
    int pos = 0;
    for (int i = 0; i < 50; i++)
        pos += snprintf(input + pos, sizeof(input) - pos, "k%d: v%d\n", i, i);
    ASSERT_EQ_INT(load_mapping_count(input), 50,
                  "many batch pairs: 50 pairs");
    char buf[256];
    ASSERT(load_mapping_value(input, 49, buf, sizeof(buf)), "val 49 exists");
    ASSERT_EQ_STR(buf, "v49", "val 49 = v49");
}

static void test_batch_mapping_empty_values(void) {
    /* Empty values trigger the batch_empty_value label */
    const char *input = "a:\nb:\nc: val\nd:\n";
    ASSERT_EQ_INT(load_mapping_count(input), 4,
                  "empty values: 4 pairs");
    char buf[256];
    ASSERT(load_mapping_value(input, 0, buf, sizeof(buf)), "val 0 exists");
    ASSERT_EQ_STR(buf, "", "val 0 is empty");
    ASSERT(load_mapping_value(input, 1, buf, sizeof(buf)), "val 1 exists");
    ASSERT_EQ_STR(buf, "", "val 1 is empty");
    ASSERT(load_mapping_value(input, 2, buf, sizeof(buf)), "val 2 exists");
    ASSERT_EQ_STR(buf, "val", "val 2 = val");
}

static void test_batch_mapping_fallback_nested_mapping(void) {
    /* Nested mapping forces fallback from batch to normal path */
    const char *input = "a: 1\nb:\n  c: 2\n  d: 3\ne: 4\n";
    ASSERT_EQ_INT(load_mapping_count(input), 3,
                  "nested mapping fallback: 3 top-level pairs");
    ASSERT_EQ_INT((int)load_mapping_value_type(input, 1), YAML_MAPPING_NODE,
                  "val 1 is mapping");
    char buf[256];
    ASSERT(load_mapping_value(input, 0, buf, sizeof(buf)), "val 0 exists");
    ASSERT_EQ_STR(buf, "1", "val 0 = 1");
    ASSERT(load_mapping_value(input, 2, buf, sizeof(buf)), "val 2 exists");
    ASSERT_EQ_STR(buf, "4", "val 2 = 4");
}

static void test_batch_mapping_fallback_nested_sequence(void) {
    /* Nested sequence forces fallback from batch to normal path */
    const char *input = "a: 1\nb:\n  - x\n  - y\nc: 3\n";
    ASSERT_EQ_INT(load_mapping_count(input), 3,
                  "nested seq fallback: 3 pairs");
    ASSERT_EQ_INT((int)load_mapping_value_type(input, 1), YAML_SEQUENCE_NODE,
                  "val 1 is sequence");
}

static void test_batch_mapping_fallback_flow(void) {
    /* Flow collection as value forces fallback */
    const char *input = "a: [1, 2]\nb: {c: d}\nc: val\n";
    ASSERT_EQ_INT(load_mapping_count(input), 3,
                  "flow fallback: 3 pairs");
    ASSERT_EQ_INT((int)load_mapping_value_type(input, 0), YAML_SEQUENCE_NODE,
                  "val 0 is sequence");
    ASSERT_EQ_INT((int)load_mapping_value_type(input, 1), YAML_MAPPING_NODE,
                  "val 1 is mapping");
}

static void test_batch_mapping_fallback_complex_key(void) {
    /* Complex key forces fallback */
    const char *input = "? complex\n: value\nsimple: val2\n";
    ASSERT_EQ_INT(load_mapping_count(input), 2,
                  "complex key fallback: 2 pairs");
    char buf[256];
    ASSERT(load_mapping_key(input, 0, buf, sizeof(buf)), "key 0 exists");
    ASSERT_EQ_STR(buf, "complex", "key 0 = complex");
}

static void test_batch_mapping_fallback_anchor(void) {
    /* Anchored value forces fallback */
    const char *input = "a: &ref value1\nb: *ref\nc: plain\n";
    ASSERT_EQ_INT(load_mapping_count(input), 3,
                  "anchor fallback: 3 pairs");
    char buf[256];
    ASSERT(load_mapping_value(input, 0, buf, sizeof(buf)), "val 0 exists");
    ASSERT_EQ_STR(buf, "value1", "val 0 = value1");
    ASSERT(load_mapping_value(input, 1, buf, sizeof(buf)), "val 1 exists");
    ASSERT_EQ_STR(buf, "value1", "val 1 = value1 (alias)");
}

static void test_batch_mapping_fallback_tag(void) {
    /* Tagged value forces fallback */
    const char *input = "a: !!int 42\nb: plain\n";
    ASSERT_EQ_INT(load_mapping_count(input), 2,
                  "tag fallback: 2 pairs");
    char buf[256];
    ASSERT(load_mapping_value(input, 0, buf, sizeof(buf)), "val 0 exists");
    ASSERT_EQ_STR(buf, "42", "val 0 = 42");
}

static void test_batch_mapping_fallback_quoted(void) {
    /* Quoted scalars may or may not be batchable depending on implementation */
    const char *input = "a: \"quoted\"\nb: 'single'\nc: plain\n";
    ASSERT_EQ_INT(load_mapping_count(input), 3,
                  "quoted fallback: 3 pairs");
    char buf[256];
    ASSERT(load_mapping_value(input, 0, buf, sizeof(buf)), "val 0 exists");
    ASSERT_EQ_STR(buf, "quoted", "val 0 = quoted");
    ASSERT(load_mapping_value(input, 1, buf, sizeof(buf)), "val 1 exists");
    ASSERT_EQ_STR(buf, "single", "val 1 = single");
}

static void test_batch_mapping_fallback_block_scalar(void) {
    /* Block scalar as value forces fallback */
    const char *input = "a: |\n  literal\n  text\nb: plain\n";
    ASSERT_EQ_INT(load_mapping_count(input), 2,
                  "block scalar fallback: 2 pairs");
    char buf[256];
    ASSERT(load_mapping_value(input, 0, buf, sizeof(buf)), "val 0 exists");
    ASSERT_EQ_STR(buf, "literal\ntext\n", "val 0 = literal text");
}

static void test_batch_mapping_alternating(void) {
    /* Alternating batch/fallback/batch.
     * f:\n  g: 6 creates f -> mapping{g:6}, so 7 top-level pairs. */
    const char *input =
        "a: 1\n"
        "b: 2\n"
        "c:\n"
        "  - nested\n"
        "d: 4\n"
        "e: 5\n"
        "f:\n"
        "  g: 6\n"
        "h: 7\n";
    ASSERT_EQ_INT(load_mapping_count(input), 7,
                  "alternating: 7 top-level pairs");
    char buf[256];
    ASSERT(load_mapping_value(input, 0, buf, sizeof(buf)), "val a exists");
    ASSERT_EQ_STR(buf, "1", "a = 1");
    ASSERT(load_mapping_value(input, 3, buf, sizeof(buf)), "val d exists");
    ASSERT_EQ_STR(buf, "4", "d = 4");
    /* f is at index 5, h at index 6 */
    ASSERT(load_mapping_value(input, 6, buf, sizeof(buf)), "val h exists");
    ASSERT_EQ_STR(buf, "7", "h = 7");
}

/* ========== Block Sequence Batch Tests ========== */

static void test_batch_seq_simple(void) {
    /* Pure batch path: all plain scalar items */
    const char *input = "- a\n- b\n- c\n";
    ASSERT_EQ_INT(load_seq_count(input), 3,
                  "simple batch seq: 3 items");
    char buf[256];
    ASSERT(load_seq_item(input, 0, buf, sizeof(buf)), "item 0 exists");
    ASSERT_EQ_STR(buf, "a", "item 0 = a");
    ASSERT(load_seq_item(input, 2, buf, sizeof(buf)), "item 2 exists");
    ASSERT_EQ_STR(buf, "c", "item 2 = c");
}

static void test_batch_seq_many_items(void) {
    /* Many items to stress batch allocation */
    char input[4096];
    int pos = 0;
    for (int i = 0; i < 100; i++)
        pos += snprintf(input + pos, sizeof(input) - pos, "- item%d\n", i);
    ASSERT_EQ_INT(load_seq_count(input), 100,
                  "many batch seq items: 100 items");
    char buf[256];
    ASSERT(load_seq_item(input, 99, buf, sizeof(buf)), "item 99 exists");
    ASSERT_EQ_STR(buf, "item99", "item 99 = item99");
}

static void test_batch_seq_fallback_nested_seq(void) {
    /* Nested sequence forces fallback */
    const char *input = "- a\n- - b\n  - c\n- d\n";
    ASSERT_EQ_INT(load_seq_count(input), 3,
                  "nested seq fallback: 3 top items");
    char buf[256];
    ASSERT(load_seq_item(input, 0, buf, sizeof(buf)), "item 0 exists");
    ASSERT_EQ_STR(buf, "a", "item 0 = a");
    ASSERT(load_seq_item(input, 2, buf, sizeof(buf)), "item 2 exists");
    ASSERT_EQ_STR(buf, "d", "item 2 = d");
}

static void test_batch_seq_fallback_mapping(void) {
    /* Mapping item forces fallback */
    const char *input = "- plain\n- key: val\n- plain2\n";
    ASSERT_EQ_INT(load_seq_count(input), 3,
                  "mapping in seq fallback: 3 items");
}

static void test_batch_seq_fallback_anchor(void) {
    /* Anchored item forces fallback */
    const char *input = "- &ref item1\n- *ref\n- plain\n";
    ASSERT_EQ_INT(load_seq_count(input), 3,
                  "anchor in seq fallback: 3 items");
    char buf[256];
    ASSERT(load_seq_item(input, 0, buf, sizeof(buf)), "item 0 exists");
    ASSERT_EQ_STR(buf, "item1", "item 0 = item1");
    ASSERT(load_seq_item(input, 1, buf, sizeof(buf)), "item 1 exists");
    ASSERT_EQ_STR(buf, "item1", "item 1 = item1 (alias)");
}

static void test_batch_seq_fallback_flow(void) {
    /* Flow collection as item forces fallback */
    const char *input = "- [a, b]\n- {c: d}\n- plain\n";
    ASSERT_EQ_INT(load_seq_count(input), 3,
                  "flow in seq fallback: 3 items");
}

static void test_batch_seq_empty_items(void) {
    /* Empty items (implicit null) */
    const char *input = "-\n-\n- val\n-\n";
    ASSERT_EQ_INT(load_seq_count(input), 4,
                  "empty seq items: 4 items");
    char buf[256];
    ASSERT(load_seq_item(input, 0, buf, sizeof(buf)), "item 0 exists");
    ASSERT_EQ_STR(buf, "", "item 0 is empty");
    ASSERT(load_seq_item(input, 2, buf, sizeof(buf)), "item 2 exists");
    ASSERT_EQ_STR(buf, "val", "item 2 = val");
}

/* ========== Flow Sequence Batch Tests ========== */

static void test_batch_flow_seq_simple(void) {
    /* Pure batch flow path: all plain scalars */
    const char *input = "[a, b, c, d]";
    ASSERT_EQ_INT(load_seq_count(input), 4,
                  "simple batch flow seq: 4 items");
    char buf[256];
    ASSERT(load_seq_item(input, 0, buf, sizeof(buf)), "item 0 exists");
    ASSERT_EQ_STR(buf, "a", "item 0 = a");
    ASSERT(load_seq_item(input, 3, buf, sizeof(buf)), "item 3 exists");
    ASSERT_EQ_STR(buf, "d", "item 3 = d");
}

static void test_batch_flow_seq_many(void) {
    /* Many flow items */
    char input[4096];
    int pos = 0;
    pos += snprintf(input + pos, sizeof(input) - pos, "[");
    for (int i = 0; i < 100; i++) {
        if (i > 0) pos += snprintf(input + pos, sizeof(input) - pos, ", ");
        pos += snprintf(input + pos, sizeof(input) - pos, "i%d", i);
    }
    pos += snprintf(input + pos, sizeof(input) - pos, "]");
    ASSERT_EQ_INT(load_seq_count(input), 100,
                  "many flow items: 100 items");
}

static void test_batch_flow_seq_fallback_nested(void) {
    /* Nested flow collection forces fallback */
    const char *input = "[a, [b, c], d]";
    ASSERT_EQ_INT(load_seq_count(input), 3,
                  "nested flow fallback: 3 items");
}

static void test_batch_flow_seq_fallback_mapping(void) {
    /* Flow mapping inside flow sequence */
    const char *input = "[a, {b: c}, d]";
    ASSERT_EQ_INT(load_seq_count(input), 3,
                  "mapping in flow seq: 3 items");
}

static void test_batch_flow_seq_empty(void) {
    /* Empty flow sequence */
    const char *input = "[]";
    ASSERT_EQ_INT(load_seq_count(input), 0,
                  "empty flow seq: 0 items");
}

static void test_batch_flow_seq_single(void) {
    /* Single item flow sequence */
    const char *input = "[single]";
    ASSERT_EQ_INT(load_seq_count(input), 1,
                  "single flow item: 1 item");
    char buf[256];
    ASSERT(load_seq_item(input, 0, buf, sizeof(buf)), "item 0 exists");
    ASSERT_EQ_STR(buf, "single", "item 0 = single");
}

static void test_batch_flow_seq_quoted(void) {
    /* Quoted strings in flow sequence */
    const char *input = "[\"hello world\", 'single quoted', plain]";
    ASSERT_EQ_INT(load_seq_count(input), 3,
                  "quoted in flow seq: 3 items");
    char buf[256];
    ASSERT(load_seq_item(input, 0, buf, sizeof(buf)), "item 0 exists");
    ASSERT_EQ_STR(buf, "hello world", "item 0 = hello world");
    ASSERT(load_seq_item(input, 1, buf, sizeof(buf)), "item 1 exists");
    ASSERT_EQ_STR(buf, "single quoted", "item 1 = single quoted");
}

static void test_batch_flow_seq_error_leading_comma(void) {
    /* Leading comma should be rejected (07faa3aa fix) */
    ASSERT(load_fails("[, a, b]"),
           "leading comma in flow seq: load fails");
}

static void test_batch_flow_seq_error_missing_separator(void) {
    /* Missing separator (no comma between items) should be rejected */
    ASSERT(load_fails("[word1 #xxx word2]"),
           "missing separator in flow seq: load fails");
}

/* ========== Mixed / Cross-Path Tests ========== */

static void test_batch_mapping_with_seq_values(void) {
    /* Mapping where some values are sequences */
    const char *input =
        "a: 1\n"
        "b:\n"
        "  - x\n"
        "  - y\n"
        "c: 3\n"
        "d:\n"
        "  - z\n"
        "e: 5\n";
    ASSERT_EQ_INT(load_mapping_count(input), 5,
                  "mapping with seq values: 5 pairs");
    ASSERT_EQ_INT((int)load_mapping_value_type(input, 1), YAML_SEQUENCE_NODE,
                  "b is sequence");
    ASSERT_EQ_INT((int)load_mapping_value_type(input, 3), YAML_SEQUENCE_NODE,
                  "d is sequence");
}

static void test_batch_seq_with_mapping_items(void) {
    /* Sequence where some items are mappings */
    const char *input =
        "- plain1\n"
        "- key1: val1\n"
        "  key2: val2\n"
        "- plain2\n";
    ASSERT_EQ_INT(load_seq_count(input), 3,
                  "seq with mapping items: 3 items");
}

static void test_batch_deeply_nested(void) {
    /* Deep nesting exercises multiple levels of batch/fallback */
    const char *input =
        "a:\n"
        "  b:\n"
        "    c: deep\n"
        "    d: value\n"
        "  e: mid\n"
        "f: top\n";
    ASSERT_EQ_INT(load_mapping_count(input), 2,
                  "deep nested: 2 top-level pairs");
    ASSERT(load_ok(input), "deep nested: loads ok");
}

static void test_batch_k8s_style(void) {
    /* Kubernetes-like structure: mapping with nested sequences and mappings */
    const char *input =
        "apiVersion: v1\n"
        "kind: Pod\n"
        "metadata:\n"
        "  name: test-pod\n"
        "  labels:\n"
        "    app: test\n"
        "    tier: frontend\n"
        "spec:\n"
        "  containers:\n"
        "    - name: web\n"
        "      image: nginx\n"
        "      ports:\n"
        "        - containerPort: 80\n";
    ASSERT(load_ok(input), "k8s style: loads ok");
    ASSERT_EQ_INT(load_mapping_count(input), 4,
                  "k8s style: 4 top-level keys");
    char buf[256];
    ASSERT(load_mapping_value(input, 0, buf, sizeof(buf)), "apiVersion exists");
    ASSERT_EQ_STR(buf, "v1", "apiVersion = v1");
    ASSERT(load_mapping_value(input, 1, buf, sizeof(buf)), "kind exists");
    ASSERT_EQ_STR(buf, "Pod", "kind = Pod");
}

static void test_batch_flow_in_block(void) {
    /* Flow collections inside block mapping */
    const char *input =
        "items: [a, b, c]\n"
        "config: {debug: true, verbose: false}\n"
        "name: simple\n";
    ASSERT(load_ok(input), "flow in block: loads ok");
    ASSERT_EQ_INT(load_mapping_count(input), 3,
                  "flow in block: 3 pairs");
}

/* ========== Error Path Tests ========== */

static void test_batch_mapping_error_duplicate_anchor(void) {
    /* Duplicate anchor in same document: libyaml rejects with error */
    const char *input = "a: &dup first\nb: &dup second\nc: *dup\n";
    ASSERT(load_fails(input), "duplicate anchor: load fails");
}

static void test_batch_mapping_error_undefined_alias(void) {
    /* Undefined alias should fail */
    ASSERT(load_fails("a: *undefined\n"),
           "undefined alias: load fails");
}

static void test_batch_multi_document(void) {
    /* Multi-document loading: each load call gets one document */
    const char *input = "a: 1\n---\nb: 2\n---\nc: 3\n";
    yaml_parser_t parser;
    yaml_document_t doc;
    int doc_count = 0;

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)input,
                                 strlen(input));

    while (yaml_parser_load(&parser, &doc)) {
        yaml_node_t *root = yaml_document_get_root_node(&doc);
        if (!root) { yaml_document_delete(&doc); break; }
        doc_count++;
        yaml_document_delete(&doc);
    }
    yaml_parser_delete(&parser);

    ASSERT_EQ_INT(doc_count, 3, "multi-document: 3 documents loaded");
}

static void test_batch_null_values(void) {
    /* Explicit null values */
    const char *input = "a: ~\nb: null\nc:\nd: Null\n";
    ASSERT_EQ_INT(load_mapping_count(input), 4,
                  "null values: 4 pairs");
    char buf[256];
    ASSERT(load_mapping_value(input, 0, buf, sizeof(buf)), "val 0 exists");
    ASSERT_EQ_STR(buf, "~", "val 0 = ~");
    ASSERT(load_mapping_value(input, 1, buf, sizeof(buf)), "val 1 exists");
    ASSERT_EQ_STR(buf, "null", "val 1 = null");
    ASSERT(load_mapping_value(input, 2, buf, sizeof(buf)), "val 2 exists");
    ASSERT_EQ_STR(buf, "", "val 2 is empty (implicit null)");
}

static void test_batch_special_scalars(void) {
    /* Scalars that might confuse the batch path */
    const char *input =
        "a: true\n"
        "b: false\n"
        "c: 123\n"
        "d: 3.14\n"
        "e: .inf\n"
        "f: .nan\n";
    ASSERT_EQ_INT(load_mapping_count(input), 6,
                  "special scalars: 6 pairs");
    char buf[256];
    ASSERT(load_mapping_value(input, 0, buf, sizeof(buf)), "true exists");
    ASSERT_EQ_STR(buf, "true", "a = true");
    ASSERT(load_mapping_value(input, 4, buf, sizeof(buf)), ".inf exists");
    ASSERT_EQ_STR(buf, ".inf", "e = .inf");
}

static void test_batch_unicode_scalars(void) {
    /* Unicode content in batch path */
    const char *input = "name: \xc3\xa9\xc3\xa0\xc3\xbc\nval: \xe4\xb8\xad\xe6\x96\x87\n";
    ASSERT_EQ_INT(load_mapping_count(input), 2,
                  "unicode scalars: 2 pairs");
    char buf[256];
    ASSERT(load_mapping_value(input, 0, buf, sizeof(buf)), "unicode val 0");
    ASSERT_EQ_STR(buf, "\xc3\xa9\xc3\xa0\xc3\xbc", "val 0 = eaue");
}

static void test_batch_long_scalars(void) {
    /* Long scalar values in batch path */
    char input[8192];
    char longval[4096];
    memset(longval, 'x', 2000);
    longval[2000] = '\0';
    snprintf(input, sizeof(input), "key: %s\n", longval);
    ASSERT(load_ok(input), "long scalar: loads ok");
    char buf[4096];
    ASSERT(load_mapping_value(input, 0, buf, sizeof(buf)), "long val exists");
    ASSERT_EQ_INT((int)strlen(buf), 2000, "long val length = 2000");
}

int main(void) {
    TEST_SUITE_BEGIN("Batch Loader");

    printf("  Block mapping batch:\n");
    test_batch_mapping_simple();
    test_batch_mapping_many_pairs();
    test_batch_mapping_empty_values();
    test_batch_mapping_fallback_nested_mapping();
    test_batch_mapping_fallback_nested_sequence();
    test_batch_mapping_fallback_flow();
    test_batch_mapping_fallback_complex_key();
    test_batch_mapping_fallback_anchor();
    test_batch_mapping_fallback_tag();
    test_batch_mapping_fallback_quoted();
    test_batch_mapping_fallback_block_scalar();
    test_batch_mapping_alternating();

    printf("  Block sequence batch:\n");
    test_batch_seq_simple();
    test_batch_seq_many_items();
    test_batch_seq_fallback_nested_seq();
    test_batch_seq_fallback_mapping();
    test_batch_seq_fallback_anchor();
    test_batch_seq_fallback_flow();
    test_batch_seq_empty_items();

    printf("  Flow sequence batch:\n");
    test_batch_flow_seq_simple();
    test_batch_flow_seq_many();
    test_batch_flow_seq_fallback_nested();
    test_batch_flow_seq_fallback_mapping();
    test_batch_flow_seq_empty();
    test_batch_flow_seq_single();
    test_batch_flow_seq_quoted();
    test_batch_flow_seq_error_leading_comma();
    test_batch_flow_seq_error_missing_separator();

    printf("  Mixed / cross-path:\n");
    test_batch_mapping_with_seq_values();
    test_batch_seq_with_mapping_items();
    test_batch_deeply_nested();
    test_batch_k8s_style();
    test_batch_flow_in_block();

    printf("  Error and edge cases:\n");
    test_batch_mapping_error_duplicate_anchor();
    test_batch_mapping_error_undefined_alias();
    test_batch_multi_document();
    test_batch_null_values();
    test_batch_special_scalars();
    test_batch_unicode_scalars();
    test_batch_long_scalars();

    TEST_SUITE_END();
}
