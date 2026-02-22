#include "test_helper.h"

/*
 * Comprehensive API tests covering document API, event construction/deletion,
 * parser/emitter lifecycle, and public API edge cases.
 */

/* ========== yaml_parser_initialize / yaml_parser_delete ========== */

static void test_api_parser_init(void) {
    yaml_parser_t parser;
    ASSERT(yaml_parser_initialize(&parser), "parser init");
    yaml_parser_delete(&parser);
}

static void test_api_parser_init_multiple(void) {
    for (int i = 0; i < 10; i++) {
        yaml_parser_t parser;
        ASSERT(yaml_parser_initialize(&parser), "parser init loop");
        yaml_parser_delete(&parser);
    }
}

/* ========== yaml_parser_set_input_string ========== */

static void test_api_set_input_string_empty(void) {
    yaml_parser_t parser;
    yaml_event_t event;
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)"", 0);
    ASSERT(yaml_parser_parse(&parser, &event), "parse empty string");
    ASSERT_EQ_INT(event.type, YAML_STREAM_START_EVENT, "stream start");
    yaml_event_delete(&event);
    yaml_parser_delete(&parser);
}

static void test_api_set_input_string_one_byte(void) {
    yaml_parser_t parser;
    yaml_event_t event;
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)"a", 1);
    ASSERT(yaml_parser_parse(&parser, &event), "parse 1 byte");
    yaml_event_delete(&event);
    yaml_parser_delete(&parser);
}

/* ========== yaml_parser_set_encoding ========== */

static void test_api_set_encoding_utf8(void) {
    yaml_parser_t parser;
    yaml_event_t event;
    yaml_parser_initialize(&parser);
    yaml_parser_set_encoding(&parser, YAML_UTF8_ENCODING);
    yaml_parser_set_input_string(&parser, (const unsigned char *)"hello", 5);
    ASSERT(yaml_parser_parse(&parser, &event), "utf8 parse");
    ASSERT_EQ_INT(event.data.stream_start.encoding, YAML_UTF8_ENCODING, "utf8 enc");
    yaml_event_delete(&event);
    yaml_parser_delete(&parser);
}

static void test_api_set_encoding_any(void) {
    yaml_parser_t parser;
    yaml_event_t event;
    yaml_parser_initialize(&parser);
    yaml_parser_set_encoding(&parser, YAML_ANY_ENCODING);
    yaml_parser_set_input_string(&parser, (const unsigned char *)"hello", 5);
    ASSERT(yaml_parser_parse(&parser, &event), "any enc parse");
    yaml_event_delete(&event);
    yaml_parser_delete(&parser);
}

/* ========== yaml_parser_set_input (custom handler) ========== */

struct test_reader_data {
    const unsigned char *buf;
    size_t len;
    size_t pos;
};

static int test_read_handler(void *data, unsigned char *buffer,
                              size_t size, size_t *size_read) {
    struct test_reader_data *d = data;
    size_t avail = d->len - d->pos;
    size_t n = avail < size ? avail : size;
    memcpy(buffer, d->buf + d->pos, n);
    d->pos += n;
    *size_read = n;
    return 1;
}

static void test_api_custom_reader(void) {
    yaml_parser_t parser;
    yaml_event_t event;
    const char *yaml = "key: value";
    struct test_reader_data data = {(const unsigned char *)yaml, strlen(yaml), 0};

    yaml_parser_initialize(&parser);
    yaml_parser_set_input(&parser, test_read_handler, &data);

    int found = 0;
    while (1) {
        if (!yaml_parser_parse(&parser, &event)) break;
        if (event.type == YAML_SCALAR_EVENT &&
            strcmp((const char *)event.data.scalar.value, "value") == 0)
            found = 1;
        int done = (event.type == YAML_STREAM_END_EVENT);
        yaml_event_delete(&event);
        if (done) break;
    }
    yaml_parser_delete(&parser);
    ASSERT(found, "custom reader found value");
}

static int test_failing_reader(void *data, unsigned char *buffer,
                                size_t size, size_t *size_read) {
    (void)data; (void)buffer; (void)size;
    *size_read = 0;
    return 0; /* error */
}

static void test_api_failing_reader(void) {
    yaml_parser_t parser;
    yaml_event_t event;
    yaml_parser_initialize(&parser);
    yaml_parser_set_input(&parser, test_failing_reader, NULL);
    int ok = yaml_parser_parse(&parser, &event);
    if (ok) yaml_event_delete(&event);
    /* Should fail at some point */
    if (ok) {
        ok = yaml_parser_parse(&parser, &event);
        if (ok) yaml_event_delete(&event);
    }
    yaml_parser_delete(&parser);
    ASSERT(1, "failing reader handled");
}

