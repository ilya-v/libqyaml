#include "test_helper.h"

/*
 * Comprehensive loader (document API) tests covering yaml_parser_load,
 * node traversal, anchors/aliases in documents, tags, and edge cases.
 */

/* Helper: load a document and get root node type */
static yaml_node_type_t load_root_type(const char *input) {
    yaml_parser_t parser;
    yaml_document_t doc;
    yaml_node_type_t type = YAML_NO_NODE;

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));
    if (yaml_parser_load(&parser, &doc)) {
        yaml_node_t *root = yaml_document_get_root_node(&doc);
        if (root) type = root->type;
        yaml_document_delete(&doc);
    }
    yaml_parser_delete(&parser);
    return type;
}

/* Helper: load and get scalar value from root */
static int load_root_scalar(const char *input, const char *expected) {
    yaml_parser_t parser;
    yaml_document_t doc;
    int found = 0;

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));
    if (yaml_parser_load(&parser, &doc)) {
        yaml_node_t *root = yaml_document_get_root_node(&doc);
        if (root && root->type == YAML_SCALAR_NODE) {
            if (strcmp((const char *)root->data.scalar.value, expected) == 0)
                found = 1;
        }
        yaml_document_delete(&doc);
    }
    yaml_parser_delete(&parser);
    return found;
}

/* Helper: load and count sequence items */
static int load_seq_count(const char *input) {
    yaml_parser_t parser;
    yaml_document_t doc;
    int count = -1;

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));
    if (yaml_parser_load(&parser, &doc)) {
        yaml_node_t *root = yaml_document_get_root_node(&doc);
        if (root && root->type == YAML_SEQUENCE_NODE) {
            count = (int)(root->data.sequence.items.top - root->data.sequence.items.start);
        }
        yaml_document_delete(&doc);
    }
    yaml_parser_delete(&parser);
    return count;
}

/* Helper: load and count mapping pairs */
static int load_map_count(const char *input) {
    yaml_parser_t parser;
    yaml_document_t doc;
    int count = -1;

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));
    if (yaml_parser_load(&parser, &doc)) {
        yaml_node_t *root = yaml_document_get_root_node(&doc);
        if (root && root->type == YAML_MAPPING_NODE) {
            count = (int)(root->data.mapping.pairs.top - root->data.mapping.pairs.start);
        }
        yaml_document_delete(&doc);
    }
    yaml_parser_delete(&parser);
    return count;
}

/* ========== Root node types ========== */

static void test_l_scalar_root(void) {
    ASSERT_EQ_INT(load_root_type("hello"), YAML_SCALAR_NODE, "scalar root");
}

static void test_l_seq_root(void) {
    ASSERT_EQ_INT(load_root_type("- a\n- b\n"), YAML_SEQUENCE_NODE, "seq root");
}

static void test_l_map_root(void) {
    ASSERT_EQ_INT(load_root_type("a: b\n"), YAML_MAPPING_NODE, "map root");
}

static void test_l_empty_root(void) {
    ASSERT_EQ_INT(load_root_type(""), YAML_NO_NODE, "empty root");
}

static void test_l_comment_only_root(void) {
    ASSERT_EQ_INT(load_root_type("# comment\n"), YAML_NO_NODE, "comment root");
}

/* ========== Scalar values ========== */

static void test_l_plain_scalar(void) {
    ASSERT(load_root_scalar("hello", "hello"), "plain scalar");
}

static void test_l_single_quoted(void) {
    ASSERT(load_root_scalar("'hello'", "hello"), "single quoted");
}

static void test_l_double_quoted(void) {
    ASSERT(load_root_scalar("\"hello\"", "hello"), "double quoted");
}

static void test_l_number_scalar(void) {
    ASSERT(load_root_scalar("42", "42"), "number scalar");
}

static void test_l_float_scalar(void) {
    ASSERT(load_root_scalar("3.14", "3.14"), "float scalar");
}

static void test_l_bool_true(void) {
    ASSERT(load_root_scalar("true", "true"), "bool true");
}

static void test_l_bool_false(void) {
    ASSERT(load_root_scalar("false", "false"), "bool false");
}

static void test_l_null_scalar(void) {
    ASSERT(load_root_scalar("null", "null"), "null scalar");
}

static void test_l_tilde_scalar(void) {
    ASSERT(load_root_scalar("~", "~"), "tilde scalar");
}

static void test_l_empty_scalar(void) {
    ASSERT(load_root_scalar("''", ""), "empty scalar");
}

