/*
 * True differential testing: compare libqyaml against system libyaml.
 *
 * Uses dlopen to load /lib64/libyaml-0.so.2 at runtime and compares
 * the event streams produced by both parsers on identical inputs.
 * Any divergence is a bug in libqyaml.
 */

#include "test_helper.h"
#include <dlfcn.h>

/* Function pointer types matching libyaml API */
typedef int (*fn_parser_initialize)(yaml_parser_t *);
typedef void (*fn_parser_delete)(yaml_parser_t *);
typedef void (*fn_parser_set_input_string)(yaml_parser_t *,
    const unsigned char *, size_t);
typedef int (*fn_parser_parse)(yaml_parser_t *, yaml_event_t *);
typedef void (*fn_event_delete)(yaml_event_t *);
typedef int (*fn_parser_scan)(yaml_parser_t *, yaml_token_t *);
typedef void (*fn_token_delete)(yaml_token_t *);
typedef int (*fn_parser_load)(yaml_parser_t *, yaml_document_t *);
typedef void (*fn_document_delete)(yaml_document_t *);
typedef yaml_node_t *(*fn_document_get_root_node)(yaml_document_t *);
typedef yaml_node_t *(*fn_document_get_node)(yaml_document_t *, int);

/* libyaml function pointers */
static fn_parser_initialize  ref_parser_initialize;
static fn_parser_delete      ref_parser_delete;
static fn_parser_set_input_string ref_parser_set_input_string;
static fn_parser_parse       ref_parser_parse;
static fn_event_delete       ref_event_delete;
static fn_parser_scan        ref_parser_scan;
static fn_token_delete       ref_token_delete;
static fn_parser_load        ref_parser_load;
static fn_document_delete    ref_document_delete;
static fn_document_get_root_node ref_document_get_root_node;
static fn_document_get_node  ref_document_get_node;

static void *libyaml_handle = NULL;

static int load_libyaml(void) {
    libyaml_handle = dlopen("libyaml-0.so.2", RTLD_LAZY);
    if (!libyaml_handle) {
        fprintf(stderr, "  SKIP: cannot load libyaml: %s\n", dlerror());
        return 0;
    }

    ref_parser_initialize = (fn_parser_initialize)dlsym(libyaml_handle,
        "yaml_parser_initialize");
    ref_parser_delete = (fn_parser_delete)dlsym(libyaml_handle,
        "yaml_parser_delete");
    ref_parser_set_input_string = (fn_parser_set_input_string)dlsym(libyaml_handle,
        "yaml_parser_set_input_string");
    ref_parser_parse = (fn_parser_parse)dlsym(libyaml_handle,
        "yaml_parser_parse");
    ref_event_delete = (fn_event_delete)dlsym(libyaml_handle,
        "yaml_event_delete");

    ref_parser_scan = (fn_parser_scan)dlsym(libyaml_handle,
        "yaml_parser_scan");
    ref_token_delete = (fn_token_delete)dlsym(libyaml_handle,
        "yaml_token_delete");
    ref_parser_load = (fn_parser_load)dlsym(libyaml_handle,
        "yaml_parser_load");
    ref_document_delete = (fn_document_delete)dlsym(libyaml_handle,
        "yaml_document_delete");
    ref_document_get_root_node = (fn_document_get_root_node)dlsym(libyaml_handle,
        "yaml_document_get_root_node");
    ref_document_get_node = (fn_document_get_node)dlsym(libyaml_handle,
        "yaml_document_get_node");

    if (!ref_parser_initialize || !ref_parser_delete ||
        !ref_parser_set_input_string || !ref_parser_parse ||
        !ref_event_delete || !ref_parser_scan || !ref_token_delete ||
        !ref_parser_load || !ref_document_delete ||
        !ref_document_get_root_node || !ref_document_get_node) {
        fprintf(stderr, "  SKIP: missing libyaml symbols: %s\n", dlerror());
        dlclose(libyaml_handle);
        libyaml_handle = NULL;
        return 0;
    }

    return 1;
}

static void unload_libyaml(void) {
    if (libyaml_handle) {
        dlclose(libyaml_handle);
        libyaml_handle = NULL;
    }
}

/*
 * Compare event streams from both parsers.
 * Returns 1 if identical, 0 if divergent.
 */