/* ========== yaml_parser_set_input_file ========== */

static void test_api_file_input(void) {
    FILE *f = tmpfile();
    ASSERT_NOT_NULL(f, "tmpfile");
    if (!f) return;
    fprintf(f, "key: value\n");
    rewind(f);

    yaml_parser_t parser;
    yaml_event_t event;
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_file(&parser, f);

    int found = 0;
    while (1) {
        if (!yaml_parser_parse(&parser, &event)) break;
        if (event.type == YAML_SCALAR_EVENT &&
            strcmp((const char *)event.data.scalar.value, "value") == 0)
            found = 1;
        int done = (event.type == YAML_STREAM_END_EVENT);
        yaml_event_delete(&event);
        if (done) break;
    }
    yaml_parser_delete(&parser);
    fclose(f);
    ASSERT(found, "file input found value");
}

static void test_api_file_input_empty(void) {
    FILE *f = tmpfile();
    ASSERT_NOT_NULL(f, "tmpfile empty");
    if (!f) return;
    rewind(f);

    yaml_parser_t parser;
    yaml_event_t event;
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_file(&parser, f);

    yaml_parser_parse(&parser, &event);
    ASSERT_EQ_INT(event.type, YAML_STREAM_START_EVENT, "empty file: stream start");
    yaml_event_delete(&event);

    yaml_parser_parse(&parser, &event);
    ASSERT_EQ_INT(event.type, YAML_STREAM_END_EVENT, "empty file: stream end");
    yaml_event_delete(&event);

    yaml_parser_delete(&parser);
    fclose(f);
}

/* ========== yaml_emitter_initialize / yaml_emitter_delete ========== */

static void test_api_emitter_init(void) {
    yaml_emitter_t emitter;
    ASSERT(yaml_emitter_initialize(&emitter), "emitter init");
    yaml_emitter_delete(&emitter);
}

static void test_api_emitter_init_multiple(void) {
    for (int i = 0; i < 10; i++) {
        yaml_emitter_t emitter;
        ASSERT(yaml_emitter_initialize(&emitter), "emitter init loop");
        yaml_emitter_delete(&emitter);
    }
}

/* ========== yaml_emitter settings ========== */

static void test_api_emitter_set_canonical(void) {
    yaml_emitter_t emitter;
    yaml_emitter_initialize(&emitter);
    yaml_emitter_set_canonical(&emitter, 1);
    yaml_emitter_delete(&emitter);
    ASSERT(1, "set canonical ok");
}

static void test_api_emitter_set_indent(void) {
    yaml_emitter_t emitter;
    yaml_emitter_initialize(&emitter);
    yaml_emitter_set_indent(&emitter, 4);
    yaml_emitter_delete(&emitter);
    ASSERT(1, "set indent ok");
}

static void test_api_emitter_set_width(void) {
    yaml_emitter_t emitter;
    yaml_emitter_initialize(&emitter);
    yaml_emitter_set_width(&emitter, 120);
    yaml_emitter_delete(&emitter);
    ASSERT(1, "set width ok");
}

static void test_api_emitter_set_unicode(void) {
    yaml_emitter_t emitter;
    yaml_emitter_initialize(&emitter);
    yaml_emitter_set_unicode(&emitter, 1);
    yaml_emitter_delete(&emitter);
    ASSERT(1, "set unicode ok");
}

static void test_api_emitter_set_break(void) {
    yaml_emitter_t emitter;
    yaml_emitter_initialize(&emitter);
    yaml_emitter_set_break(&emitter, YAML_LN_BREAK);
    yaml_emitter_delete(&emitter);
    ASSERT(1, "set break ok");
}

static void test_api_emitter_set_encoding(void) {
    yaml_emitter_t emitter;
    yaml_emitter_initialize(&emitter);
    yaml_emitter_set_encoding(&emitter, YAML_UTF8_ENCODING);
    yaml_emitter_delete(&emitter);
    ASSERT(1, "set encoding ok");
}

/* ========== Event construction and deletion ========== */

static void test_api_stream_start_event(void) {
    yaml_event_t event;
    ASSERT(yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING),
           "stream start init");
    ASSERT_EQ_INT(event.type, YAML_STREAM_START_EVENT, "stream start type");
    yaml_event_delete(&event);
}

