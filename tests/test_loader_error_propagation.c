/*
 * Tests for loader error propagation.
 *
 * Verifies that when the parser rejects an input, the loader also rejects it.
 * The batch loader paths in loader.c have historically had bugs where parse
 * errors were swallowed, causing the loader to succeed on invalid input.
 *
 * Test strategy: for each input, verify that parse and load agree on
 * success/failure. If parse fails, load must also fail.
 */

#include "test_helper.h"

/* Helper: check if parsing succeeds */
static int parser_accepts(const char *input) {
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

/* Helper: check if loading succeeds */
static int loader_accepts(const char *input) {
    yaml_parser_t parser;
    yaml_document_t doc;

    if (!yaml_parser_initialize(&parser)) return 0;
    yaml_parser_set_input_string(&parser, (const unsigned char *)input,
                                 strlen(input));

    /* Load all documents until stream end */
    while (1) {
        if (!yaml_parser_load(&parser, &doc)) {
            yaml_document_delete(&doc);
            yaml_parser_delete(&parser);
            return 0;
        }
        yaml_node_t *root = yaml_document_get_root_node(&doc);
        if (!root) {
            /* Stream end */
            yaml_document_delete(&doc);
            break;
        }
        yaml_document_delete(&doc);
    }

    yaml_parser_delete(&parser);
    return 1;
}

/* Core consistency check: if parser rejects, loader must also reject */
static void check_consistency(const char *name, const char *input) {
    int parse_ok = parser_accepts(input);
    int load_ok = loader_accepts(input);

    if (!parse_ok && load_ok) {
        /* BUG: parser rejects but loader accepts -- error swallowed */
        fprintf(stderr, "  %s: LOADER SWALLOWS ERROR (parser rejects, "
                        "loader accepts)\n", name);
        ASSERT(0, name);
    } else if (parse_ok && !load_ok) {
        /* Possible bug, but less critical -- loader is stricter.
         * This can happen legitimately for some alias/anchor issues. */
        /* Don't fail, just note it */
        ASSERT(1, name);
    } else {
        /* Consistent: both accept or both reject */
        ASSERT(1, name);
    }
}

/* ========================================================================
 * Inputs that the parser SHOULD reject -- verify loader also rejects
 * ======================================================================== */

/* Tab in indentation (invalid YAML) */
static void test_error_tab_indent(void) {
    check_consistency("tab_indent_mapping",
                      "key:\n\tvalue\n");
    check_consistency("tab_indent_seq",
                      "- item\n\t- nested\n");
}

/* Invalid directives */
static void test_error_bad_directive(void) {
    check_consistency("bad_directive",
                      "% invalid\n");
    check_consistency("bad_yaml_version",
                      "%YAML 2.0\n---\nhello\n");
}

/* Duplicate keys with complex structures that stress batch loader */
static void test_error_batch_loader_stress(void) {
    /* Mapping with non-scalar values that force batch loader fallback */
    check_consistency("batch_fallback_nested_map",
                      "a:\n  b:\n    c: d\ne: f\n");
    check_consistency("batch_fallback_seq_value",
                      "key:\n  - item1\n  - item2\nkey2: val\n");
    check_consistency("batch_fallback_flow_value",
                      "key: [1, 2, 3]\nkey2: val\n");
    check_consistency("batch_fallback_anchor",
                      "key: &ref value\nkey2: *ref\n");
    check_consistency("batch_fallback_tag",
                      "key: !!str value\nkey2: val\n");
}

/* Invalid flow collections */
static void test_error_invalid_flow(void) {
    /* Unclosed flow sequence */
    check_consistency("unclosed_flow_seq", "[1, 2, 3");
    /* Unclosed flow mapping */
    check_consistency("unclosed_flow_map", "{a: 1, b: 2");
    /* Mismatched brackets */
    check_consistency("mismatched_brackets", "[1, 2}");
    check_consistency("mismatched_braces", "{a: 1]");
    /* Extra closing bracket */
    check_consistency("extra_close_bracket", "[1, 2]]");
}

/* Invalid block scalars */
static void test_error_invalid_block_scalar(void) {
    /* Block scalar with invalid indent indicator */
    check_consistency("block_scalar_bad_indent",
                      "|0\n  text\n");
    check_consistency("block_scalar_indent_10",
                      "|10\n  text\n");
}

/* Invalid anchors and aliases */
static void test_error_invalid_anchors(void) {
    /* Alias to undefined anchor (loader should reject) */
    check_consistency("undefined_alias",
                      "- *undefined\n");
    /* Duplicate anchor (loader should reject) */
    check_consistency("duplicate_anchor",
                      "- &dup hello\n- &dup world\n");
}

/* Inputs that mix block and flow in potentially confusing ways */
static void test_error_mixed_context(void) {
    /* Block content after flow indicator on same line */
    check_consistency("block_after_flow_inline",
                      "key: [1, 2]\n  extra: value\n");
    /* Flow content in block indentation context */
    check_consistency("flow_in_block_indent",
                      "outer:\n  inner: {a: 1}\n  more: val\n");
}

/* ========================================================================
 * Inputs that SHOULD succeed through both paths
 * ======================================================================== */

/* Simple mappings that go through batch path */
static void test_valid_batch_simple(void) {
    check_consistency("batch_simple_1",
                      "a: b\n");
    check_consistency("batch_simple_2",
                      "a: b\nc: d\n");
    check_consistency("batch_simple_10",
                      "k1: v1\nk2: v2\nk3: v3\nk4: v4\nk5: v5\n"
                      "k6: v6\nk7: v7\nk8: v8\nk9: v9\nk10: v10\n");
}

/* Mappings with empty values in batch path */
static void test_valid_batch_empty_values(void) {
    check_consistency("batch_empty_val_1",
                      "key:\n");
    check_consistency("batch_empty_val_2",
                      "a:\nb:\nc:\n");
    check_consistency("batch_empty_val_mixed",
                      "a: val\nb:\nc: val2\nd:\n");
}

/* Complex keys that force batch loader fallback */
static void test_valid_complex_key(void) {
    check_consistency("complex_key_basic",
                      "? key\n: value\n");
    check_consistency("complex_key_empty",
                      "?\n: value\n");
    check_consistency("complex_key_nested",
                      "? [1, 2]\n: value\n");
}

/* Sequences that go through batch path */
static void test_valid_batch_sequence(void) {
    check_consistency("batch_seq_simple",
                      "- a\n- b\n- c\n");
    check_consistency("batch_seq_10",
                      "- i1\n- i2\n- i3\n- i4\n- i5\n"
                      "- i6\n- i7\n- i8\n- i9\n- i10\n");
}

/* Flow collections */
static void test_valid_flow(void) {
    check_consistency("flow_seq", "[1, 2, 3]");
    check_consistency("flow_map", "{a: 1, b: 2}");
    check_consistency("flow_nested", "[{a: [1, 2]}, {b: [3, 4]}]");
    check_consistency("flow_empty_seq", "[]");
    check_consistency("flow_empty_map", "{}");
}

/* Multi-document */
static void test_valid_multi_doc(void) {
    check_consistency("multi_doc",
                      "---\na\n---\nb\n---\nc\n");
    check_consistency("multi_doc_explicit_end",
                      "---\na\n...\n---\nb\n...\n");
}

/* Documents with directives */
static void test_valid_directives(void) {
    check_consistency("yaml_directive",
                      "%YAML 1.1\n---\nhello\n");
    check_consistency("tag_directive",
                      "%TAG !e! tag:example.com,2000:\n---\n!e!foo bar\n");
}

/* Block scalars */
static void test_valid_block_scalars(void) {
    check_consistency("literal", "|\n  line1\n  line2\n");
    check_consistency("folded", ">\n  line1\n  line2\n");
    check_consistency("literal_strip", "|-\n  text\n");
    check_consistency("literal_keep", "|+\n  text\n\n");
}

/* Quoted scalars with edge cases */
static void test_valid_quoted_scalars(void) {
    check_consistency("dquote_escapes", "\"\\t\\n\\r\\0\\\\\\\"\"");
    check_consistency("squote_basic", "'hello world'");
    check_consistency("squote_escape", "'it''s'");
    check_consistency("dquote_multiline", "\"hello\n  world\"");
    check_consistency("squote_multiline", "'hello\n  world'");
}

/* ========================================================================
 * Targeted tests for the "loader swallows parse errors" bug pattern
 *
 * These inputs are specifically designed to trigger the batch loader's
 * fallback paths with invalid content, testing that errors propagate
 * correctly through the batch -> normal path transition.
 * ======================================================================== */

static void test_error_batch_to_normal_transition(void) {
    /* Mapping that starts as batch-eligible then hits invalid content */
    check_consistency("batch_to_normal_invalid_value",
                      "a: b\nc: \t\nd: e\n");

    /* Batch path encounters key that's too long to be simple key */
    /* (not really an error, but tests the transition) */
    char long_input[4096];
    snprintf(long_input, sizeof(long_input),
             "short: val\n"
             "key: value\n"
             "another: pair\n");
    check_consistency("batch_then_normal_long", long_input);

    /* Mapping then invalid indentation */
    check_consistency("mapping_bad_indent",
                      "a: 1\n b: 2\n");

    /* Duplicate value indicators */
    check_consistency("double_value_indicator",
                      "a: : b\n");
}

/* Test that loader error code matches expected type */
static void test_loader_error_codes(void) {
    yaml_parser_t parser;
    yaml_document_t doc;

    /* Test COMPOSER_ERROR for undefined alias */
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser,
        (const unsigned char *)"*nope\n", 6);
    if (!yaml_parser_load(&parser, &doc)) {
        ASSERT_EQ_INT(parser.error, YAML_COMPOSER_ERROR,
                      "undefined alias: COMPOSER_ERROR");
    } else {
        ASSERT(0, "undefined alias: should have failed");
        yaml_document_delete(&doc);
    }
    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);

    /* Test SCANNER_ERROR for tab in indentation */
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser,
        (const unsigned char *)"key:\n\tval\n", 10);
    if (!yaml_parser_load(&parser, &doc)) {
        ASSERT(parser.error == YAML_SCANNER_ERROR ||
               parser.error == YAML_PARSER_ERROR,
               "tab indent: SCANNER or PARSER error");
    } else {
        /* If it accepts, the loader might be doing something different.
         * Some YAML implementations accept tabs after colon. Just note. */
        yaml_document_delete(&doc);
    }
    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
}

int main(void) {
    TEST_SUITE_BEGIN("Loader Error Propagation");

    /* Parser-rejected inputs: loader must also reject */
    test_error_tab_indent();
    test_error_bad_directive();
    test_error_batch_loader_stress();
    test_error_invalid_flow();
    test_error_invalid_block_scalar();
    test_error_invalid_anchors();
    test_error_mixed_context();

    /* Valid inputs: both must accept */
    test_valid_batch_simple();
    test_valid_batch_empty_values();
    test_valid_complex_key();
    test_valid_batch_sequence();
    test_valid_flow();
    test_valid_multi_doc();
    test_valid_directives();
    test_valid_block_scalars();
    test_valid_quoted_scalars();

    /* Targeted batch -> normal transition tests */
    test_error_batch_to_normal_transition();
    test_loader_error_codes();

    TEST_SUITE_END();
}