static int compare_event_streams(const char *name, const char *input, size_t len) {
    yaml_parser_t our_parser, ref_parser;
    yaml_event_t our_event, ref_event;
    int event_idx = 0;
    int result = 1;

    if (!yaml_parser_initialize(&our_parser)) {
        fprintf(stderr, "  %s: libqyaml parser init failed\n", name);
        return 0;
    }
    if (!ref_parser_initialize(&ref_parser)) {
        fprintf(stderr, "  %s: libyaml parser init failed\n", name);
        yaml_parser_delete(&our_parser);
        return 0;
    }

    yaml_parser_set_input_string(&our_parser, (const unsigned char *)input, len);
    ref_parser_set_input_string(&ref_parser, (const unsigned char *)input, len);

    while (1) {
        int our_ok = yaml_parser_parse(&our_parser, &our_event);
        int ref_ok = ref_parser_parse(&ref_parser, &ref_event);

        /* Both should succeed or both should fail */
        if (our_ok != ref_ok) {
            fprintf(stderr, "  %s[%d]: parse status divergence "
                    "(ours=%d, ref=%d)\n", name, event_idx, our_ok, ref_ok);
            if (our_ok) yaml_event_delete(&our_event);
            if (ref_ok) ref_event_delete(&ref_event);
            result = 0;
            break;
        }

        if (!our_ok) {
            /* Both failed -- that's consistent */
            break;
        }

        /* Compare event types */
        if (our_event.type != ref_event.type) {
            fprintf(stderr, "  %s[%d]: event type divergence "
                    "(ours=%d, ref=%d)\n", name, event_idx,
                    our_event.type, ref_event.type);
            yaml_event_delete(&our_event);
            ref_event_delete(&ref_event);
            result = 0;
            break;
        }

        /* Compare scalar values */
        if (our_event.type == YAML_SCALAR_EVENT) {
            const char *our_val = (const char *)our_event.data.scalar.value;
            const char *ref_val = (const char *)ref_event.data.scalar.value;
            size_t our_len = our_event.data.scalar.length;
            size_t ref_len = ref_event.data.scalar.length;

            if (our_len != ref_len ||
                memcmp(our_val, ref_val, our_len) != 0) {
                fprintf(stderr, "  %s[%d]: scalar value divergence "
                        "(ours='%.*s' len=%zu, ref='%.*s' len=%zu)\n",
                        name, event_idx,
                        (int)(our_len < 50 ? our_len : 50), our_val, our_len,
                        (int)(ref_len < 50 ? ref_len : 50), ref_val, ref_len);
                yaml_event_delete(&our_event);
                ref_event_delete(&ref_event);
                result = 0;
                break;
            }

            /* Compare anchor */
            const char *our_anc = (const char *)our_event.data.scalar.anchor;
            const char *ref_anc = (const char *)ref_event.data.scalar.anchor;
            if ((our_anc == NULL) != (ref_anc == NULL) ||
                (our_anc && ref_anc && strcmp(our_anc, ref_anc) != 0)) {
                fprintf(stderr, "  %s[%d]: scalar anchor divergence\n",
                        name, event_idx);
                yaml_event_delete(&our_event);
                ref_event_delete(&ref_event);
                result = 0;
                break;
            }

            /* Compare tag */
            const char *our_tag = (const char *)our_event.data.scalar.tag;
            const char *ref_tag = (const char *)ref_event.data.scalar.tag;
            if ((our_tag == NULL) != (ref_tag == NULL) ||
                (our_tag && ref_tag && strcmp(our_tag, ref_tag) != 0)) {
                fprintf(stderr, "  %s[%d]: scalar tag divergence "
                        "(ours='%s', ref='%s')\n", name, event_idx,
                        our_tag ? our_tag : "(null)",
                        ref_tag ? ref_tag : "(null)");
                yaml_event_delete(&our_event);
                ref_event_delete(&ref_event);
                result = 0;
                break;
            }
        }

        /* Compare alias anchor */
        if (our_event.type == YAML_ALIAS_EVENT) {
            const char *our_anc = (const char *)our_event.data.alias.anchor;
            const char *ref_anc = (const char *)ref_event.data.alias.anchor;
            if ((our_anc == NULL) != (ref_anc == NULL) ||
                (our_anc && ref_anc && strcmp(our_anc, ref_anc) != 0)) {
                fprintf(stderr, "  %s[%d]: alias anchor divergence\n",
                        name, event_idx);
                yaml_event_delete(&our_event);
                ref_event_delete(&ref_event);
                result = 0;
                break;
            }
        }

        /* Compare sequence/mapping anchors and tags */
        if (our_event.type == YAML_SEQUENCE_START_EVENT) {
            const char *our_anc = (const char *)our_event.data.sequence_start.anchor;
            const char *ref_anc = (const char *)ref_event.data.sequence_start.anchor;
            if ((our_anc == NULL) != (ref_anc == NULL) ||
                (our_anc && ref_anc && strcmp(our_anc, ref_anc) != 0)) {
                fprintf(stderr, "  %s[%d]: sequence anchor divergence\n",
                        name, event_idx);
                yaml_event_delete(&our_event);
                ref_event_delete(&ref_event);
                result = 0;
                break;
            }
        }

        if (our_event.type == YAML_MAPPING_START_EVENT) {
            const char *our_anc = (const char *)our_event.data.mapping_start.anchor;
            const char *ref_anc = (const char *)ref_event.data.mapping_start.anchor;
            if ((our_anc == NULL) != (ref_anc == NULL) ||
                (our_anc && ref_anc && strcmp(our_anc, ref_anc) != 0)) {
                fprintf(stderr, "  %s[%d]: mapping anchor divergence\n",
                        name, event_idx);
                yaml_event_delete(&our_event);
                ref_event_delete(&ref_event);
                result = 0;
                break;
            }
        }

        int done = (our_event.type == YAML_STREAM_END_EVENT);
        yaml_event_delete(&our_event);
        ref_event_delete(&ref_event);
        event_idx++;

        if (done) break;
    }

    yaml_parser_delete(&our_parser);
    ref_parser_delete(&ref_parser);
    return result;
}

/*
 * Compare scanner token streams from both parsers.
 * Returns 1 if identical, 0 if divergent.
 */