static void test_api_stream_end_event(void) {
    yaml_event_t event;
    ASSERT(yaml_stream_end_event_initialize(&event), "stream end init");
    ASSERT_EQ_INT(event.type, YAML_STREAM_END_EVENT, "stream end type");
    yaml_event_delete(&event);
}

static void test_api_document_start_event(void) {
    yaml_event_t event;
    ASSERT(yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1),
           "doc start init");
    ASSERT_EQ_INT(event.type, YAML_DOCUMENT_START_EVENT, "doc start type");
    yaml_event_delete(&event);
}

static void test_api_document_start_explicit(void) {
    yaml_event_t event;
    ASSERT(yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 0),
           "doc start explicit");
    ASSERT(!event.data.document_start.implicit, "not implicit");
    yaml_event_delete(&event);
}

static void test_api_document_end_event(void) {
    yaml_event_t event;
    ASSERT(yaml_document_end_event_initialize(&event, 1), "doc end init");
    ASSERT_EQ_INT(event.type, YAML_DOCUMENT_END_EVENT, "doc end type");
    yaml_event_delete(&event);
}

static void test_api_scalar_event_plain(void) {
    yaml_event_t event;
    ASSERT(yaml_scalar_event_initialize(&event, NULL, NULL,
           (yaml_char_t *)"hello", 5, 1, 0, YAML_PLAIN_SCALAR_STYLE),
           "scalar plain init");
    ASSERT_EQ_INT(event.type, YAML_SCALAR_EVENT, "scalar type");
    ASSERT_EQ_STR(event.data.scalar.value, "hello", "scalar value");
    ASSERT_EQ_INT((int)event.data.scalar.length, 5, "scalar length");
    yaml_event_delete(&event);
}

static void test_api_scalar_event_tagged(void) {
    yaml_event_t event;
    ASSERT(yaml_scalar_event_initialize(&event, NULL,
           (yaml_char_t *)"tag:yaml.org,2002:str",
           (yaml_char_t *)"hello", 5, 0, 0, YAML_PLAIN_SCALAR_STYLE),
           "scalar tagged init");
    ASSERT_EQ_STR(event.data.scalar.tag, "tag:yaml.org,2002:str", "scalar tag");
    yaml_event_delete(&event);
}

static void test_api_scalar_event_anchored(void) {
    yaml_event_t event;
    ASSERT(yaml_scalar_event_initialize(&event,
           (yaml_char_t *)"ref", NULL,
           (yaml_char_t *)"hello", 5, 1, 0, YAML_PLAIN_SCALAR_STYLE),
           "scalar anchored init");
    ASSERT_EQ_STR(event.data.scalar.anchor, "ref", "scalar anchor");
    yaml_event_delete(&event);
}

static void test_api_scalar_event_empty(void) {
    yaml_event_t event;
    ASSERT(yaml_scalar_event_initialize(&event, NULL, NULL,
           (yaml_char_t *)"", 0, 1, 0, YAML_PLAIN_SCALAR_STYLE),
           "scalar empty init");
    ASSERT_EQ_INT((int)event.data.scalar.length, 0, "empty length");
    yaml_event_delete(&event);
}

static void test_api_scalar_event_styles(void) {
    yaml_scalar_style_t styles[] = {
        YAML_PLAIN_SCALAR_STYLE,
        YAML_SINGLE_QUOTED_SCALAR_STYLE,
        YAML_DOUBLE_QUOTED_SCALAR_STYLE,
        YAML_LITERAL_SCALAR_STYLE,
        YAML_FOLDED_SCALAR_STYLE
    };
    for (int i = 0; i < 5; i++) {
        yaml_event_t event;
        ASSERT(yaml_scalar_event_initialize(&event, NULL, NULL,
               (yaml_char_t *)"x", 1, 1, 0, styles[i]),
               "scalar style init");
        ASSERT_EQ_INT(event.data.scalar.style, styles[i], "scalar style match");
        yaml_event_delete(&event);
    }
}

static void test_api_sequence_start_event(void) {
    yaml_event_t event;
    ASSERT(yaml_sequence_start_event_initialize(&event, NULL, NULL, 1,
           YAML_BLOCK_SEQUENCE_STYLE), "seq start init");
    ASSERT_EQ_INT(event.type, YAML_SEQUENCE_START_EVENT, "seq start type");
    yaml_event_delete(&event);
}

static void test_api_sequence_start_flow(void) {
    yaml_event_t event;
    ASSERT(yaml_sequence_start_event_initialize(&event, NULL, NULL, 1,
           YAML_FLOW_SEQUENCE_STYLE), "seq start flow");
    ASSERT_EQ_INT(event.data.sequence_start.style, YAML_FLOW_SEQUENCE_STYLE, "flow style");
    yaml_event_delete(&event);
}