static void test_l_escape_newline(void) {
    ASSERT(load_root_scalar("\"a\\nb\"", "a\nb"), "escape newline");
}

static void test_l_escape_tab(void) {
    ASSERT(load_root_scalar("\"a\\tb\"", "a\tb"), "escape tab");
}

static void test_l_multiline_literal(void) {
    ASSERT(load_root_scalar("|\n  hello\n  world\n", "hello\nworld\n"), "literal");
}

static void test_l_multiline_folded(void) {
    ASSERT(load_root_scalar(">\n  hello\n  world\n", "hello world\n"), "folded");
}

/* ========== Sequences ========== */

static void test_l_seq_single(void) {
    ASSERT_EQ_INT(load_seq_count("- a\n"), 1, "seq 1 item");
}

static void test_l_seq_three(void) {
    ASSERT_EQ_INT(load_seq_count("- a\n- b\n- c\n"), 3, "seq 3 items");
}

static void test_l_seq_ten(void) {
    ASSERT_EQ_INT(load_seq_count(
        "- 0\n- 1\n- 2\n- 3\n- 4\n- 5\n- 6\n- 7\n- 8\n- 9\n"), 10, "seq 10 items");
}

static void test_l_flow_seq(void) {
    ASSERT_EQ_INT(load_seq_count("[a, b, c]"), 3, "flow seq 3");
}

static void test_l_flow_seq_empty(void) {
    ASSERT_EQ_INT(load_seq_count("[]"), 0, "flow seq empty");
}

static void test_l_nested_seq(void) {
    yaml_parser_t parser;
    yaml_document_t doc;
    yaml_parser_initialize(&parser);
    const char *input = "- - a\n  - b\n";
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));
    yaml_parser_load(&parser, &doc);
    yaml_node_t *root = yaml_document_get_root_node(&doc);
    ASSERT_NOT_NULL(root, "nested seq root");
    ASSERT_EQ_INT(root->type, YAML_SEQUENCE_NODE, "outer is seq");
    if (root && root->type == YAML_SEQUENCE_NODE) {
        int item_id = *root->data.sequence.items.start;
        yaml_node_t *inner = yaml_document_get_node(&doc, item_id);
        ASSERT_NOT_NULL(inner, "inner seq exists");
        ASSERT_EQ_INT(inner->type, YAML_SEQUENCE_NODE, "inner is seq");
    }
    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
}

/* ========== Mappings ========== */

static void test_l_map_single(void) {
    ASSERT_EQ_INT(load_map_count("a: b\n"), 1, "map 1 pair");
}

static void test_l_map_three(void) {
    ASSERT_EQ_INT(load_map_count("a: 1\nb: 2\nc: 3\n"), 3, "map 3 pairs");
}

static void test_l_flow_map(void) {
    ASSERT_EQ_INT(load_map_count("{a: 1, b: 2}"), 2, "flow map 2");
}

static void test_l_flow_map_empty(void) {
    ASSERT_EQ_INT(load_map_count("{}"), 0, "flow map empty");
}

static void test_l_nested_map(void) {
    yaml_parser_t parser;
    yaml_document_t doc;
    yaml_parser_initialize(&parser);
    const char *input = "a:\n  b: c\n";
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));
    yaml_parser_load(&parser, &doc);
    yaml_node_t *root = yaml_document_get_root_node(&doc);
    ASSERT_NOT_NULL(root, "nested map root");
    ASSERT_EQ_INT(root->type, YAML_MAPPING_NODE, "outer is map");
    if (root && root->type == YAML_MAPPING_NODE) {
        yaml_node_pair_t *pair = root->data.mapping.pairs.start;
        yaml_node_t *val = yaml_document_get_node(&doc, pair->value);
        ASSERT_NOT_NULL(val, "inner map exists");
        ASSERT_EQ_INT(val->type, YAML_MAPPING_NODE, "inner is map");
    }
    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
}

static void test_l_map_seq_value(void) {
    yaml_parser_t parser;
    yaml_document_t doc;
    yaml_parser_initialize(&parser);
    const char *input = "key:\n  - a\n  - b\n";
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));
    yaml_parser_load(&parser, &doc);
    yaml_node_t *root = yaml_document_get_root_node(&doc);
    ASSERT_NOT_NULL(root, "map seq val root");
    if (root && root->type == YAML_MAPPING_NODE) {
        yaml_node_pair_t *pair = root->data.mapping.pairs.start;
        yaml_node_t *val = yaml_document_get_node(&doc, pair->value);
        ASSERT_EQ_INT(val->type, YAML_SEQUENCE_NODE, "value is seq");
    }
    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
}