static int compare_token_streams(const char *name, const char *input, size_t len) {
    yaml_parser_t our_parser, ref_parser;
    yaml_token_t our_token, ref_token;
    int token_idx = 0;
    int result = 1;

    if (!yaml_parser_initialize(&our_parser)) {
        fprintf(stderr, "  %s: libqyaml parser init failed (scan)\n", name);
        return 0;
    }
    if (!ref_parser_initialize(&ref_parser)) {
        fprintf(stderr, "  %s: libyaml parser init failed (scan)\n", name);
        yaml_parser_delete(&our_parser);
        return 0;
    }

    yaml_parser_set_input_string(&our_parser, (const unsigned char *)input, len);
    ref_parser_set_input_string(&ref_parser, (const unsigned char *)input, len);

    while (1) {
        int our_ok = yaml_parser_scan(&our_parser, &our_token);
        int ref_ok = ref_parser_scan(&ref_parser, &ref_token);

        if (our_ok != ref_ok) {
            fprintf(stderr, "  %s[tok %d]: scan status divergence "
                    "(ours=%d, ref=%d)\n", name, token_idx, our_ok, ref_ok);
            if (our_ok) yaml_token_delete(&our_token);
            if (ref_ok) ref_token_delete(&ref_token);
            result = 0;
            break;
        }

        if (!our_ok) break;

        if (our_token.type != ref_token.type) {
            fprintf(stderr, "  %s[tok %d]: token type divergence "
                    "(ours=%d, ref=%d)\n", name, token_idx,
                    our_token.type, ref_token.type);
            yaml_token_delete(&our_token);
            ref_token_delete(&ref_token);
            result = 0;
            break;
        }

        /* Compare scalar token values */
        if (our_token.type == YAML_SCALAR_TOKEN) {
            const char *our_val = (const char *)our_token.data.scalar.value;
            const char *ref_val = (const char *)ref_token.data.scalar.value;
            size_t our_len = our_token.data.scalar.length;
            size_t ref_len = ref_token.data.scalar.length;

            if (our_len != ref_len || memcmp(our_val, ref_val, our_len) != 0) {
                fprintf(stderr, "  %s[tok %d]: scalar token value divergence "
                        "(ours='%.*s' len=%zu, ref='%.*s' len=%zu)\n",
                        name, token_idx,
                        (int)(our_len < 50 ? our_len : 50), our_val, our_len,
                        (int)(ref_len < 50 ? ref_len : 50), ref_val, ref_len);
                yaml_token_delete(&our_token);
                ref_token_delete(&ref_token);
                result = 0;
                break;
            }
        }

        /* Compare tag token values */
        if (our_token.type == YAML_TAG_TOKEN) {
            const char *our_handle = (const char *)our_token.data.tag.handle;
            const char *ref_handle = (const char *)ref_token.data.tag.handle;
            const char *our_suffix = (const char *)our_token.data.tag.suffix;
            const char *ref_suffix = (const char *)ref_token.data.tag.suffix;

            if ((our_handle == NULL) != (ref_handle == NULL) ||
                (our_handle && ref_handle && strcmp(our_handle, ref_handle) != 0)) {
                fprintf(stderr, "  %s[tok %d]: tag handle divergence\n", name, token_idx);
                yaml_token_delete(&our_token);
                ref_token_delete(&ref_token);
                result = 0;
                break;
            }
            if ((our_suffix == NULL) != (ref_suffix == NULL) ||
                (our_suffix && ref_suffix && strcmp(our_suffix, ref_suffix) != 0)) {
                fprintf(stderr, "  %s[tok %d]: tag suffix divergence\n", name, token_idx);
                yaml_token_delete(&our_token);
                ref_token_delete(&ref_token);
                result = 0;
                break;
            }
        }

        /* Compare anchor/alias token values */
        if (our_token.type == YAML_ANCHOR_TOKEN || our_token.type == YAML_ALIAS_TOKEN) {
            const char *our_val = (const char *)our_token.data.anchor.value;
            const char *ref_val = (const char *)ref_token.data.anchor.value;
            if ((our_val == NULL) != (ref_val == NULL) ||
                (our_val && ref_val && strcmp(our_val, ref_val) != 0)) {
                fprintf(stderr, "  %s[tok %d]: anchor/alias value divergence\n",
                        name, token_idx);
                yaml_token_delete(&our_token);
                ref_token_delete(&ref_token);
                result = 0;
                break;
            }
        }

        int done = (our_token.type == YAML_STREAM_END_TOKEN);
        yaml_token_delete(&our_token);
        ref_token_delete(&ref_token);
        token_idx++;

        if (done) break;
    }

    yaml_parser_delete(&our_parser);
    ref_parser_delete(&ref_parser);
    return result;
}

/*
 * Recursively compare document nodes.
 * Returns 1 if identical, 0 if divergent.
 */
