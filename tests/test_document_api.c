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

    TEST_SUITE_END();
}