/* ========== Anchors and aliases in documents ========== */

static void test_l_anchor_alias_scalar(void) {
    yaml_parser_t parser;
    yaml_document_t doc;
    yaml_parser_initialize(&parser);
    const char *input = "- &ref hello\n- *ref\n";
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));
    yaml_parser_load(&parser, &doc);
    yaml_node_t *root = yaml_document_get_root_node(&doc);
    ASSERT_NOT_NULL(root, "anchor alias root");
    if (root && root->type == YAML_SEQUENCE_NODE) {
        int id1 = root->data.sequence.items.start[0];
        int id2 = root->data.sequence.items.start[1];
        yaml_node_t *n1 = yaml_document_get_node(&doc, id1);
        yaml_node_t *n2 = yaml_document_get_node(&doc, id2);
        ASSERT_EQ_STR(n1->data.scalar.value, "hello", "anchor value");
        /* Alias resolves to same node */
        ASSERT_EQ_INT(id1, id2, "alias same node id");
    }
    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
}

static void test_l_anchor_alias_mapping(void) {
    yaml_parser_t parser;
    yaml_document_t doc;
    yaml_parser_initialize(&parser);
    const char *input = "original: &m\n  a: 1\ncopy: *m\n";
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));
    yaml_parser_load(&parser, &doc);
    yaml_node_t *root = yaml_document_get_root_node(&doc);
    ASSERT_NOT_NULL(root, "anchor map root");
    ASSERT_EQ_INT(root->type, YAML_MAPPING_NODE, "root is map");
    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
}

/* ========== Tags in documents ========== */

static void test_l_scalar_tag(void) {
    yaml_parser_t parser;
    yaml_document_t doc;
    yaml_parser_initialize(&parser);
    const char *input = "!!str 42";
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));
    yaml_parser_load(&parser, &doc);
    yaml_node_t *root = yaml_document_get_root_node(&doc);
    ASSERT_NOT_NULL(root, "tagged scalar root");
    if (root) {
        ASSERT_EQ_STR(root->tag, "tag:yaml.org,2002:str", "str tag");
    }
    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
}

static void test_l_map_tag(void) {
    yaml_parser_t parser;
    yaml_document_t doc;
    yaml_parser_initialize(&parser);
    const char *input = "!!map\na: b\n";
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));
    yaml_parser_load(&parser, &doc);
    yaml_node_t *root = yaml_document_get_root_node(&doc);
    ASSERT_NOT_NULL(root, "tagged map root");
    if (root) {
        ASSERT_EQ_STR(root->tag, "tag:yaml.org,2002:map", "map tag");
    }
    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
}

static void test_l_seq_tag(void) {
    yaml_parser_t parser;
    yaml_document_t doc;
    yaml_parser_initialize(&parser);
    const char *input = "!!seq\n- a\n";
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));
    yaml_parser_load(&parser, &doc);
    yaml_node_t *root = yaml_document_get_root_node(&doc);
    ASSERT_NOT_NULL(root, "tagged seq root");
    if (root) {
        ASSERT_EQ_STR(root->tag, "tag:yaml.org,2002:seq", "seq tag");
    }
    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
}

/* ========== Multiple document loading ========== */

static void test_l_load_two_docs(void) {
    yaml_parser_t parser;
    yaml_document_t doc;
    int count = 0;
    yaml_parser_initialize(&parser);
    const char *input = "---\na\n---\nb\n";
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));
    while (yaml_parser_load(&parser, &doc)) {
        yaml_node_t *root = yaml_document_get_root_node(&doc);
        if (!root) { yaml_document_delete(&doc); break; }
        count++;
        yaml_document_delete(&doc);
    }
    yaml_parser_delete(&parser);
    ASSERT_EQ_INT(count, 2, "loaded 2 docs");
}

static void test_l_load_three_docs(void) {
    yaml_parser_t parser;
    yaml_document_t doc;
    int count = 0;
    yaml_parser_initialize(&parser);
    const char *input = "---\na\n---\nb\n---\nc\n";
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));
    while (yaml_parser_load(&parser, &doc)) {
        yaml_node_t *root = yaml_document_get_root_node(&doc);
        if (!root) { yaml_document_delete(&doc); break; }
        count++;
        yaml_document_delete(&doc);
    }
    yaml_parser_delete(&parser);
    ASSERT_EQ_INT(count, 3, "loaded 3 docs");
}