static int compare_nodes(const char *name, int node_id,
                          yaml_document_t *our_doc, yaml_document_t *ref_doc,
                          int depth) {
    if (depth > 100) {
        fprintf(stderr, "  %s: recursion depth exceeded at node %d\n", name, node_id);
        return 0;
    }

    yaml_node_t *our_node = yaml_document_get_node(our_doc, node_id);
    yaml_node_t *ref_node = ref_document_get_node(ref_doc, node_id);

    if ((our_node == NULL) != (ref_node == NULL)) {
        fprintf(stderr, "  %s: node %d existence divergence (ours=%s, ref=%s)\n",
                name, node_id,
                our_node ? "exists" : "null",
                ref_node ? "exists" : "null");
        return 0;
    }
    if (!our_node) return 1;

    if (our_node->type != ref_node->type) {
        fprintf(stderr, "  %s: node %d type divergence (ours=%d, ref=%d)\n",
                name, node_id, our_node->type, ref_node->type);
        return 0;
    }

    /* Compare tags */
    const char *our_tag = (const char *)our_node->tag;
    const char *ref_tag = (const char *)ref_node->tag;
    if ((our_tag == NULL) != (ref_tag == NULL) ||
        (our_tag && ref_tag && strcmp(our_tag, ref_tag) != 0)) {
        fprintf(stderr, "  %s: node %d tag divergence (ours='%s', ref='%s')\n",
                name, node_id,
                our_tag ? our_tag : "(null)",
                ref_tag ? ref_tag : "(null)");
        return 0;
    }

    switch (our_node->type) {
    case YAML_SCALAR_NODE: {
        size_t our_len = our_node->data.scalar.length;
        size_t ref_len = ref_node->data.scalar.length;
        const char *our_val = (const char *)our_node->data.scalar.value;
        const char *ref_val = (const char *)ref_node->data.scalar.value;

        if (our_len != ref_len || memcmp(our_val, ref_val, our_len) != 0) {
            fprintf(stderr, "  %s: node %d scalar value divergence "
                    "(ours='%.*s' len=%zu, ref='%.*s' len=%zu)\n",
                    name, node_id,
                    (int)(our_len < 50 ? our_len : 50), our_val, our_len,
                    (int)(ref_len < 50 ? ref_len : 50), ref_val, ref_len);
            return 0;
        }
        if (our_node->data.scalar.style != ref_node->data.scalar.style) {
            fprintf(stderr, "  %s: node %d scalar style divergence "
                    "(ours=%d, ref=%d)\n",
                    name, node_id,
                    our_node->data.scalar.style, ref_node->data.scalar.style);
            return 0;
        }
        break;
    }
    case YAML_SEQUENCE_NODE: {
        int our_count = our_node->data.sequence.items.top -
                        our_node->data.sequence.items.start;
        int ref_count = ref_node->data.sequence.items.top -
                        ref_node->data.sequence.items.start;
        if (our_count != ref_count) {
            fprintf(stderr, "  %s: node %d sequence length divergence "
                    "(ours=%d, ref=%d)\n",
                    name, node_id, our_count, ref_count);
            return 0;
        }
        for (int i = 0; i < our_count; i++) {
            int our_child = our_node->data.sequence.items.start[i];
            int ref_child = ref_node->data.sequence.items.start[i];
            if (our_child != ref_child) {
                fprintf(stderr, "  %s: node %d seq[%d] child id divergence "
                        "(ours=%d, ref=%d)\n",
                        name, node_id, i, our_child, ref_child);
                return 0;
            }
            if (!compare_nodes(name, our_child, our_doc, ref_doc, depth + 1))
                return 0;
        }
        break;
    }
    case YAML_MAPPING_NODE: {
        int our_count = our_node->data.mapping.pairs.top -
                        our_node->data.mapping.pairs.start;
        int ref_count = ref_node->data.mapping.pairs.top -
                        ref_node->data.mapping.pairs.start;
        if (our_count != ref_count) {
            fprintf(stderr, "  %s: node %d mapping pair count divergence "
                    "(ours=%d, ref=%d)\n",
                    name, node_id, our_count, ref_count);
            return 0;
        }
        for (int i = 0; i < our_count; i++) {
            int our_key = our_node->data.mapping.pairs.start[i].key;
            int ref_key = ref_node->data.mapping.pairs.start[i].key;
            int our_val_id = our_node->data.mapping.pairs.start[i].value;
            int ref_val_id = ref_node->data.mapping.pairs.start[i].value;
            if (our_key != ref_key || our_val_id != ref_val_id) {
                fprintf(stderr, "  %s: node %d pair[%d] id divergence\n",
                        name, node_id, i);
                return 0;
            }
            if (!compare_nodes(name, our_key, our_doc, ref_doc, depth + 1))
                return 0;
            if (!compare_nodes(name, our_val_id, our_doc, ref_doc, depth + 1))
                return 0;
        }
        break;
    }
    default:
        break;
    }

    return 1;
}

/*
 * Compare loaded documents from both parsers.
 * Returns 1 if identical, 0 if divergent.
 */
static int compare_documents(const char *name, const char *input, size_t len) {
    yaml_parser_t our_parser, ref_parser;
    yaml_document_t our_doc, ref_doc;
    int doc_idx = 0;
    int result = 1;

    if (!yaml_parser_initialize(&our_parser)) {
        fprintf(stderr, "  %s: libqyaml parser init failed (load)\n", name);
        return 0;
    }
    if (!ref_parser_initialize(&ref_parser)) {
        fprintf(stderr, "  %s: libyaml parser init failed (load)\n", name);
        yaml_parser_delete(&our_parser);
        return 0;
    }

    yaml_parser_set_input_string(&our_parser, (const unsigned char *)input, len);
    ref_parser_set_input_string(&ref_parser, (const unsigned char *)input, len);

    while (1) {
        int our_ok = yaml_parser_load(&our_parser, &our_doc);
        int ref_ok = ref_parser_load(&ref_parser, &ref_doc);

        if (our_ok != ref_ok) {
            fprintf(stderr, "  %s[doc %d]: load status divergence "
                    "(ours=%d, ref=%d)\n", name, doc_idx, our_ok, ref_ok);
            if (our_ok) yaml_document_delete(&our_doc);
            if (ref_ok) ref_document_delete(&ref_doc);
            result = 0;
            break;
        }

        if (!our_ok) break;

        yaml_node_t *our_root = yaml_document_get_root_node(&our_doc);
        yaml_node_t *ref_root = ref_document_get_root_node(&ref_doc);

        if ((our_root == NULL) != (ref_root == NULL)) {
            fprintf(stderr, "  %s[doc %d]: root existence divergence\n",
                    name, doc_idx);
            yaml_document_delete(&our_doc);
            ref_document_delete(&ref_doc);
            result = 0;
            break;
        }

        if (!our_root) {
            yaml_document_delete(&our_doc);
            ref_document_delete(&ref_doc);
            break;
        }

        if (!compare_nodes(name, 1, &our_doc, &ref_doc, 0)) {
            yaml_document_delete(&our_doc);
            ref_document_delete(&ref_doc);
            result = 0;
            break;
        }

        yaml_document_delete(&our_doc);
        ref_document_delete(&ref_doc);
        doc_idx++;
    }

    yaml_parser_delete(&our_parser);
    ref_parser_delete(&ref_parser);
    return result;
}