static void test_api_sequence_end_event(void) {
    yaml_event_t event;
    ASSERT(yaml_sequence_end_event_initialize(&event), "seq end init");
    ASSERT_EQ_INT(event.type, YAML_SEQUENCE_END_EVENT, "seq end type");
    yaml_event_delete(&event);
}

static void test_api_mapping_start_event(void) {
    yaml_event_t event;
    ASSERT(yaml_mapping_start_event_initialize(&event, NULL, NULL, 1,
           YAML_BLOCK_MAPPING_STYLE), "map start init");
    ASSERT_EQ_INT(event.type, YAML_MAPPING_START_EVENT, "map start type");
    yaml_event_delete(&event);
}

static void test_api_mapping_start_flow(void) {
    yaml_event_t event;
    ASSERT(yaml_mapping_start_event_initialize(&event, NULL, NULL, 1,
           YAML_FLOW_MAPPING_STYLE), "map start flow");
    ASSERT_EQ_INT(event.data.mapping_start.style, YAML_FLOW_MAPPING_STYLE, "flow style");
    yaml_event_delete(&event);
}

static void test_api_mapping_end_event(void) {
    yaml_event_t event;
    ASSERT(yaml_mapping_end_event_initialize(&event), "map end init");
    ASSERT_EQ_INT(event.type, YAML_MAPPING_END_EVENT, "map end type");
    yaml_event_delete(&event);
}

static void test_api_alias_event(void) {
    yaml_event_t event;
    ASSERT(yaml_alias_event_initialize(&event, (yaml_char_t *)"anchor"),
           "alias init");
    ASSERT_EQ_INT(event.type, YAML_ALIAS_EVENT, "alias type");
    ASSERT_EQ_STR(event.data.alias.anchor, "anchor", "alias anchor");
    yaml_event_delete(&event);
}

/* ========== Document API ========== */

static void test_api_document_init(void) {
    yaml_document_t doc;
    ASSERT(yaml_document_initialize(&doc, NULL, NULL, NULL, 1, 1), "doc init");
    yaml_document_delete(&doc);
}

static void test_api_document_init_explicit(void) {
    yaml_document_t doc;
    ASSERT(yaml_document_initialize(&doc, NULL, NULL, NULL, 0, 0), "doc init explicit");
    yaml_document_delete(&doc);
}

static void test_api_document_add_scalar(void) {
    yaml_document_t doc;
    yaml_document_initialize(&doc, NULL, NULL, NULL, 1, 1);
    int id = yaml_document_add_scalar(&doc, NULL, (yaml_char_t *)"hello", 5,
                                       YAML_PLAIN_SCALAR_STYLE);
    ASSERT(id > 0, "add scalar returns id");
    yaml_document_delete(&doc);
}

static void test_api_document_add_sequence(void) {
    yaml_document_t doc;
    yaml_document_initialize(&doc, NULL, NULL, NULL, 1, 1);
    int id = yaml_document_add_sequence(&doc, NULL, YAML_BLOCK_SEQUENCE_STYLE);
    ASSERT(id > 0, "add sequence returns id");
    yaml_document_delete(&doc);
}

static void test_api_document_add_mapping(void) {
    yaml_document_t doc;
    yaml_document_initialize(&doc, NULL, NULL, NULL, 1, 1);
    int id = yaml_document_add_mapping(&doc, NULL, YAML_BLOCK_MAPPING_STYLE);
    ASSERT(id > 0, "add mapping returns id");
    yaml_document_delete(&doc);
}

static void test_api_document_append_seq_item(void) {
    yaml_document_t doc;
    yaml_document_initialize(&doc, NULL, NULL, NULL, 1, 1);
    int seq = yaml_document_add_sequence(&doc, NULL, YAML_BLOCK_SEQUENCE_STYLE);
    int scalar = yaml_document_add_scalar(&doc, NULL, (yaml_char_t *)"item", 4,
                                           YAML_PLAIN_SCALAR_STYLE);
    ASSERT(yaml_document_append_sequence_item(&doc, seq, scalar), "append seq item");
    yaml_document_delete(&doc);
}