static void test_l_load_with_end_markers(void) {
    yaml_parser_t parser;
    yaml_document_t doc;
    int count = 0;
    yaml_parser_initialize(&parser);
    const char *input = "---\na\n...\n---\nb\n...\n";
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));
    while (yaml_parser_load(&parser, &doc)) {
        yaml_node_t *root = yaml_document_get_root_node(&doc);
        if (!root) { yaml_document_delete(&doc); break; }
        count++;
        yaml_document_delete(&doc);
    }
    yaml_parser_delete(&parser);
    ASSERT_EQ_INT(count, 2, "loaded 2 docs with end markers");
}

/* ========== Node marks ========== */

static void test_l_scalar_marks(void) {
    yaml_parser_t parser;
    yaml_document_t doc;
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)"hello", 5);
    yaml_parser_load(&parser, &doc);
    yaml_node_t *root = yaml_document_get_root_node(&doc);
    ASSERT_NOT_NULL(root, "marks: root");
    if (root) {
        ASSERT_EQ_INT((int)root->start_mark.line, 0, "start line 0");
        ASSERT_EQ_INT((int)root->start_mark.column, 0, "start col 0");
    }
    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
}

/* ========== Scalar styles in loaded documents ========== */

static void test_l_style_plain(void) {
    yaml_parser_t parser;
    yaml_document_t doc;
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)"hello", 5);
    yaml_parser_load(&parser, &doc);
    yaml_node_t *root = yaml_document_get_root_node(&doc);
    ASSERT_NOT_NULL(root, "style plain root");
    if (root) ASSERT_EQ_INT(root->data.scalar.style, YAML_PLAIN_SCALAR_STYLE, "plain style");
    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
}

static void test_l_style_single(void) {
    yaml_parser_t parser;
    yaml_document_t doc;
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)"'hello'", 7);
    yaml_parser_load(&parser, &doc);
    yaml_node_t *root = yaml_document_get_root_node(&doc);
    ASSERT_NOT_NULL(root, "style single root");
    if (root) ASSERT_EQ_INT(root->data.scalar.style, YAML_SINGLE_QUOTED_SCALAR_STYLE, "single style");
    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
}

static void test_l_style_double(void) {
    yaml_parser_t parser;
    yaml_document_t doc;
    yaml_parser_initialize(&parser);
    const char *input = "\"hello\"";
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));
    yaml_parser_load(&parser, &doc);
    yaml_node_t *root = yaml_document_get_root_node(&doc);
    ASSERT_NOT_NULL(root, "style double root");
    if (root) ASSERT_EQ_INT(root->data.scalar.style, YAML_DOUBLE_QUOTED_SCALAR_STYLE, "double style");
    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
}

static void test_l_style_literal(void) {
    yaml_parser_t parser;
    yaml_document_t doc;
    yaml_parser_initialize(&parser);
    const char *input = "|\n  hello\n";
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));
    yaml_parser_load(&parser, &doc);
    yaml_node_t *root = yaml_document_get_root_node(&doc);
    ASSERT_NOT_NULL(root, "style literal root");
    if (root) ASSERT_EQ_INT(root->data.scalar.style, YAML_LITERAL_SCALAR_STYLE, "literal style");
    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
}

static void test_l_style_folded(void) {
    yaml_parser_t parser;
    yaml_document_t doc;
    yaml_parser_initialize(&parser);
    const char *input = ">\n  hello\n";
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));
    yaml_parser_load(&parser, &doc);
    yaml_node_t *root = yaml_document_get_root_node(&doc);
    ASSERT_NOT_NULL(root, "style folded root");
    if (root) ASSERT_EQ_INT(root->data.scalar.style, YAML_FOLDED_SCALAR_STYLE, "folded style");
    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
}

/* ========== Collection styles ========== */

static void test_l_block_seq_style(void) {
    yaml_parser_t parser;
    yaml_document_t doc;
    yaml_parser_initialize(&parser);
    const char *input = "- a\n";
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));
    yaml_parser_load(&parser, &doc);
    yaml_node_t *root = yaml_document_get_root_node(&doc);
    ASSERT_NOT_NULL(root, "block seq style root");
    if (root) ASSERT_EQ_INT(root->data.sequence.style, YAML_BLOCK_SEQUENCE_STYLE, "block seq style");
    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
}

static void test_l_flow_seq_style(void) {
    yaml_parser_t parser;
    yaml_document_t doc;
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)"[a]", 3);
    yaml_parser_load(&parser, &doc);
    yaml_node_t *root = yaml_document_get_root_node(&doc);
    ASSERT_NOT_NULL(root, "flow seq style root");
    if (root) ASSERT_EQ_INT(root->data.sequence.style, YAML_FLOW_SEQUENCE_STYLE, "flow seq style");
    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
}