/* Helper macro for differential tests -- tests all three API levels */
#define DIFF_TEST(name, input) do { \
    ASSERT(compare_token_streams(name, input, strlen(input)), \
           name ": token streams match libyaml"); \
    ASSERT(compare_event_streams(name, input, strlen(input)), \
           name ": event streams match libyaml"); \
    ASSERT(compare_documents(name, input, strlen(input)), \
           name ": loaded documents match libyaml"); \
} while(0)

/* ================================================================
 * Test cases -- a broad selection of YAML inputs
 * ================================================================ */

static void test_diff_empty(void) {
    DIFF_TEST("empty", "");
}

static void test_diff_single_scalar(void) {
    DIFF_TEST("scalar", "hello");
}

static void test_diff_simple_mapping(void) {
    DIFF_TEST("mapping", "key: value\n");
}

static void test_diff_simple_sequence(void) {
    DIFF_TEST("sequence", "- a\n- b\n- c\n");
}

static void test_diff_flow_sequence(void) {
    DIFF_TEST("flow_seq", "[1, 2, 3]");
}

static void test_diff_flow_mapping(void) {
    DIFF_TEST("flow_map", "{a: 1, b: 2}");
}

static void test_diff_nested_mapping(void) {
    DIFF_TEST("nested_map", "outer:\n  inner: val\n");
}

static void test_diff_seq_in_mapping(void) {
    DIFF_TEST("seq_in_map", "items:\n  - a\n  - b\n");
}

static void test_diff_map_in_sequence(void) {
    DIFF_TEST("map_in_seq", "- key: val\n- key2: val2\n");
}

static void test_diff_explicit_doc(void) {
    DIFF_TEST("explicit_doc", "---\nhello\n...\n");
}

static void test_diff_multi_doc(void) {
    DIFF_TEST("multi_doc", "---\na\n---\nb\n...\n");
}

static void test_diff_anchor_alias(void) {
    DIFF_TEST("anchor_alias", "- &a hello\n- *a\n");
}

static void test_diff_tagged_scalar(void) {
    DIFF_TEST("tagged", "!!str hello");
}

static void test_diff_empty_value(void) {
    DIFF_TEST("empty_value", "key:\n");
}

static void test_diff_nested_flow(void) {
    DIFF_TEST("nested_flow", "[[1], [2]]");
}

static void test_diff_block_literal(void) {
    DIFF_TEST("literal", "|\n  line one\n  line two\n");
}

static void test_diff_block_folded(void) {
    DIFF_TEST("folded", ">\n  folded\n  text\n");
}

static void test_diff_single_quoted(void) {
    DIFF_TEST("single_quoted", "'hello world'");
}

static void test_diff_double_quoted(void) {
    DIFF_TEST("double_quoted", "\"hello\\nworld\"");
}

static void test_diff_escape_sequences(void) {
    DIFF_TEST("escapes", "\"\\t\\n\\r\\0\\x41\\u0041\"");
}

static void test_diff_unicode(void) {
    DIFF_TEST("unicode", "key: caf\xC3\xA9\n");
}

static void test_diff_multiline_plain(void) {
    DIFF_TEST("multiline_plain", "key:\n  word1\n  word2\n");
}

static void test_diff_complex_keys(void) {
    DIFF_TEST("complex_keys", "? key\n: value\n");
}

static void test_diff_version_directive(void) {
    DIFF_TEST("version", "%YAML 1.1\n---\nhello\n");
}

static void test_diff_tag_directive(void) {
    DIFF_TEST("tag_dir", "%TAG !e! tag:example.com,2000:\n---\n!e!foo bar\n");
}

static void test_diff_comments(void) {
    DIFF_TEST("comments", "# comment\nkey: val # inline\n");
}

static void test_diff_null_values(void) {
    DIFF_TEST("nulls", "a: ~\nb: null\nc:\n");
}

static void test_diff_boolean_values(void) {
    DIFF_TEST("booleans", "a: true\nb: false\nc: yes\nd: no\n");
}

static void test_diff_numeric_values(void) {
    DIFF_TEST("numbers", "a: 42\nb: 3.14\nc: 0xff\nd: 1.0e5\n");
}

static void test_diff_deeply_nested(void) {
    DIFF_TEST("deep", "a:\n  b:\n    c:\n      d:\n        e: val\n");
}

static void test_diff_mixed_collections(void) {
    DIFF_TEST("mixed",
        "users:\n"
        "  - name: alice\n"
        "    roles:\n"
        "      - admin\n"
        "      - user\n"
        "  - name: bob\n"
        "    roles:\n"
        "      - user\n");
}

static void test_diff_flow_in_block(void) {
    DIFF_TEST("flow_in_block",
        "mapping:\n"
        "  key1: [a, b, c]\n"
        "  key2: {x: 1, y: 2}\n");
}

static void test_diff_kubernetes(void) {
    DIFF_TEST("kubernetes",
        "apiVersion: v1\n"
        "kind: Service\n"
        "metadata:\n"
        "  name: my-service\n"
        "  labels:\n"
        "    app: web\n"
        "spec:\n"
        "  ports:\n"
        "    - port: 80\n"
        "      targetPort: 8080\n"
        "  selector:\n"
        "    app: web\n");
}

static void test_diff_docker_compose(void) {
    DIFF_TEST("docker_compose",
        "version: '3'\n"
        "services:\n"
        "  web:\n"
        "    image: nginx\n"
        "    ports:\n"
        "      - '80:80'\n"
        "    volumes:\n"
        "      - ./html:/usr/share/nginx/html\n");
}

static void test_diff_github_actions(void) {
    DIFF_TEST("github_actions",
        "name: CI\n"
        "on:\n"
        "  push:\n"
        "    branches: [main]\n"
        "jobs:\n"
        "  build:\n"
        "    runs-on: ubuntu-latest\n"
        "    steps:\n"
        "      - uses: actions/checkout@v2\n"
        "      - name: Build\n"
        "        run: make\n");
}

