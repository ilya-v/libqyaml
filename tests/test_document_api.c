#include "test_helper.h"

/* Test document initialization */
static void test_document_init(void) {
    yaml_document_t doc;

    ASSERT(yaml_document_initialize(&doc, NULL, NULL, NULL, 1, 1), "doc init");
    ASSERT(doc.start_implicit, "doc: start implicit");
    ASSERT(doc.end_implicit, "doc: end implicit");
    yaml_document_delete(&doc);

    /* With version directive */
    yaml_version_directive_t version = {1, 1};
    ASSERT(yaml_document_initialize(&doc, &version, NULL, NULL, 0, 0), "doc init version");
    ASSERT_NOT_NULL(doc.version_directive, "doc: has version");
    ASSERT_EQ_INT(doc.version_directive->major, 1, "doc: version major");
    ASSERT_EQ_INT(doc.version_directive->minor, 1, "doc: version minor");
    yaml_document_delete(&doc);

    /* With tag directives */
    yaml_tag_directive_t tags[1] = {
        {(yaml_char_t *)"!", (yaml_char_t *)"!"}
    };
    ASSERT(yaml_document_initialize(&doc, NULL, tags, tags + 1, 1, 1),
           "doc init tags");
    yaml_document_delete(&doc);
}

/* Test adding scalar nodes */
static void test_add_scalar(void) {
    yaml_document_t doc;

    ASSERT(yaml_document_initialize(&doc, NULL, NULL, NULL, 1, 1), "doc init scalar");

    int id = yaml_document_add_scalar(&doc, (const yaml_char_t *)YAML_STR_TAG,
             (const yaml_char_t *)"hello", 5, YAML_PLAIN_SCALAR_STYLE);
    ASSERT(id > 0, "scalar: node id > 0");

    yaml_node_t *node = yaml_document_get_node(&doc, id);
    ASSERT_NOT_NULL(node, "scalar: node exists");
    ASSERT_EQ_INT(node->type, YAML_SCALAR_NODE, "scalar: node type");
    ASSERT_EQ_STR((const char *)node->data.scalar.value, "hello", "scalar: value");
    ASSERT_EQ_INT((int)node->data.scalar.length, 5, "scalar: length");

    /* Root node should be the first added */
    yaml_node_t *root = yaml_document_get_root_node(&doc);
    ASSERT_NOT_NULL(root, "scalar: root exists");
    ASSERT(root == node, "scalar: root is first node");

    yaml_document_delete(&doc);
}

/* Test adding sequence nodes */
static void test_add_sequence(void) {
    yaml_document_t doc;

    ASSERT(yaml_document_initialize(&doc, NULL, NULL, NULL, 1, 1), "doc init seq");

    int seq_id = yaml_document_add_sequence(&doc,
                 (const yaml_char_t *)YAML_SEQ_TAG, YAML_BLOCK_SEQUENCE_STYLE);
    ASSERT(seq_id > 0, "seq: id > 0");

    int s1 = yaml_document_add_scalar(&doc, (const yaml_char_t *)YAML_STR_TAG,
              (const yaml_char_t *)"item1", 5, YAML_PLAIN_SCALAR_STYLE);
    int s2 = yaml_document_add_scalar(&doc, (const yaml_char_t *)YAML_STR_TAG,
              (const yaml_char_t *)"item2", 5, YAML_PLAIN_SCALAR_STYLE);

    ASSERT(yaml_document_append_sequence_item(&doc, seq_id, s1), "seq: append 1");
    ASSERT(yaml_document_append_sequence_item(&doc, seq_id, s2), "seq: append 2");

    yaml_node_t *seq_node = yaml_document_get_node(&doc, seq_id);
    ASSERT_NOT_NULL(seq_node, "seq: node exists");
    ASSERT_EQ_INT(seq_node->type, YAML_SEQUENCE_NODE, "seq: node type");

    /* Check items count */
    int item_count = (int)(seq_node->data.sequence.items.top -
                           seq_node->data.sequence.items.start);
    ASSERT_EQ_INT(item_count, 2, "seq: 2 items");

    yaml_document_delete(&doc);
}

/* Test adding mapping nodes */
static void test_add_mapping(void) {
    yaml_document_t doc;

    ASSERT(yaml_document_initialize(&doc, NULL, NULL, NULL, 1, 1), "doc init map");

    int map_id = yaml_document_add_mapping(&doc,
                 (const yaml_char_t *)YAML_MAP_TAG, YAML_BLOCK_MAPPING_STYLE);
    ASSERT(map_id > 0, "map: id > 0");

    int k1 = yaml_document_add_scalar(&doc, (const yaml_char_t *)YAML_STR_TAG,
              (const yaml_char_t *)"key1", 4, YAML_PLAIN_SCALAR_STYLE);
    int v1 = yaml_document_add_scalar(&doc, (const yaml_char_t *)YAML_STR_TAG,
              (const yaml_char_t *)"val1", 4, YAML_PLAIN_SCALAR_STYLE);

    ASSERT(yaml_document_append_mapping_pair(&doc, map_id, k1, v1), "map: append pair");

    yaml_node_t *map_node = yaml_document_get_node(&doc, map_id);
    ASSERT_NOT_NULL(map_node, "map: node exists");
    ASSERT_EQ_INT(map_node->type, YAML_MAPPING_NODE, "map: node type");

    int pair_count = (int)(map_node->data.mapping.pairs.top -
                           map_node->data.mapping.pairs.start);
    ASSERT_EQ_INT(pair_count, 1, "map: 1 pair");

    yaml_document_delete(&doc);
}