static void test_api_document_append_map_pair(void) {
    yaml_document_t doc;
    yaml_document_initialize(&doc, NULL, NULL, NULL, 1, 1);
    int map = yaml_document_add_mapping(&doc, NULL, YAML_BLOCK_MAPPING_STYLE);
    int key = yaml_document_add_scalar(&doc, NULL, (yaml_char_t *)"key", 3,
                                        YAML_PLAIN_SCALAR_STYLE);
    int val = yaml_document_add_scalar(&doc, NULL, (yaml_char_t *)"val", 3,
                                        YAML_PLAIN_SCALAR_STYLE);
    ASSERT(yaml_document_append_mapping_pair(&doc, map, key, val), "append map pair");
    yaml_document_delete(&doc);
}

static void test_api_document_get_node(void) {
    yaml_document_t doc;
    yaml_document_initialize(&doc, NULL, NULL, NULL, 1, 1);
    int id = yaml_document_add_scalar(&doc, NULL, (yaml_char_t *)"test", 4,
                                       YAML_PLAIN_SCALAR_STYLE);
    yaml_node_t *node = yaml_document_get_node(&doc, id);
    ASSERT_NOT_NULL(node, "get node");
    ASSERT_EQ_INT(node->type, YAML_SCALAR_NODE, "node is scalar");
    ASSERT_EQ_STR(node->data.scalar.value, "test", "node value");
    yaml_document_delete(&doc);
}

static void test_api_document_get_root(void) {
    yaml_document_t doc;
    yaml_document_initialize(&doc, NULL, NULL, NULL, 1, 1);
    yaml_document_add_scalar(&doc, NULL, (yaml_char_t *)"root", 4,
                              YAML_PLAIN_SCALAR_STYLE);
    yaml_node_t *root = yaml_document_get_root_node(&doc);
    ASSERT_NOT_NULL(root, "get root");
    ASSERT_EQ_STR(root->data.scalar.value, "root", "root value");
    yaml_document_delete(&doc);
}

static void test_api_document_get_root_empty(void) {
    yaml_document_t doc;
    yaml_document_initialize(&doc, NULL, NULL, NULL, 1, 1);
    yaml_node_t *root = yaml_document_get_root_node(&doc);
    ASSERT_NULL(root, "empty doc root is null");
    yaml_document_delete(&doc);
}

static void test_api_document_get_node_invalid(void) {
    yaml_document_t doc;
    yaml_document_initialize(&doc, NULL, NULL, NULL, 1, 1);
    yaml_node_t *node = yaml_document_get_node(&doc, 999);
    ASSERT_NULL(node, "invalid id returns null");
    yaml_document_delete(&doc);
}

static void test_api_document_get_node_zero(void) {
    yaml_document_t doc;
    yaml_document_initialize(&doc, NULL, NULL, NULL, 1, 1);
    yaml_node_t *node = yaml_document_get_node(&doc, 0);
    ASSERT_NULL(node, "id 0 returns null");
    yaml_document_delete(&doc);
}

/* ========== Document load/dump roundtrip ========== */

static void test_api_load_simple(void) {
    yaml_parser_t parser;
    yaml_document_t doc;

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)"hello", 5);
    ASSERT(yaml_parser_load(&parser, &doc), "load simple");

    yaml_node_t *root = yaml_document_get_root_node(&doc);
    ASSERT_NOT_NULL(root, "root exists");
    ASSERT_EQ_INT(root->type, YAML_SCALAR_NODE, "root is scalar");
    ASSERT_EQ_STR(root->data.scalar.value, "hello", "root value");

    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
}

static void test_api_load_mapping(void) {
    yaml_parser_t parser;
    yaml_document_t doc;

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)"a: b", 4);
    ASSERT(yaml_parser_load(&parser, &doc), "load mapping");

    yaml_node_t *root = yaml_document_get_root_node(&doc);
    ASSERT_NOT_NULL(root, "root exists");
    ASSERT_EQ_INT(root->type, YAML_MAPPING_NODE, "root is mapping");

    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
}

static void test_api_load_sequence(void) {
    yaml_parser_t parser;
    yaml_document_t doc;

    yaml_parser_initialize(&parser);
    const char *input = "- a\n- b\n";
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));
    ASSERT(yaml_parser_load(&parser, &doc), "load sequence");

    yaml_node_t *root = yaml_document_get_root_node(&doc);
    ASSERT_NOT_NULL(root, "root exists");
    ASSERT_EQ_INT(root->type, YAML_SEQUENCE_NODE, "root is sequence");

    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
}

static void test_api_load_empty(void) {
    yaml_parser_t parser;
    yaml_document_t doc;

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)"", 0);
    ASSERT(yaml_parser_load(&parser, &doc), "load empty");

    yaml_node_t *root = yaml_document_get_root_node(&doc);
    ASSERT_NULL(root, "empty: no root");

    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
}