static void test_diff_anchors_complex(void) {
    DIFF_TEST("anchors_complex",
        "defaults: &defaults\n"
        "  timeout: 30\n"
        "  retries: 3\n"
        "production:\n"
        "  <<: *defaults\n"
        "  timeout: 60\n");
}

static void test_diff_multiline_strings(void) {
    DIFF_TEST("multiline",
        "literal: |\n"
        "  line 1\n"
        "  line 2\n"
        "folded: >\n"
        "  para 1\n"
        "  continues\n"
        "\n"
        "  para 2\n");
}

static void test_diff_special_chars(void) {
    DIFF_TEST("special", "\"colon: in value\"\n");
}

static void test_diff_empty_collections(void) {
    DIFF_TEST("empty_coll", "seq: []\nmap: {}\n");
}

static void test_diff_indentless_seq(void) {
    DIFF_TEST("indentless",
        "key:\n"
        "- item1\n"
        "- item2\n");
}

static void test_diff_block_scalar_strip(void) {
    DIFF_TEST("strip", "|-\n  text\n");
}

static void test_diff_block_scalar_keep(void) {
    DIFF_TEST("keep", "|+\n  text\n\n");
}

static void test_diff_flow_multiline(void) {
    DIFF_TEST("flow_multi",
        "[\n"
        "  a,\n"
        "  b,\n"
        "  c\n"
        "]\n");
}

static void test_diff_mapping_many_keys(void) {
    DIFF_TEST("many_keys",
        "k1: v1\nk2: v2\nk3: v3\nk4: v4\nk5: v5\n"
        "k6: v6\nk7: v7\nk8: v8\nk9: v9\nk10: v10\n");
}

static void test_diff_sequence_many_items(void) {
    DIFF_TEST("many_items",
        "- i1\n- i2\n- i3\n- i4\n- i5\n"
        "- i6\n- i7\n- i8\n- i9\n- i10\n");
}

static void test_diff_utf8_bom(void) {
    const char input[] = "\xEF\xBB\xBFhello\n";
    ASSERT(compare_token_streams("utf8_bom", input, sizeof(input) - 1),
           "utf8_bom: token streams match libyaml");
    ASSERT(compare_event_streams("utf8_bom", input, sizeof(input) - 1),
           "utf8_bom: event streams match libyaml");
    ASSERT(compare_documents("utf8_bom", input, sizeof(input) - 1),
           "utf8_bom: loaded documents match libyaml");
}

/* ================================================================
 * Optimization boundary tests
 *
 * These tests specifically target the batch-skip and batch-copy
 * optimizations in libqyaml's scanner and reader. The optimizations
 * process consecutive ASCII bytes in bulk, so the critical boundaries
 * are transitions between ASCII and multi-byte UTF-8, and edge
 * characters at the ASCII/non-ASCII border.
 * ================================================================ */

/* Test: ASCII-to-UTF8 transition in plain scalar values.
 * Exercises scanner.c batch-copy for plain scalars (line ~2963). */
static void test_diff_opt_plain_ascii_to_utf8(void) {
    /* ASCII prefix then 2-byte UTF-8 */
    DIFF_TEST("opt_plain_a2u_2byte",
        "key: abcdefghijklmnop\xC3\xA9rest\n");
    /* ASCII prefix then 3-byte UTF-8 (CJK) */
    DIFF_TEST("opt_plain_a2u_3byte",
        "key: abcdefghijklmnop\xE4\xB8\xADrest\n");
    /* ASCII prefix then 4-byte UTF-8 (emoji) */
    DIFF_TEST("opt_plain_a2u_4byte",
        "key: abcdefghijklmnop\xF0\x9F\x98\x80rest\n");
    /* Single ASCII char before UTF-8 (below batch threshold) */
    DIFF_TEST("opt_plain_short_then_utf8",
        "key: a\xC3\xA9\n");
    /* Exactly 4 ASCII chars (batch threshold) before UTF-8 */
    DIFF_TEST("opt_plain_threshold_utf8",
        "key: abcd\xC3\xA9\n");
    /* UTF-8 at start, ASCII after */
    DIFF_TEST("opt_plain_utf8_then_ascii",
        "key: \xC3\xA9" "abcdefghijklmnop\n");
    /* Alternating ASCII and UTF-8 */
    DIFF_TEST("opt_plain_alternating",
        "key: ab\xC3\xA9" "cd\xE4\xB8\xAD" "ef\xF0\x9F\x98\x80gh\n");
}

/* Test: ASCII-to-UTF8 transition in double-quoted strings.
 * Exercises scanner.c batch-copy for quoted strings (line ~3364). */
static void test_diff_opt_quoted_ascii_to_utf8(void) {
    DIFF_TEST("opt_dquote_a2u",
        "key: \"abcdefghijklmnop\xC3\xA9rest\"\n");
    DIFF_TEST("opt_squote_a2u",
        "key: 'abcdefghijklmnop\xC3\xA9rest'\n");
    /* Escape sequence followed by UTF-8 */
    DIFF_TEST("opt_dquote_escape_utf8",
        "key: \"abc\\ndef\xC3\xA9ghi\"\n");
    /* UTF-8 followed by escape sequence */
    DIFF_TEST("opt_dquote_utf8_escape",
        "key: \"abc\xC3\xA9" "def\\nghi\"\n");
    /* Long quoted with multiple UTF-8 islands */
    DIFF_TEST("opt_dquote_multi_utf8",
        "key: \"aaaa\xC3\xA9" "bbbb\xE4\xB8\xAD" "cccc\xF0\x9F\x98\x80" "dddd\"\n");
    /* Single quotes with UTF-8 and '' escape */
    DIFF_TEST("opt_squote_utf8_escape",
        "key: 'abc\xC3\xA9" "d''e\xE4\xB8\xAD" "f'\n");
}