/* Test loading a document from parser */
static void test_load_document(void) {
    yaml_parser_t parser;
    yaml_document_t doc;

    const char *yaml = "key: value\n";

    ASSERT(yaml_parser_initialize(&parser), "load init");
    yaml_parser_set_input_string(&parser, (const unsigned char *)yaml, strlen(yaml));

    ASSERT(yaml_parser_load(&parser, &doc), "load doc");

    yaml_node_t *root = yaml_document_get_root_node(&doc);
    ASSERT_NOT_NULL(root, "load: root exists");
    ASSERT_EQ_INT(root->type, YAML_MAPPING_NODE, "load: root is mapping");

    /* Check the mapping has one pair */
    int pair_count = (int)(root->data.mapping.pairs.top -
                           root->data.mapping.pairs.start);
    ASSERT_EQ_INT(pair_count, 1, "load: 1 pair");

    /* Get key and value */
    yaml_node_pair_t *pair = root->data.mapping.pairs.start;
    yaml_node_t *key_node = yaml_document_get_node(&doc, pair->key);
    yaml_node_t *val_node = yaml_document_get_node(&doc, pair->value);

    ASSERT_NOT_NULL(key_node, "load: key exists");
    ASSERT_NOT_NULL(val_node, "load: val exists");
    ASSERT_EQ_STR((const char *)key_node->data.scalar.value, "key", "load: key value");
    ASSERT_EQ_STR((const char *)val_node->data.scalar.value, "value", "load: val value");

    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
}

/* Test loading sequence document */
static void test_load_sequence(void) {
    yaml_parser_t parser;
    yaml_document_t doc;

    const char *yaml = "- a\n- b\n- c\n";

    ASSERT(yaml_parser_initialize(&parser), "load seq init");
    yaml_parser_set_input_string(&parser, (const unsigned char *)yaml, strlen(yaml));

    ASSERT(yaml_parser_load(&parser, &doc), "load seq doc");

    yaml_node_t *root = yaml_document_get_root_node(&doc);
    ASSERT_NOT_NULL(root, "load seq: root");
    ASSERT_EQ_INT(root->type, YAML_SEQUENCE_NODE, "load seq: is sequence");

    int item_count = (int)(root->data.sequence.items.top -
                           root->data.sequence.items.start);
    ASSERT_EQ_INT(item_count, 3, "load seq: 3 items");

    /* Verify items */
    for (int i = 0; i < 3; i++) {
        int item_id = root->data.sequence.items.start[i];
        yaml_node_t *item = yaml_document_get_node(&doc, item_id);
        ASSERT_NOT_NULL(item, "load seq: item exists");
        ASSERT_EQ_INT(item->type, YAML_SCALAR_NODE, "load seq: item is scalar");
    }

    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
}

/* Test loading multiple documents */
static void test_load_multiple_documents(void) {
    yaml_parser_t parser;
    yaml_document_t doc;

    const char *yaml = "---\ndoc1\n---\ndoc2\n...\n";

    ASSERT(yaml_parser_initialize(&parser), "load multi init");
    yaml_parser_set_input_string(&parser, (const unsigned char *)yaml, strlen(yaml));

    /* First document */
    ASSERT(yaml_parser_load(&parser, &doc), "load multi doc1");
    yaml_node_t *root = yaml_document_get_root_node(&doc);
    ASSERT_NOT_NULL(root, "multi: doc1 root");
    ASSERT_EQ_STR((const char *)root->data.scalar.value, "doc1", "multi: doc1 value");
    yaml_document_delete(&doc);

    /* Second document */
    ASSERT(yaml_parser_load(&parser, &doc), "load multi doc2");
    root = yaml_document_get_root_node(&doc);
    ASSERT_NOT_NULL(root, "multi: doc2 root");
    ASSERT_EQ_STR((const char *)root->data.scalar.value, "doc2", "multi: doc2 value");
    yaml_document_delete(&doc);

    /* End of stream (empty document) */
    ASSERT(yaml_parser_load(&parser, &doc), "load multi end");
    root = yaml_document_get_root_node(&doc);
    ASSERT_NULL(root, "multi: end is empty");
    yaml_document_delete(&doc);

    yaml_parser_delete(&parser);
}

/* Test loading anchors and aliases */
static void test_load_anchors(void) {
    yaml_parser_t parser;
    yaml_document_t doc;

    const char *yaml = "- &anchor hello\n- *anchor\n";

    ASSERT(yaml_parser_initialize(&parser), "load anchor init");
    yaml_parser_set_input_string(&parser, (const unsigned char *)yaml, strlen(yaml));

    ASSERT(yaml_parser_load(&parser, &doc), "load anchor doc");

    yaml_node_t *root = yaml_document_get_root_node(&doc);
    ASSERT_NOT_NULL(root, "anchor: root");
    ASSERT_EQ_INT(root->type, YAML_SEQUENCE_NODE, "anchor: is sequence");

    int item_count = (int)(root->data.sequence.items.top -
                           root->data.sequence.items.start);
    ASSERT_EQ_INT(item_count, 2, "anchor: 2 items");

    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
}