static void test_l_block_map_style(void) {
    yaml_parser_t parser;
    yaml_document_t doc;
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)"a: b\n", 5);
    yaml_parser_load(&parser, &doc);
    yaml_node_t *root = yaml_document_get_root_node(&doc);
    ASSERT_NOT_NULL(root, "block map style root");
    if (root) ASSERT_EQ_INT(root->data.mapping.style, YAML_BLOCK_MAPPING_STYLE, "block map style");
    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
}

static void test_l_flow_map_style(void) {
    yaml_parser_t parser;
    yaml_document_t doc;
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)"{a: b}", 6);
    yaml_parser_load(&parser, &doc);
    yaml_node_t *root = yaml_document_get_root_node(&doc);
    ASSERT_NOT_NULL(root, "flow map style root");
    if (root) ASSERT_EQ_INT(root->data.mapping.style, YAML_FLOW_MAPPING_STYLE, "flow map style");
    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
}

/* ========== Document construction and deletion ========== */

static void test_l_construct_scalar_doc(void) {
    yaml_document_t doc;
    yaml_document_initialize(&doc, NULL, NULL, NULL, 1, 1);
    int id = yaml_document_add_scalar(&doc, NULL, (yaml_char_t *)"test", 4, YAML_PLAIN_SCALAR_STYLE);
    ASSERT(id > 0, "add scalar id");
    yaml_node_t *root = yaml_document_get_root_node(&doc);
    ASSERT_NOT_NULL(root, "constructed root");
    yaml_document_delete(&doc);
}

static void test_l_construct_seq_doc(void) {
    yaml_document_t doc;
    yaml_document_initialize(&doc, NULL, NULL, NULL, 1, 1);
    int seq = yaml_document_add_sequence(&doc, NULL, YAML_BLOCK_SEQUENCE_STYLE);
    int s1 = yaml_document_add_scalar(&doc, NULL, (yaml_char_t *)"a", 1, YAML_PLAIN_SCALAR_STYLE);
    int s2 = yaml_document_add_scalar(&doc, NULL, (yaml_char_t *)"b", 1, YAML_PLAIN_SCALAR_STYLE);
    yaml_document_append_sequence_item(&doc, seq, s1);
    yaml_document_append_sequence_item(&doc, seq, s2);
    yaml_node_t *root = yaml_document_get_root_node(&doc);
    ASSERT_NOT_NULL(root, "seq root");
    ASSERT_EQ_INT(root->type, YAML_SEQUENCE_NODE, "is seq");
    int items = (int)(root->data.sequence.items.top - root->data.sequence.items.start);
    ASSERT_EQ_INT(items, 2, "2 items");
    yaml_document_delete(&doc);
}

static void test_l_construct_map_doc(void) {
    yaml_document_t doc;
    yaml_document_initialize(&doc, NULL, NULL, NULL, 1, 1);
    int map = yaml_document_add_mapping(&doc, NULL, YAML_BLOCK_MAPPING_STYLE);
    int k = yaml_document_add_scalar(&doc, NULL, (yaml_char_t *)"key", 3, YAML_PLAIN_SCALAR_STYLE);
    int v = yaml_document_add_scalar(&doc, NULL, (yaml_char_t *)"val", 3, YAML_PLAIN_SCALAR_STYLE);
    yaml_document_append_mapping_pair(&doc, map, k, v);
    yaml_node_t *root = yaml_document_get_root_node(&doc);
    ASSERT_NOT_NULL(root, "map root");
    ASSERT_EQ_INT(root->type, YAML_MAPPING_NODE, "is map");
    int pairs = (int)(root->data.mapping.pairs.top - root->data.mapping.pairs.start);
    ASSERT_EQ_INT(pairs, 1, "1 pair");
    yaml_document_delete(&doc);
}

/* ========== Large documents ========== */

static void test_l_large_seq(void) {
    char buf[8192];
    int pos = 0;
    for (int i = 0; i < 200 && pos < 8000; i++) {
        pos += snprintf(buf + pos, 8192 - pos, "- item%d\n", i);
    }
    ASSERT_EQ_INT(load_seq_count(buf), 200, "large seq 200 items");
}

static void test_l_large_map(void) {
    char buf[8192];
    int pos = 0;
    for (int i = 0; i < 100 && pos < 8000; i++) {
        pos += snprintf(buf + pos, 8192 - pos, "key%d: val%d\n", i, i);
    }
    ASSERT_EQ_INT(load_map_count(buf), 100, "large map 100 pairs");
}