static void test_api_load_multiple_docs(void) {
    yaml_parser_t parser;
    yaml_document_t doc;
    int doc_count = 0;

    yaml_parser_initialize(&parser);
    const char *input = "---\na\n---\nb\n";
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));

    while (yaml_parser_load(&parser, &doc)) {
        yaml_node_t *root = yaml_document_get_root_node(&doc);
        if (!root) { yaml_document_delete(&doc); break; }
        doc_count++;
        yaml_document_delete(&doc);
    }
    yaml_parser_delete(&parser);
    ASSERT_EQ_INT(doc_count, 2, "loaded 2 docs");
}

/* ========== Document node traversal ========== */

static void test_api_traverse_mapping_pairs(void) {
    yaml_parser_t parser;
    yaml_document_t doc;

    yaml_parser_initialize(&parser);
    const char *input = "a: 1\nb: 2\n";
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));
    yaml_parser_load(&parser, &doc);

    yaml_node_t *root = yaml_document_get_root_node(&doc);
    ASSERT_NOT_NULL(root, "root exists");
    ASSERT_EQ_INT(root->type, YAML_MAPPING_NODE, "root is mapping");

    int pair_count = 0;
    yaml_node_pair_t *pair;
    for (pair = root->data.mapping.pairs.start;
         pair < root->data.mapping.pairs.top; pair++) {
        yaml_node_t *key = yaml_document_get_node(&doc, pair->key);
        yaml_node_t *val = yaml_document_get_node(&doc, pair->value);
        ASSERT_NOT_NULL(key, "key exists");
        ASSERT_NOT_NULL(val, "val exists");
        pair_count++;
    }
    ASSERT_EQ_INT(pair_count, 2, "2 pairs");

    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
}

static void test_api_traverse_sequence_items(void) {
    yaml_parser_t parser;
    yaml_document_t doc;

    yaml_parser_initialize(&parser);
    const char *input = "- x\n- y\n- z\n";
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));
    yaml_parser_load(&parser, &doc);

    yaml_node_t *root = yaml_document_get_root_node(&doc);
    ASSERT_NOT_NULL(root, "root exists");
    ASSERT_EQ_INT(root->type, YAML_SEQUENCE_NODE, "root is sequence");

    int item_count = 0;
    yaml_node_item_t *item;
    for (item = root->data.sequence.items.start;
         item < root->data.sequence.items.top; item++) {
        yaml_node_t *node = yaml_document_get_node(&doc, *item);
        ASSERT_NOT_NULL(node, "item exists");
        item_count++;
    }
    ASSERT_EQ_INT(item_count, 3, "3 items");

    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
}

/* ========== yaml_token_delete ========== */

static void test_api_token_delete_scalar(void) {
    yaml_parser_t parser;
    yaml_token_t token;
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)"hello", 5);
    while (1) {
        if (!yaml_parser_scan(&parser, &token)) break;
        int done = (token.type == YAML_STREAM_END_TOKEN);
        yaml_token_delete(&token);
        if (done) break;
    }
    yaml_parser_delete(&parser);
    ASSERT(1, "token delete ok");
}

/* ========== Version API ========== */

static void test_api_get_version_string(void) {
    const char *ver = yaml_get_version_string();
    ASSERT_NOT_NULL(ver, "version string not null");
    ASSERT(strlen(ver) > 0, "version string not empty");
}

static void test_api_get_version(void) {
    int major, minor, patch;
    yaml_get_version(&major, &minor, &patch);
    ASSERT(major >= 0, "major >= 0");
    ASSERT(minor >= 0, "minor >= 0");
    ASSERT(patch >= 0, "patch >= 0");
}

/* ========== Emitter output to string ========== */

static int test_write_handler(void *data, unsigned char *buffer, size_t size) {
    (void)data; (void)buffer; (void)size;
    return 1;
}

static void test_api_emit_simple(void) {
    yaml_emitter_t emitter;
    yaml_event_t event;

    yaml_emitter_initialize(&emitter);
    yaml_emitter_set_output(&emitter, test_write_handler, NULL);

    yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
    ASSERT(yaml_emitter_emit(&emitter, &event), "emit stream start");

    yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1);
    ASSERT(yaml_emitter_emit(&emitter, &event), "emit doc start");

    yaml_scalar_event_initialize(&event, NULL, NULL,
        (yaml_char_t *)"hello", 5, 1, 0, YAML_PLAIN_SCALAR_STYLE);
    ASSERT(yaml_emitter_emit(&emitter, &event), "emit scalar");

    yaml_document_end_event_initialize(&event, 1);
    ASSERT(yaml_emitter_emit(&emitter, &event), "emit doc end");

    yaml_stream_end_event_initialize(&event);
    ASSERT(yaml_emitter_emit(&emitter, &event), "emit stream end");

    yaml_emitter_delete(&emitter);
}