/* Test loading nested mapping */
static void test_load_nested(void) {
    yaml_parser_t parser;
    yaml_document_t doc;

    const char *yaml = "outer:\n  inner: value\n";

    ASSERT(yaml_parser_initialize(&parser), "load nested init");
    yaml_parser_set_input_string(&parser, (const unsigned char *)yaml, strlen(yaml));

    ASSERT(yaml_parser_load(&parser, &doc), "load nested doc");

    yaml_node_t *root = yaml_document_get_root_node(&doc);
    ASSERT_NOT_NULL(root, "nested: root");
    ASSERT_EQ_INT(root->type, YAML_MAPPING_NODE, "nested: root is mapping");

    yaml_node_pair_t *pair = root->data.mapping.pairs.start;
    yaml_node_t *key = yaml_document_get_node(&doc, pair->key);
    yaml_node_t *val = yaml_document_get_node(&doc, pair->value);

    ASSERT_EQ_STR((const char *)key->data.scalar.value, "outer", "nested: key");
    ASSERT_EQ_INT(val->type, YAML_MAPPING_NODE, "nested: value is mapping");

    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
}

/* Test get_node with invalid indices */
static void test_get_node_bounds(void) {
    yaml_document_t doc;
    ASSERT(yaml_document_initialize(&doc, NULL, NULL, NULL, 1, 1), "bounds init");

    yaml_document_add_scalar(&doc, (const yaml_char_t *)YAML_STR_TAG,
                             (const yaml_char_t *)"test", 4, YAML_PLAIN_SCALAR_STYLE);

    /* Valid index */
    ASSERT_NOT_NULL(yaml_document_get_node(&doc, 1), "bounds: index 1 valid");

    /* Invalid indices */
    ASSERT_NULL(yaml_document_get_node(&doc, 0), "bounds: index 0 null");
    ASSERT_NULL(yaml_document_get_node(&doc, 2), "bounds: index 2 null");
    ASSERT_NULL(yaml_document_get_node(&doc, -1), "bounds: index -1 null");

    yaml_document_delete(&doc);
}

/* Test empty document */
static void test_empty_document(void) {
    yaml_document_t doc;
    ASSERT(yaml_document_initialize(&doc, NULL, NULL, NULL, 1, 1), "empty doc init");

    yaml_node_t *root = yaml_document_get_root_node(&doc);
    ASSERT_NULL(root, "empty doc: no root");

    yaml_document_delete(&doc);
}

/* ==================================================================
 * Loader coverage: duplicate anchor (composer error context)
 * Covers: yaml_parser_register_anchor duplicate check,
 *         yaml_parser_set_composer_error_context
 * ================================================================== */

static void test_load_duplicate_anchor(void) {
    yaml_parser_t parser;
    yaml_document_t doc;
    const char *yaml = "- &dup hello\n- &dup world\n";

    ASSERT(yaml_parser_initialize(&parser), "dup anchor init");
    yaml_parser_set_input_string(&parser, (const unsigned char *)yaml, strlen(yaml));

    int result = yaml_parser_load(&parser, &doc);
    ASSERT(!result, "dup anchor: load should fail");
    ASSERT_EQ_INT(parser.error, YAML_COMPOSER_ERROR, "dup anchor: composer error");
    ASSERT_NOT_NULL(parser.problem, "dup anchor: has problem");

    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
}

/* ==================================================================
 * Loader coverage: undefined alias (composer error)
 * Covers: yaml_parser_load_alias undefined path,
 *         yaml_parser_set_composer_error
 * ================================================================== */

static void test_load_undefined_alias(void) {
    yaml_parser_t parser;
    yaml_document_t doc;
    const char *yaml = "- *noexist\n";

    ASSERT(yaml_parser_initialize(&parser), "undef alias init");
    yaml_parser_set_input_string(&parser, (const unsigned char *)yaml, strlen(yaml));

    int result = yaml_parser_load(&parser, &doc);
    ASSERT(!result, "undef alias: load should fail");
    ASSERT_EQ_INT(parser.error, YAML_COMPOSER_ERROR, "undef alias: composer error");

    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
}

/* ==================================================================
 * Loader coverage: anchored sequence and mapping
 * Covers: yaml_parser_load_sequence anchor path,
 *         yaml_parser_load_mapping anchor path
 * ================================================================== */

static void test_load_anchored_sequence(void) {
    yaml_parser_t parser;
    yaml_document_t doc;
    const char *yaml = "&myseq\n- a\n- b\n";

    ASSERT(yaml_parser_initialize(&parser), "anchored seq init");
    yaml_parser_set_input_string(&parser, (const unsigned char *)yaml, strlen(yaml));
    ASSERT(yaml_parser_load(&parser, &doc), "anchored seq load");

    yaml_node_t *root = yaml_document_get_root_node(&doc);
    ASSERT_NOT_NULL(root, "anchored seq: root");
    ASSERT_EQ_INT(root->type, YAML_SEQUENCE_NODE, "anchored seq: is seq");
    ASSERT_NOT_NULL(root->tag, "anchored seq: has tag");

    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
}

static void test_load_anchored_mapping(void) {
    yaml_parser_t parser;
    yaml_document_t doc;
    const char *yaml = "&mymap\na: b\nc: d\n";

    ASSERT(yaml_parser_initialize(&parser), "anchored map init");
    yaml_parser_set_input_string(&parser, (const unsigned char *)yaml, strlen(yaml));
    ASSERT(yaml_parser_load(&parser, &doc), "anchored map load");

    yaml_node_t *root = yaml_document_get_root_node(&doc);
    ASSERT_NOT_NULL(root, "anchored map: root");
    ASSERT_EQ_INT(root->type, YAML_MAPPING_NODE, "anchored map: is map");

    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
}