/* ========== Real-world structures ========== */

static void test_l_kubernetes_pod(void) {
    const char *input =
        "apiVersion: v1\n"
        "kind: Pod\n"
        "metadata:\n"
        "  name: test-pod\n"
        "  labels:\n"
        "    app: test\n"
        "spec:\n"
        "  containers:\n"
        "    - name: nginx\n"
        "      image: nginx:1.21\n";
    yaml_parser_t parser;
    yaml_document_t doc;
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));
    ASSERT(yaml_parser_load(&parser, &doc), "k8s load");
    yaml_node_t *root = yaml_document_get_root_node(&doc);
    ASSERT_NOT_NULL(root, "k8s root");
    ASSERT_EQ_INT(root->type, YAML_MAPPING_NODE, "k8s is map");
    int pairs = (int)(root->data.mapping.pairs.top - root->data.mapping.pairs.start);
    ASSERT_EQ_INT(pairs, 4, "k8s 4 top-level keys");
    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
}

static void test_l_github_actions(void) {
    const char *input =
        "name: Build\n"
        "on:\n"
        "  push:\n"
        "    branches: [main]\n"
        "jobs:\n"
        "  test:\n"
        "    runs-on: ubuntu-latest\n"
        "    steps:\n"
        "      - uses: actions/checkout@v3\n"
        "      - run: make test\n";
    ASSERT_EQ_INT(load_root_type(input), YAML_MAPPING_NODE, "gha is map");
    ASSERT_EQ_INT(load_map_count(input), 3, "gha 3 top-level keys");
}

/* ========== Scalar length tracking ========== */

static void test_l_scalar_length_plain(void) {
    yaml_parser_t parser;
    yaml_document_t doc;
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)"hello", 5);
    yaml_parser_load(&parser, &doc);
    yaml_node_t *root = yaml_document_get_root_node(&doc);
    ASSERT_NOT_NULL(root, "len plain root");
    if (root) ASSERT_EQ_INT((int)root->data.scalar.length, 5, "plain len=5");
    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
}

static void test_l_scalar_length_empty(void) {
    yaml_parser_t parser;
    yaml_document_t doc;
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)"''", 2);
    yaml_parser_load(&parser, &doc);
    yaml_node_t *root = yaml_document_get_root_node(&doc);
    ASSERT_NOT_NULL(root, "len empty root");
    if (root) ASSERT_EQ_INT((int)root->data.scalar.length, 0, "empty len=0");
    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
}

static void test_l_scalar_length_escaped(void) {
    yaml_parser_t parser;
    yaml_document_t doc;
    yaml_parser_initialize(&parser);
    const char *input = "\"a\\nb\"";
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));
    yaml_parser_load(&parser, &doc);
    yaml_node_t *root = yaml_document_get_root_node(&doc);
    ASSERT_NOT_NULL(root, "len escaped root");
    if (root) ASSERT_EQ_INT((int)root->data.scalar.length, 3, "escaped len=3");
    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
}

/* ========== UTF-8 in documents ========== */

static void test_l_utf8_scalar(void) {
    ASSERT(load_root_scalar("\xc3\xa9", "\xc3\xa9"), "utf8 2-byte");
}

static void test_l_utf8_key(void) {
    yaml_parser_t parser;
    yaml_document_t doc;
    yaml_parser_initialize(&parser);
    const char *input = "\xc3\xa9: val\n";
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));
    yaml_parser_load(&parser, &doc);
    yaml_node_t *root = yaml_document_get_root_node(&doc);
    ASSERT_NOT_NULL(root, "utf8 key root");
    ASSERT_EQ_INT(root->type, YAML_MAPPING_NODE, "utf8 key map");
    if (root && root->type == YAML_MAPPING_NODE) {
        yaml_node_t *key = yaml_document_get_node(&doc, root->data.mapping.pairs.start->key);
        ASSERT_EQ_STR(key->data.scalar.value, "\xc3\xa9", "utf8 key val");
    }
    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
}

static void test_l_utf8_3byte(void) {
    ASSERT(load_root_scalar("\xe2\x82\xac", "\xe2\x82\xac"), "utf8 euro");
}

static void test_l_utf8_4byte(void) {
    ASSERT(load_root_scalar("\xf0\x9f\x98\x80", "\xf0\x9f\x98\x80"), "utf8 emoji");
}

/* ========== Indentation edge cases ========== */