/* Test: ASCII-to-UTF8 transition in flow scalars.
 * Exercises scanner.c batch-copy for flow plain scalars (line ~3644/3651). */
static void test_diff_opt_flow_ascii_to_utf8(void) {
    DIFF_TEST("opt_flow_a2u_seq",
        "[abcdefgh\xC3\xA9ijk, lmnop\xE4\xB8\xADqrs]\n");
    DIFF_TEST("opt_flow_a2u_map",
        "{abcdefgh\xC3\xA9ijk: lmnop\xE4\xB8\xADqrs}\n");
    /* Nested flow with UTF-8 */
    DIFF_TEST("opt_flow_nested_utf8",
        "{a: [b\xC3\xA9, c\xE4\xB8\xAD], d\xF0\x9F\x98\x80: e}\n");
    /* Flow scalar ending right after UTF-8 */
    DIFF_TEST("opt_flow_utf8_at_end",
        "[abc\xC3\xA9]\n");
    /* Flow indicators after UTF-8 */
    DIFF_TEST("opt_flow_utf8_before_comma",
        "[abc\xC3\xA9, def]\n");
}

/* Test: whitespace batch-skip with adjacent non-ASCII.
 * Exercises scanner.c whitespace batch-skip (line ~1943). */
static void test_diff_opt_whitespace_boundaries(void) {
    /* Many spaces before a UTF-8 value */
    DIFF_TEST("opt_ws_before_utf8",
        "key:                    \xC3\xA9value\n");
    /* Many spaces in indentation before UTF-8 */
    DIFF_TEST("opt_ws_indent_utf8",
        "parent:\n"
        "          \xC3\xA9" "child: value\n");
    /* Tab mixed with spaces (tabs are handled differently) */
    DIFF_TEST("opt_ws_tab_space_mix",
        "key: value\n"
        "key2:  \tvalue2\n");
    /* Long sequence of spaces */
    DIFF_TEST("opt_ws_long_spaces",
        "key:                                                            value\n");
}

/* Test: comment batch-skip with adjacent non-ASCII.
 * Exercises scanner.c comment batch-skip (line ~1973). */
static void test_diff_opt_comment_boundaries(void) {
    /* Comment with UTF-8 characters */
    DIFF_TEST("opt_comment_utf8",
        "key: value # This is a comment with \xC3\xA9\n"
        "key2: value2\n");
    /* Long ASCII comment then UTF-8 */
    DIFF_TEST("opt_comment_long_ascii_utf8",
        "key: value # aaaaaaaaaaaaaaaaaaaaaaaaa\xC3\xA9\n"
        "key2: value2\n");
    /* Comment with only UTF-8 */
    DIFF_TEST("opt_comment_only_utf8",
        "key: value #\xC3\xA9\xE4\xB8\xAD\xF0\x9F\x98\x80\n"
        "key2: value2\n");
    /* Multi-byte at comment start */
    DIFF_TEST("opt_comment_utf8_start",
        "# \xC3\xA9 heading\nkey: value\n");
}

/* Test: reader batch processing at UTF-8 boundaries.
 * Exercises reader.c batch-copy (line ~243). */
static void test_diff_opt_reader_utf8_boundaries(void) {
    /* 0x7E and 0x7F boundary (printable ASCII limit) */
    DIFF_TEST("opt_reader_7e",
        "key: abc~def\n");
    /* DEL (0x7F) in quoted string -- should be rejected by reader */

    /* Control chars that ARE allowed: TAB (0x09) */
    DIFF_TEST("opt_reader_tab",
        "key: \"abc\tdef\"\n");

    /* Long ASCII block to stress batch processing */
    DIFF_TEST("opt_reader_long_ascii",
        "key: abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ\n");

    /* Long ASCII then sudden UTF-8 multi-byte */
    DIFF_TEST("opt_reader_long_then_utf8",
        "key: abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "\xC3\xA9\xE4\xB8\xAD\xF0\x9F\x98\x80"
        "abcdefghijklmnopqrstuvwxyz\n");

    /* All 2-byte UTF-8 sequence (Latin Extended) */
    DIFF_TEST("opt_reader_all_2byte",
        "key: \xC3\xA0\xC3\xA1\xC3\xA2\xC3\xA3\xC3\xA4\xC3\xA5\xC3\xA6\xC3\xA7\n");

    /* All 3-byte UTF-8 sequence (CJK) */
    DIFF_TEST("opt_reader_all_3byte",
        "key: \xE4\xB8\x80\xE4\xBA\x8C\xE4\xB8\x89\xE5\x9B\x9B\xE4\xBA\x94\n");
}

/* Test: plain scalar special char boundaries.
 * '#' and ':' end batch-copy in plain scalars. */
static void test_diff_opt_plain_special_chars(void) {
    /* '#' after space ends plain scalar (comment) */
    DIFF_TEST("opt_plain_hash_comment",
        "key: abcdefghijklmnop #comment\n");
    /* '#' without preceding space (not a comment) */
    DIFF_TEST("opt_plain_hash_inline",
        "key: abcdefghijklmnop#notcomment\n");
    /* ':' followed by space ends mapping value */
    DIFF_TEST("opt_plain_colon_space",
        "key: abcdefghijklmnop: value\n");
    /* ':' without following space */
    DIFF_TEST("opt_plain_colon_inline",
        "key: abcdefghijklmnop:notkey\n");
    /* Mix of special chars */
    DIFF_TEST("opt_plain_special_mix",
        "key: abc#def:ghi jkl\n");
}