static void test_load_anchored_scalar(void) {
    yaml_parser_t parser;
    yaml_document_t doc;
    const char *yaml = "&myscalar hello\n";

    ASSERT(yaml_parser_initialize(&parser), "anchored scalar init");
    yaml_parser_set_input_string(&parser, (const unsigned char *)yaml, strlen(yaml));
    ASSERT(yaml_parser_load(&parser, &doc), "anchored scalar load");

    yaml_node_t *root = yaml_document_get_root_node(&doc);
    ASSERT_NOT_NULL(root, "anchored scalar: root");
    ASSERT_EQ_INT(root->type, YAML_SCALAR_NODE, "anchored scalar: is scalar");
    ASSERT_EQ_STR((const char *)root->data.scalar.value, "hello", "anchored scalar: value");

    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
}

/* ==================================================================
 * Loader coverage: tag handling (! tag replacement)
 * Covers: yaml_parser_load_scalar tag=! path,
 *         yaml_parser_load_sequence tag=! path,
 *         yaml_parser_load_mapping tag=! path
 * ================================================================== */

static void test_load_tagged_scalar(void) {
    yaml_parser_t parser;
    yaml_document_t doc;
    const char *yaml = "!!str hello\n";

    ASSERT(yaml_parser_initialize(&parser), "tagged scalar init");
    yaml_parser_set_input_string(&parser, (const unsigned char *)yaml, strlen(yaml));
    ASSERT(yaml_parser_load(&parser, &doc), "tagged scalar load");

    yaml_node_t *root = yaml_document_get_root_node(&doc);
    ASSERT_NOT_NULL(root, "tagged scalar: root");
    ASSERT_EQ_INT(root->type, YAML_SCALAR_NODE, "tagged scalar: type");
    ASSERT_NOT_NULL(root->tag, "tagged scalar: has tag");

    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
}

static void test_load_tagged_sequence(void) {
    yaml_parser_t parser;
    yaml_document_t doc;
    const char *yaml = "!!seq\n- a\n- b\n";

    ASSERT(yaml_parser_initialize(&parser), "tagged seq init");
    yaml_parser_set_input_string(&parser, (const unsigned char *)yaml, strlen(yaml));
    ASSERT(yaml_parser_load(&parser, &doc), "tagged seq load");

    yaml_node_t *root = yaml_document_get_root_node(&doc);
    ASSERT_NOT_NULL(root, "tagged seq: root");
    ASSERT_EQ_INT(root->type, YAML_SEQUENCE_NODE, "tagged seq: type");

    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
}

static void test_load_tagged_mapping(void) {
    yaml_parser_t parser;
    yaml_document_t doc;
    const char *yaml = "!!map\na: b\n";

    ASSERT(yaml_parser_initialize(&parser), "tagged map init");
    yaml_parser_set_input_string(&parser, (const unsigned char *)yaml, strlen(yaml));
    ASSERT(yaml_parser_load(&parser, &doc), "tagged map load");

    yaml_node_t *root = yaml_document_get_root_node(&doc);
    ASSERT_NOT_NULL(root, "tagged map: root");
    ASSERT_EQ_INT(root->type, YAML_MAPPING_NODE, "tagged map: type");

    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
}

/* ==================================================================
 * Loader coverage: alias as mapping value
 * Covers: yaml_parser_load_alias found path,
 *         yaml_parser_load_node_add mapping value path
 * ================================================================== */

static void test_load_alias_in_mapping(void) {
    yaml_parser_t parser;
    yaml_document_t doc;
    const char *yaml = "key1: &val hello\nkey2: *val\n";

    ASSERT(yaml_parser_initialize(&parser), "alias map init");
    yaml_parser_set_input_string(&parser, (const unsigned char *)yaml, strlen(yaml));
    ASSERT(yaml_parser_load(&parser, &doc), "alias map load");

    yaml_node_t *root = yaml_document_get_root_node(&doc);
    ASSERT_NOT_NULL(root, "alias map: root");
    ASSERT_EQ_INT(root->type, YAML_MAPPING_NODE, "alias map: is mapping");

    int pair_count = (int)(root->data.mapping.pairs.top -
                           root->data.mapping.pairs.start);
    ASSERT_EQ_INT(pair_count, 2, "alias map: 2 pairs");

    /* Both values should reference the same node or equivalent */
    yaml_node_pair_t *p1 = root->data.mapping.pairs.start;
    yaml_node_pair_t *p2 = p1 + 1;
    yaml_node_t *v1 = yaml_document_get_node(&doc, p1->value);
    yaml_node_t *v2 = yaml_document_get_node(&doc, p2->value);
    ASSERT_NOT_NULL(v1, "alias map: val1");
    ASSERT_NOT_NULL(v2, "alias map: val2");
    /* Alias points to same node index */
    ASSERT_EQ_INT(p1->value, p2->value, "alias map: same node index");

    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
}

/* ==================================================================
 * Loader coverage: alias in sequence
 * Covers: yaml_parser_load_node_add sequence path
 * ================================================================== */