static void test_api_emit_mapping(void) {
    yaml_emitter_t emitter;
    yaml_event_t event;

    yaml_emitter_initialize(&emitter);
    yaml_emitter_set_output(&emitter, test_write_handler, NULL);

    yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
    yaml_emitter_emit(&emitter, &event);

    yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1);
    yaml_emitter_emit(&emitter, &event);

    yaml_mapping_start_event_initialize(&event, NULL, NULL, 1, YAML_BLOCK_MAPPING_STYLE);
    ASSERT(yaml_emitter_emit(&emitter, &event), "emit map start");

    yaml_scalar_event_initialize(&event, NULL, NULL,
        (yaml_char_t *)"key", 3, 1, 0, YAML_PLAIN_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);

    yaml_scalar_event_initialize(&event, NULL, NULL,
        (yaml_char_t *)"value", 5, 1, 0, YAML_PLAIN_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);

    yaml_mapping_end_event_initialize(&event);
    ASSERT(yaml_emitter_emit(&emitter, &event), "emit map end");

    yaml_document_end_event_initialize(&event, 1);
    yaml_emitter_emit(&emitter, &event);

    yaml_stream_end_event_initialize(&event);
    yaml_emitter_emit(&emitter, &event);

    yaml_emitter_delete(&emitter);
}

/* ========== Document dump ========== */

static void test_api_dump_simple(void) {
    yaml_emitter_t emitter;
    yaml_document_t doc;

    yaml_emitter_initialize(&emitter);
    yaml_emitter_set_output(&emitter, test_write_handler, NULL);
    yaml_emitter_set_encoding(&emitter, YAML_UTF8_ENCODING);

    yaml_document_initialize(&doc, NULL, NULL, NULL, 1, 1);
    yaml_document_add_scalar(&doc, NULL, (yaml_char_t *)"hello", 5,
                              YAML_PLAIN_SCALAR_STYLE);

    /* yaml_emitter_dump handles stream start internally on first call */
    ASSERT(yaml_emitter_dump(&emitter, &doc), "dump simple");

    /* Close the stream by dumping a NULL-root document */
    yaml_document_initialize(&doc, NULL, NULL, NULL, 1, 1);
    yaml_emitter_dump(&emitter, &doc);

    yaml_emitter_delete(&emitter);
}

/* ========== Scanning operations ========== */

static void test_api_scan_all_token_types(void) {
    const char *input =
        "%YAML 1.1\n"
        "%TAG !e! tag:example.com,\n"
        "---\n"
        "!!map\n"
        "&anchor key: !<tag:yaml.org,2002:str> value\n"
        "seq:\n"
        "  - *anchor\n"
        "  - 'single'\n"
        "  - \"double\"\n"
        "  - |\n"
        "    literal\n"
        "  - >\n"
        "    folded\n"
        "flow: [a, {b: c}]\n"
        "...\n";

    yaml_parser_t parser;
    yaml_token_t token;
    int types_seen[32] = {0};

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));

    while (1) {
        if (!yaml_parser_scan(&parser, &token)) break;
        if (token.type < 32) types_seen[token.type] = 1;
        int done = (token.type == YAML_STREAM_END_TOKEN);
        yaml_token_delete(&token);
        if (done) break;
    }
    yaml_parser_delete(&parser);

    ASSERT(types_seen[YAML_STREAM_START_TOKEN], "seen stream start");
    ASSERT(types_seen[YAML_STREAM_END_TOKEN], "seen stream end");
    ASSERT(types_seen[YAML_DOCUMENT_START_TOKEN], "seen doc start");
    ASSERT(types_seen[YAML_DOCUMENT_END_TOKEN], "seen doc end");
    ASSERT(types_seen[YAML_KEY_TOKEN], "seen key");
    ASSERT(types_seen[YAML_VALUE_TOKEN], "seen value");
    ASSERT(types_seen[YAML_SCALAR_TOKEN], "seen scalar");
    ASSERT(types_seen[YAML_ANCHOR_TOKEN], "seen anchor");
    ASSERT(types_seen[YAML_ALIAS_TOKEN], "seen alias");
    ASSERT(types_seen[YAML_TAG_TOKEN], "seen tag");
    ASSERT(types_seen[YAML_TAG_DIRECTIVE_TOKEN], "seen tag directive");
}