static void test_l_deep_indent(void) {
    const char *input = "a:\n  b:\n    c:\n      d:\n        e: f\n";
    ASSERT_EQ_INT(load_root_type(input), YAML_MAPPING_NODE, "deep indent");
}

static void test_l_mixed_indent(void) {
    const char *input = "a:\n  b: 1\nc:\n  d: 2\n";
    ASSERT_EQ_INT(load_map_count(input), 2, "mixed indent pairs");
}

/* ========== Flow in block ========== */

static void test_l_flow_seq_in_block(void) {
    const char *input = "key: [a, b, c]\n";
    yaml_parser_t parser;
    yaml_document_t doc;
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));
    yaml_parser_load(&parser, &doc);
    yaml_node_t *root = yaml_document_get_root_node(&doc);
    ASSERT_NOT_NULL(root, "flow in block root");
    if (root && root->type == YAML_MAPPING_NODE) {
        yaml_node_t *val = yaml_document_get_node(&doc, root->data.mapping.pairs.start->value);
        ASSERT_EQ_INT(val->type, YAML_SEQUENCE_NODE, "val is flow seq");
    }
    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
}

static void test_l_flow_map_in_block(void) {
    const char *input = "key: {a: 1}\n";
    yaml_parser_t parser;
    yaml_document_t doc;
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));
    yaml_parser_load(&parser, &doc);
    yaml_node_t *root = yaml_document_get_root_node(&doc);
    ASSERT_NOT_NULL(root, "flow map in block root");
    if (root && root->type == YAML_MAPPING_NODE) {
        yaml_node_t *val = yaml_document_get_node(&doc, root->data.mapping.pairs.start->value);
        ASSERT_EQ_INT(val->type, YAML_MAPPING_NODE, "val is flow map");
    }
    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
}

/* ========== Explicit key ========== */

static void test_l_explicit_key(void) {
    const char *input = "? key\n: value\n";
    ASSERT_EQ_INT(load_map_count(input), 1, "explicit key pair");
}

static void test_l_explicit_key_complex(void) {
    const char *input = "? [a, b]\n: value\n";
    ASSERT_EQ_INT(load_root_type(input), YAML_MAPPING_NODE, "complex key");
}

/* ========== Empty values ========== */

static void test_l_empty_map_value(void) {
    const char *input = "key:\n";
    yaml_parser_t parser;
    yaml_document_t doc;
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));
    yaml_parser_load(&parser, &doc);
    yaml_node_t *root = yaml_document_get_root_node(&doc);
    ASSERT_NOT_NULL(root, "empty val root");
    if (root && root->type == YAML_MAPPING_NODE) {
        yaml_node_t *val = yaml_document_get_node(&doc, root->data.mapping.pairs.start->value);
        ASSERT_NOT_NULL(val, "empty val node");
        ASSERT_EQ_INT(val->type, YAML_SCALAR_NODE, "empty val is scalar");
        ASSERT_EQ_INT((int)val->data.scalar.length, 0, "empty val length 0");
    }
    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
}

static void test_l_empty_seq_item(void) {
    const char *input = "- \n- a\n";
    ASSERT_EQ_INT(load_seq_count(input), 2, "empty item: 2 items");
}

/* ========== Special scalar content ========== */

static void test_l_scalar_with_colon(void) {
    ASSERT(load_root_scalar("\"a: b\"", "a: b"), "scalar with colon");
}

static void test_l_scalar_with_hash(void) {
    ASSERT(load_root_scalar("\"a # b\"", "a # b"), "scalar with hash");
}

static void test_l_scalar_with_bracket(void) {
    ASSERT(load_root_scalar("\"[a]\"", "[a]"), "scalar with bracket");
}

static void test_l_scalar_with_brace(void) {
    ASSERT(load_root_scalar("\"{a}\"", "{a}"), "scalar with brace");
}

static void test_l_scalar_multiword(void) {
    ASSERT(load_root_scalar("hello world", "hello world"), "multiword scalar");
}

static void test_l_scalar_with_dash(void) {
    ASSERT(load_root_scalar("hello-world", "hello-world"), "dash scalar");
}

static void test_l_scalar_with_underscore(void) {
    ASSERT(load_root_scalar("hello_world", "hello_world"), "underscore scalar");
}

static void test_l_scalar_with_dot(void) {
    ASSERT(load_root_scalar("hello.world", "hello.world"), "dot scalar");
}

static void test_l_scalar_with_slash(void) {
    ASSERT(load_root_scalar("path/to/file", "path/to/file"), "slash scalar");
}