static void test_load_alias_in_sequence(void) {
    yaml_parser_t parser;
    yaml_document_t doc;
    const char *yaml = "- &first hello\n- *first\n- *first\n";

    ASSERT(yaml_parser_initialize(&parser), "alias seq init");
    yaml_parser_set_input_string(&parser, (const unsigned char *)yaml, strlen(yaml));
    ASSERT(yaml_parser_load(&parser, &doc), "alias seq load");

    yaml_node_t *root = yaml_document_get_root_node(&doc);
    ASSERT_NOT_NULL(root, "alias seq: root");
    ASSERT_EQ_INT(root->type, YAML_SEQUENCE_NODE, "alias seq: is seq");

    int item_count = (int)(root->data.sequence.items.top -
                           root->data.sequence.items.start);
    ASSERT_EQ_INT(item_count, 3, "alias seq: 3 items");

    /* Items 2 and 3 should reference the same node as item 1 */
    int id1 = root->data.sequence.items.start[0];
    int id2 = root->data.sequence.items.start[1];
    int id3 = root->data.sequence.items.start[2];
    ASSERT_EQ_INT(id1, id2, "alias seq: item 2 = item 1");
    ASSERT_EQ_INT(id1, id3, "alias seq: item 3 = item 1");

    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
}

/* ==================================================================
 * Loader coverage: nested sequences and mappings
 * Covers: yaml_parser_load_node_add nesting,
 *         yaml_parser_load_sequence_end, yaml_parser_load_mapping_end
 * ================================================================== */

static void test_load_nested_sequence(void) {
    yaml_parser_t parser;
    yaml_document_t doc;
    const char *yaml = "- - a\n  - b\n- - c\n  - d\n";

    ASSERT(yaml_parser_initialize(&parser), "nested seq init");
    yaml_parser_set_input_string(&parser, (const unsigned char *)yaml, strlen(yaml));
    ASSERT(yaml_parser_load(&parser, &doc), "nested seq load");

    yaml_node_t *root = yaml_document_get_root_node(&doc);
    ASSERT_NOT_NULL(root, "nested seq: root");
    ASSERT_EQ_INT(root->type, YAML_SEQUENCE_NODE, "nested seq: is seq");

    int outer_count = (int)(root->data.sequence.items.top -
                            root->data.sequence.items.start);
    ASSERT_EQ_INT(outer_count, 2, "nested seq: 2 outer items");

    /* Each inner seq should have 2 items */
    for (int i = 0; i < 2; i++) {
        int inner_id = root->data.sequence.items.start[i];
        yaml_node_t *inner = yaml_document_get_node(&doc, inner_id);
        ASSERT_NOT_NULL(inner, "nested seq: inner exists");
        ASSERT_EQ_INT(inner->type, YAML_SEQUENCE_NODE, "nested seq: inner is seq");
        int inner_count = (int)(inner->data.sequence.items.top -
                                inner->data.sequence.items.start);
        ASSERT_EQ_INT(inner_count, 2, "nested seq: inner has 2 items");
    }

    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
}

static void test_load_nested_mapping(void) {
    yaml_parser_t parser;
    yaml_document_t doc;
    const char *yaml = "outer1:\n  inner_a: 1\n  inner_b: 2\nouter2:\n  inner_c: 3\n";

    ASSERT(yaml_parser_initialize(&parser), "nested map init");
    yaml_parser_set_input_string(&parser, (const unsigned char *)yaml, strlen(yaml));
    ASSERT(yaml_parser_load(&parser, &doc), "nested map load");

    yaml_node_t *root = yaml_document_get_root_node(&doc);
    ASSERT_NOT_NULL(root, "nested map: root");
    ASSERT_EQ_INT(root->type, YAML_MAPPING_NODE, "nested map: is map");

    int pair_count = (int)(root->data.mapping.pairs.top -
                           root->data.mapping.pairs.start);
    ASSERT_EQ_INT(pair_count, 2, "nested map: 2 pairs");

    /* First value is a mapping */
    yaml_node_pair_t *p1 = root->data.mapping.pairs.start;
    yaml_node_t *v1 = yaml_document_get_node(&doc, p1->value);
    ASSERT_NOT_NULL(v1, "nested map: val1");
    ASSERT_EQ_INT(v1->type, YAML_MAPPING_NODE, "nested map: val1 is map");

    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
}

/* ==================================================================
 * Loader coverage: load empty stream
 * Covers: yaml_parser_load stream_end path
 * ================================================================== */

static void test_load_empty_stream(void) {
    yaml_parser_t parser;
    yaml_document_t doc;
    const char *yaml = "";

    ASSERT(yaml_parser_initialize(&parser), "empty stream init");
    yaml_parser_set_input_string(&parser, (const unsigned char *)yaml, strlen(yaml));

    ASSERT(yaml_parser_load(&parser, &doc), "empty stream load");
    yaml_node_t *root = yaml_document_get_root_node(&doc);
    ASSERT_NULL(root, "empty stream: no root");
    yaml_document_delete(&doc);

    /* Second load should also succeed with no content */
    ASSERT(yaml_parser_load(&parser, &doc), "empty stream load2");
    root = yaml_document_get_root_node(&doc);
    ASSERT_NULL(root, "empty stream: still no root");
    yaml_document_delete(&doc);

    yaml_parser_delete(&parser);
}

/* ==================================================================
 * Loader coverage: document with version + tag directives
 * Covers: yaml_parser_load_document directive handling
 * ================================================================== */