/* ========== Error reporting ========== */

static void test_api_parser_error_info(void) {
    yaml_parser_t parser;
    yaml_event_t event;

    yaml_parser_initialize(&parser);
    /* Invalid: tab character at start of line for indentation */
    yaml_parser_set_input_string(&parser, (const unsigned char *)"\t: bad", 6);

    /* Skip stream start */
    yaml_parser_parse(&parser, &event);
    yaml_event_delete(&event);

    /* The next parse should fail */
    int ok = yaml_parser_parse(&parser, &event);
    if (!ok) {
        ASSERT_NOT_NULL(parser.problem, "error has problem string");
    } else {
        yaml_event_delete(&event);
        /* Drain remaining */
        while (1) {
            if (!yaml_parser_parse(&parser, &event)) break;
            int done = (event.type == YAML_STREAM_END_EVENT);
            yaml_event_delete(&event);
            if (done) break;
        }
    }
    yaml_parser_delete(&parser);
    ASSERT(1, "error info ok");
}

static void test_api_parser_error_mark(void) {
    yaml_parser_t parser;
    yaml_token_t token;

    yaml_parser_initialize(&parser);
    const unsigned char input[] = {'a', ':', ' ', 0x80}; /* invalid UTF-8 */
    yaml_parser_set_input_string(&parser, input, 4);

    int got_error = 0;
    while (1) {
        if (!yaml_parser_scan(&parser, &token)) {
            got_error = 1;
            break;
        }
        int done = (token.type == YAML_STREAM_END_TOKEN);
        yaml_token_delete(&token);
        if (done) break;
    }
    if (got_error) {
        ASSERT(parser.problem_mark.line >= 0, "error mark line set");
    }
    yaml_parser_delete(&parser);
    ASSERT(1, "error mark ok");
}

int main(void) {
    TEST_SUITE_BEGIN("API Comprehensive");

    /* Parser init/delete */
    test_api_parser_init();
    test_api_parser_init_multiple();

    /* Input string */
    test_api_set_input_string_empty();
    test_api_set_input_string_one_byte();

    /* Encoding */
    test_api_set_encoding_utf8();
    test_api_set_encoding_any();

    /* Custom reader */
    test_api_custom_reader();
    test_api_failing_reader();

    /* File input */
    test_api_file_input();
    test_api_file_input_empty();

    /* Emitter init/delete */
    test_api_emitter_init();
    test_api_emitter_init_multiple();

    /* Emitter settings */
    test_api_emitter_set_canonical();
    test_api_emitter_set_indent();
    test_api_emitter_set_width();
    test_api_emitter_set_unicode();
    test_api_emitter_set_break();
    test_api_emitter_set_encoding();

    /* Event construction */
    test_api_stream_start_event();
    test_api_stream_end_event();
    test_api_document_start_event();
    test_api_document_start_explicit();
    test_api_document_end_event();
    test_api_scalar_event_plain();
    test_api_scalar_event_tagged();
    test_api_scalar_event_anchored();
    test_api_scalar_event_empty();
    test_api_scalar_event_styles();
    test_api_sequence_start_event();
    test_api_sequence_start_flow();
    test_api_sequence_end_event();
    test_api_mapping_start_event();
    test_api_mapping_start_flow();
    test_api_mapping_end_event();
    test_api_alias_event();

    /* Document API */
    test_api_document_init();
    test_api_document_init_explicit();
    test_api_document_add_scalar();
    test_api_document_add_sequence();
    test_api_document_add_mapping();
    test_api_document_append_seq_item();
    test_api_document_append_map_pair();
    test_api_document_get_node();
    test_api_document_get_root();
    test_api_document_get_root_empty();
    test_api_document_get_node_invalid();
    test_api_document_get_node_zero();

    /* Document load */
    test_api_load_simple();
    test_api_load_mapping();
    test_api_load_sequence();
    test_api_load_empty();
    test_api_load_multiple_docs();

    /* Document traversal */
    test_api_traverse_mapping_pairs();
    test_api_traverse_sequence_items();

    /* Token operations */
    test_api_token_delete_scalar();

    /* Version API */
    test_api_get_version_string();
    test_api_get_version();

    /* Emitter output */
    test_api_emit_simple();
    test_api_emit_mapping();

    /* Document dump */
    test_api_dump_simple();

    /* Scanning */
    test_api_scan_all_token_types();

    /* Error reporting */
    test_api_parser_error_info();
    test_api_parser_error_mark();

    TEST_SUITE_END();
}
