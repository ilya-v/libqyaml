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

/* libyaml function pointers */
static fn_parser_initialize  ref_parser_initialize;
static fn_parser_delete      ref_parser_delete;
static fn_parser_set_input_string ref_parser_set_input_string;
static fn_parser_parse       ref_parser_parse;
static fn_event_delete       ref_event_delete;

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

    if (!ref_parser_initialize || !ref_parser_delete ||
        !ref_parser_set_input_string || !ref_parser_parse ||
        !ref_event_delete) {
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

/* Helper macro for differential tests */
#define DIFF_TEST(name, input) do { \
    ASSERT(compare_event_streams(name, input, strlen(input)), \
           name ": event streams match libyaml"); \
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
    ASSERT(compare_event_streams("utf8_bom", input, sizeof(input) - 1),
           "utf8_bom: event streams match libyaml");
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

    unload_libyaml();

    TEST_SUITE_END();
}