static void test_load_with_directives(void) {
    yaml_parser_t parser;
    yaml_document_t doc;
    const char *yaml = "%YAML 1.1\n%TAG !e! tag:example.com,2000:\n---\n!e!foo bar\n...\n";

    ASSERT(yaml_parser_initialize(&parser), "directives init");
    yaml_parser_set_input_string(&parser, (const unsigned char *)yaml, strlen(yaml));
    ASSERT(yaml_parser_load(&parser, &doc), "directives load");

    ASSERT_NOT_NULL(doc.version_directive, "directives: has version");
    if (doc.version_directive) {
        ASSERT_EQ_INT(doc.version_directive->major, 1, "directives: major");
        ASSERT_EQ_INT(doc.version_directive->minor, 1, "directives: minor");
    }
    ASSERT(doc.tag_directives.start != doc.tag_directives.end,
           "directives: has tags");

    yaml_node_t *root = yaml_document_get_root_node(&doc);
    ASSERT_NOT_NULL(root, "directives: root");
    ASSERT_EQ_INT(root->type, YAML_SCALAR_NODE, "directives: scalar");
    ASSERT_NOT_NULL(root->tag, "directives: has tag");

    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
}

/* ==================================================================
 * Loader coverage: flow collections
 * Covers: flow-style loading
 * ================================================================== */

static void test_load_flow_sequence(void) {
    yaml_parser_t parser;
    yaml_document_t doc;
    const char *yaml = "[a, b, c]\n";

    ASSERT(yaml_parser_initialize(&parser), "flow seq init");
    yaml_parser_set_input_string(&parser, (const unsigned char *)yaml, strlen(yaml));
    ASSERT(yaml_parser_load(&parser, &doc), "flow seq load");

    yaml_node_t *root = yaml_document_get_root_node(&doc);
    ASSERT_NOT_NULL(root, "flow seq: root");
    ASSERT_EQ_INT(root->type, YAML_SEQUENCE_NODE, "flow seq: type");
    ASSERT_EQ_INT(root->data.sequence.style, YAML_FLOW_SEQUENCE_STYLE, "flow seq: style");

    int count = (int)(root->data.sequence.items.top -
                      root->data.sequence.items.start);
    ASSERT_EQ_INT(count, 3, "flow seq: 3 items");

    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
}

static void test_load_flow_mapping(void) {
    yaml_parser_t parser;
    yaml_document_t doc;
    const char *yaml = "{a: 1, b: 2}\n";

    ASSERT(yaml_parser_initialize(&parser), "flow map init");
    yaml_parser_set_input_string(&parser, (const unsigned char *)yaml, strlen(yaml));
    ASSERT(yaml_parser_load(&parser, &doc), "flow map load");

    yaml_node_t *root = yaml_document_get_root_node(&doc);
    ASSERT_NOT_NULL(root, "flow map: root");
    ASSERT_EQ_INT(root->type, YAML_MAPPING_NODE, "flow map: type");
    ASSERT_EQ_INT(root->data.mapping.style, YAML_FLOW_MAPPING_STYLE, "flow map: style");

    int count = (int)(root->data.mapping.pairs.top -
                      root->data.mapping.pairs.start);
    ASSERT_EQ_INT(count, 2, "flow map: 2 pairs");

    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
}

/* ==================================================================
 * Loader coverage: scalar styles preserved
 * ================================================================== */

static void test_load_scalar_styles(void) {
    yaml_parser_t parser;
    yaml_document_t doc;
    const char *yaml =
        "- plain\n"
        "- 'single'\n"
        "- \"double\"\n"
        "- |\n  literal\n"
        "- >\n  folded\n";

    ASSERT(yaml_parser_initialize(&parser), "scalar styles init");
    yaml_parser_set_input_string(&parser, (const unsigned char *)yaml, strlen(yaml));
    ASSERT(yaml_parser_load(&parser, &doc), "scalar styles load");

    yaml_node_t *root = yaml_document_get_root_node(&doc);
    ASSERT_NOT_NULL(root, "scalar styles: root");
    ASSERT_EQ_INT(root->type, YAML_SEQUENCE_NODE, "scalar styles: is seq");

    int count = (int)(root->data.sequence.items.top -
                      root->data.sequence.items.start);
    ASSERT_EQ_INT(count, 5, "scalar styles: 5 items");

    /* Check each style */
    yaml_scalar_style_t expected_styles[] = {
        YAML_PLAIN_SCALAR_STYLE,
        YAML_SINGLE_QUOTED_SCALAR_STYLE,
        YAML_DOUBLE_QUOTED_SCALAR_STYLE,
        YAML_LITERAL_SCALAR_STYLE,
        YAML_FOLDED_SCALAR_STYLE
    };
    for (int i = 0; i < 5; i++) {
        int id = root->data.sequence.items.start[i];
        yaml_node_t *n = yaml_document_get_node(&doc, id);
        ASSERT_NOT_NULL(n, "scalar styles: node exists");
        ASSERT_EQ_INT(n->type, YAML_SCALAR_NODE, "scalar styles: is scalar");
        ASSERT_EQ_INT(n->data.scalar.style, expected_styles[i], "scalar styles: style match");
    }

    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
}

/* ==================================================================
 * Document API: document_initialize with tag directives
 * Covers: yaml_document_start_event_initialize tag directive copying,
 *         yaml_document_initialize tag directive allocation
 * ================================================================== */