static void test_l_scalar_url(void) {
    ASSERT(load_root_scalar("\"https://example.com\"", "https://example.com"), "url scalar");
}

/* ========== Block scalar chomping ========== */

static void test_l_literal_clip(void) {
    ASSERT(load_root_scalar("|\n  hello\n", "hello\n"), "literal clip");
}

static void test_l_literal_strip(void) {
    ASSERT(load_root_scalar("|-\n  hello\n", "hello"), "literal strip");
}

static void test_l_literal_keep(void) {
    ASSERT(load_root_scalar("|+\n  hello\n\n", "hello\n\n"), "literal keep");
}

static void test_l_folded_clip(void) {
    ASSERT(load_root_scalar(">\n  hello\n", "hello\n"), "folded clip");
}

static void test_l_folded_strip(void) {
    ASSERT(load_root_scalar(">-\n  hello\n", "hello"), "folded strip");
}

static void test_l_folded_keep(void) {
    ASSERT(load_root_scalar(">+\n  hello\n\n", "hello\n\n"), "folded keep");
}

int main(void) {
    TEST_SUITE_BEGIN("Loader Comprehensive");

    /* Root node types */
    test_l_scalar_root();
    test_l_seq_root();
    test_l_map_root();
    test_l_empty_root();
    test_l_comment_only_root();

    /* Scalar values */
    test_l_plain_scalar();
    test_l_single_quoted();
    test_l_double_quoted();
    test_l_number_scalar();
    test_l_float_scalar();
    test_l_bool_true();
    test_l_bool_false();
    test_l_null_scalar();
    test_l_tilde_scalar();
    test_l_empty_scalar();
    test_l_escape_newline();
    test_l_escape_tab();
    test_l_multiline_literal();
    test_l_multiline_folded();

    /* Sequences */
    test_l_seq_single();
    test_l_seq_three();
    test_l_seq_ten();
    test_l_flow_seq();
    test_l_flow_seq_empty();
    test_l_nested_seq();

    /* Mappings */
    test_l_map_single();
    test_l_map_three();
    test_l_flow_map();
    test_l_flow_map_empty();
    test_l_nested_map();
    test_l_map_seq_value();

    /* Anchors and aliases */
    test_l_anchor_alias_scalar();
    test_l_anchor_alias_mapping();

    /* Tags */
    test_l_scalar_tag();
    test_l_map_tag();
    test_l_seq_tag();

    /* Multiple documents */
    test_l_load_two_docs();
    test_l_load_three_docs();
    test_l_load_with_end_markers();

    /* Node marks */
    test_l_scalar_marks();

    /* Scalar styles */
    test_l_style_plain();
    test_l_style_single();
    test_l_style_double();
    test_l_style_literal();
    test_l_style_folded();

    /* Collection styles */
    test_l_block_seq_style();
    test_l_flow_seq_style();
    test_l_block_map_style();
    test_l_flow_map_style();

    /* Document construction */
    test_l_construct_scalar_doc();
    test_l_construct_seq_doc();
    test_l_construct_map_doc();

    /* Large documents */
    test_l_large_seq();
    test_l_large_map();

    /* Real-world structures */
    test_l_kubernetes_pod();
    test_l_github_actions();

    /* Scalar length tracking */
    test_l_scalar_length_plain();
    test_l_scalar_length_empty();
    test_l_scalar_length_escaped();

    /* UTF-8 in documents */
    test_l_utf8_scalar();
    test_l_utf8_key();
    test_l_utf8_3byte();
    test_l_utf8_4byte();

    /* Indentation edge cases */
    test_l_deep_indent();
    test_l_mixed_indent();

    /* Flow in block */
    test_l_flow_seq_in_block();
    test_l_flow_map_in_block();

    /* Explicit key */
    test_l_explicit_key();
    test_l_explicit_key_complex();

    /* Empty values */
    test_l_empty_map_value();
    test_l_empty_seq_item();

    /* Special scalar content */
    test_l_scalar_with_colon();
    test_l_scalar_with_hash();
    test_l_scalar_with_bracket();
    test_l_scalar_with_brace();
    test_l_scalar_multiword();
    test_l_scalar_with_dash();
    test_l_scalar_with_underscore();
    test_l_scalar_with_dot();
    test_l_scalar_with_slash();
    test_l_scalar_url();

    /* Block scalar chomping */
    test_l_literal_clip();
    test_l_literal_strip();
    test_l_literal_keep();
    test_l_folded_clip();
    test_l_folded_strip();
    test_l_folded_keep();

    TEST_SUITE_END();
}