/* Test: block scalar (literal/folded) with UTF-8.
 * Block scalars read lines byte-by-byte but reader uses batch. */
static void test_diff_opt_block_scalar_utf8(void) {
    DIFF_TEST("opt_literal_utf8",
        "key: |\n"
        "  line one with \xC3\xA9\n"
        "  line two with \xE4\xB8\xAD\n"
        "  line three with \xF0\x9F\x98\x80\n");
    DIFF_TEST("opt_folded_utf8",
        "key: >\n"
        "  paragraph one \xC3\xA9\n"
        "  continues here\n"
        "\n"
        "  paragraph two \xE4\xB8\xAD\n");
    /* Long lines in block scalar to stress batch reader */
    DIFF_TEST("opt_literal_long_lines",
        "key: |\n"
        "  abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz0123456789\xC3\xA9\n"
        "  another long line here with plenty of ASCII characters\n");
}

/* Test: multiple documents with UTF-8 at boundaries. */
static void test_diff_opt_multi_doc_utf8(void) {
    DIFF_TEST("opt_multidoc_utf8",
        "---\n"
        "doc1: \xC3\xA9\n"
        "...\n"
        "---\n"
        "doc2: \xE4\xB8\xAD\n"
        "...\n");
    /* Directive with UTF-8 in value */
    DIFF_TEST("opt_directive_utf8_value",
        "%TAG !e! tag:example.com,2000:\n"
        "---\n"
        "key: \xC3\xA9value\n");
}

/* Test: anchors and aliases with UTF-8 values. */
static void test_diff_opt_anchor_utf8(void) {
    DIFF_TEST("opt_anchor_utf8_value",
        "- &ref \xC3\xA9hello\xE4\xB8\xAD\n"
        "- *ref\n");
    DIFF_TEST("opt_anchor_utf8_map",
        "defaults: &def\n"
        "  name: \xC3\xA9test\n"
        "prod:\n"
        "  <<: *def\n"
        "  extra: \xE4\xB8\xAD\n");
}

/* Test: very long values to ensure batch copy handles large chunks. */
static void test_diff_opt_long_values(void) {
    /* 256-byte plain scalar (pure ASCII) */
    DIFF_TEST("opt_long_plain_ascii",
        "key: "
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB"
        "CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC"
        "DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD\n");
    /* 256-byte double quoted (pure ASCII) */
    DIFF_TEST("opt_long_dquote_ascii",
        "key: \""
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB"
        "CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC"
        "DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD\"\n");
    /* Long value with UTF-8 scattered throughout */
    DIFF_TEST("opt_long_mixed_utf8",
        "key: "
        "AAAA\xC3\xA9" "BBBB\xE4\xB8\xAD" "CCCC\xF0\x9F\x98\x80"
        "DDDD\xC3\xA9" "EEEE\xE4\xB8\xAD" "FFFF\xF0\x9F\x98\x80"
        "GGGG\xC3\xA9" "HHHH\xE4\xB8\xAD" "IIII\xF0\x9F\x98\x80"
        "JJJJ\xC3\xA9" "KKKK\xE4\xB8\xAD" "LLLL\xF0\x9F\x98\x80\n");
}

int main(void) {
    TEST_SUITE_BEGIN("Differential (vs libyaml)");

    if (!load_libyaml()) {
        printf("SKIP: libyaml not available for differential testing\n");
        printf("Results: 0/0 passed (skipped)\n");
        return 0;
    }

    test_diff_empty();
    test_diff_single_scalar();
    test_diff_simple_mapping();
    test_diff_simple_sequence();
    test_diff_flow_sequence();
    test_diff_flow_mapping();
    test_diff_nested_mapping();
    test_diff_seq_in_mapping();
    test_diff_map_in_sequence();
    test_diff_explicit_doc();
    test_diff_multi_doc();
    test_diff_anchor_alias();
    test_diff_tagged_scalar();
    test_diff_empty_value();
    test_diff_nested_flow();
    test_diff_block_literal();
    test_diff_block_folded();
    test_diff_single_quoted();
    test_diff_double_quoted();
    test_diff_escape_sequences();
    test_diff_unicode();
    test_diff_multiline_plain();
    test_diff_complex_keys();
    test_diff_version_directive();
    test_diff_tag_directive();
    test_diff_comments();
    test_diff_null_values();
    test_diff_boolean_values();
    test_diff_numeric_values();
    test_diff_deeply_nested();
    test_diff_mixed_collections();
    test_diff_flow_in_block();
    test_diff_kubernetes();
    test_diff_docker_compose();
    test_diff_github_actions();
    test_diff_anchors_complex();
    test_diff_multiline_strings();
    test_diff_special_chars();
    test_diff_empty_collections();
    test_diff_indentless_seq();
    test_diff_block_scalar_strip();
    test_diff_block_scalar_keep();
    test_diff_flow_multiline();
    test_diff_mapping_many_keys();
    test_diff_sequence_many_items();
    test_diff_utf8_bom();

    /* Optimization boundary tests */
    test_diff_opt_plain_ascii_to_utf8();
    test_diff_opt_quoted_ascii_to_utf8();
    test_diff_opt_flow_ascii_to_utf8();
    test_diff_opt_whitespace_boundaries();
    test_diff_opt_comment_boundaries();
    test_diff_opt_reader_utf8_boundaries();
    test_diff_opt_plain_special_chars();
    test_diff_opt_block_scalar_utf8();
    test_diff_opt_multi_doc_utf8();
    test_diff_opt_anchor_utf8();
    test_diff_opt_long_values();

    unload_libyaml();

    TEST_SUITE_END();
}