static void test_document_init_with_multiple_tags(void) {
    yaml_document_t doc;
    yaml_version_directive_t version = {1, 1};
    yaml_tag_directive_t tags[] = {
        {(yaml_char_t *)"!", (yaml_char_t *)"!"},
        {(yaml_char_t *)"!e!", (yaml_char_t *)"tag:example.com,2000:"},
        {(yaml_char_t *)"!x!", (yaml_char_t *)"tag:example.org,2001:"}
    };

    ASSERT(yaml_document_initialize(&doc, &version, tags, tags + 3, 0, 0),
           "multi tags init");
    ASSERT_NOT_NULL(doc.version_directive, "multi tags: has version");
    ASSERT(doc.tag_directives.start != doc.tag_directives.end,
           "multi tags: has tag directives");

    yaml_document_delete(&doc);
}

/* ==================================================================
 * Document API: add_scalar/add_sequence/add_mapping with NULL tag
 * Covers: default tag substitution in document node creation
 * ================================================================== */

static void test_add_scalar_null_tag(void) {
    yaml_document_t doc;
    ASSERT(yaml_document_initialize(&doc, NULL, NULL, NULL, 1, 1), "null tag init");

    int id = yaml_document_add_scalar(&doc, NULL,
             (const yaml_char_t *)"test", 4, YAML_PLAIN_SCALAR_STYLE);
    ASSERT(id > 0, "null tag scalar: id > 0");

    yaml_node_t *node = yaml_document_get_node(&doc, id);
    ASSERT_NOT_NULL(node, "null tag scalar: node");
    ASSERT_NOT_NULL(node->tag, "null tag scalar: has default tag");

    yaml_document_delete(&doc);
}

static void test_add_sequence_null_tag(void) {
    yaml_document_t doc;
    ASSERT(yaml_document_initialize(&doc, NULL, NULL, NULL, 1, 1), "null tag seq init");

    int id = yaml_document_add_sequence(&doc, NULL, YAML_BLOCK_SEQUENCE_STYLE);
    ASSERT(id > 0, "null tag seq: id > 0");

    yaml_node_t *node = yaml_document_get_node(&doc, id);
    ASSERT_NOT_NULL(node, "null tag seq: node");
    ASSERT_NOT_NULL(node->tag, "null tag seq: has default tag");

    yaml_document_delete(&doc);
}

static void test_add_mapping_null_tag(void) {
    yaml_document_t doc;
    ASSERT(yaml_document_initialize(&doc, NULL, NULL, NULL, 1, 1), "null tag map init");

    int id = yaml_document_add_mapping(&doc, NULL, YAML_BLOCK_MAPPING_STYLE);
    ASSERT(id > 0, "null tag map: id > 0");

    yaml_node_t *node = yaml_document_get_node(&doc, id);
    ASSERT_NOT_NULL(node, "null tag map: node");
    ASSERT_NOT_NULL(node->tag, "null tag map: has default tag");

    yaml_document_delete(&doc);
}

/* ==================================================================
 * Document API: multiple scalars in sequence
 * ================================================================== */

static void test_sequence_many_items(void) {
    yaml_document_t doc;
    ASSERT(yaml_document_initialize(&doc, NULL, NULL, NULL, 1, 1), "many items init");

    int seq_id = yaml_document_add_sequence(&doc,
                 (const yaml_char_t *)YAML_SEQ_TAG, YAML_BLOCK_SEQUENCE_STYLE);

    for (int i = 0; i < 20; i++) {
        char buf[8];
        snprintf(buf, sizeof(buf), "item%d", i);
        int sid = yaml_document_add_scalar(&doc, (const yaml_char_t *)YAML_STR_TAG,
                   (const yaml_char_t *)buf, strlen(buf), YAML_PLAIN_SCALAR_STYLE);
        ASSERT(yaml_document_append_sequence_item(&doc, seq_id, sid), "many: append");
    }

    yaml_node_t *seq = yaml_document_get_node(&doc, seq_id);
    int count = (int)(seq->data.sequence.items.top - seq->data.sequence.items.start);
    ASSERT_EQ_INT(count, 20, "many items: 20 items");

    yaml_document_delete(&doc);
}

/* ==================================================================
 * Document API: multiple pairs in mapping
 * ================================================================== */

static void test_mapping_many_pairs(void) {
    yaml_document_t doc;
    ASSERT(yaml_document_initialize(&doc, NULL, NULL, NULL, 1, 1), "many pairs init");

    int map_id = yaml_document_add_mapping(&doc,
                 (const yaml_char_t *)YAML_MAP_TAG, YAML_BLOCK_MAPPING_STYLE);

    for (int i = 0; i < 15; i++) {
        char kbuf[8], vbuf[8];
        snprintf(kbuf, sizeof(kbuf), "k%d", i);
        snprintf(vbuf, sizeof(vbuf), "v%d", i);
        int kid = yaml_document_add_scalar(&doc, (const yaml_char_t *)YAML_STR_TAG,
                   (const yaml_char_t *)kbuf, strlen(kbuf), YAML_PLAIN_SCALAR_STYLE);
        int vid = yaml_document_add_scalar(&doc, (const yaml_char_t *)YAML_STR_TAG,
                   (const yaml_char_t *)vbuf, strlen(vbuf), YAML_PLAIN_SCALAR_STYLE);
        ASSERT(yaml_document_append_mapping_pair(&doc, map_id, kid, vid), "many pairs: append");
    }

    yaml_node_t *map = yaml_document_get_node(&doc, map_id);
    int count = (int)(map->data.mapping.pairs.top - map->data.mapping.pairs.start);
    ASSERT_EQ_INT(count, 15, "many pairs: 15 pairs");

    yaml_document_delete(&doc);
}

/* ==================================================================
 * Loader: complex nested structure with anchors and aliases
 * ================================================================== */

static void test_load_complex_structure(void) {
    yaml_parser_t parser;
    yaml_document_t doc;
    const char *yaml =
        "servers:\n"
        "  - &web\n"
        "    host: web.example.com\n"
        "    port: 80\n"
        "  - &db\n"
        "    host: db.example.com\n"
        "    port: 5432\n"
        "primary: *web\n"
        "secondary: *db\n";

    ASSERT(yaml_parser_initialize(&parser), "complex init");
    yaml_parser_set_input_string(&parser, (const unsigned char *)yaml, strlen(yaml));
    ASSERT(yaml_parser_load(&parser, &doc), "complex load");

    yaml_node_t *root = yaml_document_get_root_node(&doc);
    ASSERT_NOT_NULL(root, "complex: root");
    ASSERT_EQ_INT(root->type, YAML_MAPPING_NODE, "complex: is mapping");

    int pair_count = (int)(root->data.mapping.pairs.top -
                           root->data.mapping.pairs.start);
    ASSERT_EQ_INT(pair_count, 3, "complex: 3 pairs");

    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
}

/* ==================================================================
 * Loader: implicit document end
 * ================================================================== */

static void test_load_implicit_doc_end(void) {
    yaml_parser_t parser;
    yaml_document_t doc;
    const char *yaml = "hello\n";

    ASSERT(yaml_parser_initialize(&parser), "implicit end init");
    yaml_parser_set_input_string(&parser, (const unsigned char *)yaml, strlen(yaml));
    ASSERT(yaml_parser_load(&parser, &doc), "implicit end load");

    ASSERT(doc.end_implicit, "implicit end: end is implicit");

    yaml_node_t *root = yaml_document_get_root_node(&doc);
    ASSERT_NOT_NULL(root, "implicit end: root");
    ASSERT_EQ_STR((const char *)root->data.scalar.value, "hello", "implicit end: value");

    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
}

static void test_load_explicit_doc_end(void) {
    yaml_parser_t parser;
    yaml_document_t doc;
    const char *yaml = "---\nhello\n...\n";

    ASSERT(yaml_parser_initialize(&parser), "explicit end init");
    yaml_parser_set_input_string(&parser, (const unsigned char *)yaml, strlen(yaml));
    ASSERT(yaml_parser_load(&parser, &doc), "explicit end load");

    ASSERT(!doc.start_implicit, "explicit end: start not implicit");

    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
}

/* ==================================================================
 * Document delete: multiple nodes with tags
 * Covers: yaml_document_delete multi-node cleanup paths
 * ================================================================== */

static void test_document_delete_complex(void) {
    yaml_document_t doc;
    ASSERT(yaml_document_initialize(&doc, NULL, NULL, NULL, 1, 1), "delete complex init");

    int map_id = yaml_document_add_mapping(&doc,
                 (const yaml_char_t *)YAML_MAP_TAG, YAML_BLOCK_MAPPING_STYLE);
    int seq_id = yaml_document_add_sequence(&doc,
                 (const yaml_char_t *)YAML_SEQ_TAG, YAML_BLOCK_SEQUENCE_STYLE);
    int s1 = yaml_document_add_scalar(&doc, (const yaml_char_t *)YAML_STR_TAG,
              (const yaml_char_t *)"a", 1, YAML_PLAIN_SCALAR_STYLE);
    int s2 = yaml_document_add_scalar(&doc, (const yaml_char_t *)YAML_STR_TAG,
              (const yaml_char_t *)"b", 1, YAML_PLAIN_SCALAR_STYLE);

    yaml_document_append_mapping_pair(&doc, map_id, s1, seq_id);
    yaml_document_append_sequence_item(&doc, seq_id, s2);

    /* Delete frees all nodes recursively */
    yaml_document_delete(&doc);

    /* If we get here without crash, cleanup worked */
    ASSERT(1, "delete complex: no crash");
}

int main(void) {
    TEST_SUITE_BEGIN("Document API");

    test_document_init();
    test_add_scalar();
    test_add_sequence();
    test_add_mapping();
    test_load_document();
    test_load_sequence();
    test_load_multiple_documents();
    test_load_anchors();
    test_load_nested();
    test_get_node_bounds();
    test_empty_document();

    /* Loader error paths */
    test_load_duplicate_anchor();
    test_load_undefined_alias();

    /* Loader anchor handling */
    test_load_anchored_sequence();
    test_load_anchored_mapping();
    test_load_anchored_scalar();

    /* Loader tag handling */
    test_load_tagged_scalar();
    test_load_tagged_sequence();
    test_load_tagged_mapping();

    /* Loader alias resolution */
    test_load_alias_in_mapping();
    test_load_alias_in_sequence();

    /* Loader nested collections */
    test_load_nested_sequence();
    test_load_nested_mapping();

    /* Loader edge cases */
    test_load_empty_stream();
    test_load_with_directives();
    test_load_flow_sequence();
    test_load_flow_mapping();
    test_load_scalar_styles();
    test_load_implicit_doc_end();
    test_load_explicit_doc_end();
    test_load_complex_structure();

    /* Document API coverage */
    test_document_init_with_multiple_tags();
    test_add_scalar_null_tag();
    test_add_sequence_null_tag();
    test_add_mapping_null_tag();
    test_sequence_many_items();
    test_mapping_many_pairs();
    test_document_delete_complex();

    TEST_SUITE_END();
}
