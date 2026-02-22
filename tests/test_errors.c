#include "test_helper.h"

/* Helper: try to parse and return whether it errored */
static int parse_should_error(const char *yaml) {
    yaml_parser_t parser;
    yaml_event_t event;

    if (!yaml_parser_initialize(&parser)) return 1;
    yaml_parser_set_input_string(&parser, (const unsigned char *)yaml, strlen(yaml));

    while (1) {
        if (!yaml_parser_parse(&parser, &event)) {
            yaml_parser_delete(&parser);
            return 1; /* got error */
        }
        if (event.type == YAML_STREAM_END_EVENT) {
            yaml_event_delete(&event);
            yaml_parser_delete(&parser);
            return 0; /* no error */
        }
        yaml_event_delete(&event);
    }
}

/* Helper: try to parse and return whether it succeeded */
static int parse_should_succeed(const char *yaml) {
    yaml_parser_t parser;
    yaml_event_t event;

    if (!yaml_parser_initialize(&parser)) return 0;
    yaml_parser_set_input_string(&parser, (const unsigned char *)yaml, strlen(yaml));

    while (1) {
        if (!yaml_parser_parse(&parser, &event)) {
            yaml_parser_delete(&parser);
            return 0;
        }
        if (event.type == YAML_STREAM_END_EVENT) {
            yaml_event_delete(&event);
            yaml_parser_delete(&parser);
            return 1;
        }
        yaml_event_delete(&event);
    }
}

/* Test: unmatched flow indicators */
static void test_unmatched_flow(void) {
    ASSERT(parse_should_error("[a, b"), "unmatched [");
    ASSERT(parse_should_error("{a: b"), "unmatched {");
    ASSERT(parse_should_error("[a, {b: c}"), "unmatched [ with nested");
}

/* Test: duplicate keys (allowed by YAML spec but a warning) */
static void test_duplicate_keys(void) {
    /* libyaml allows duplicate keys -- just parses both */
    ASSERT(parse_should_succeed("a: 1\na: 2\n"), "duplicate keys: succeeds");
}

/* Test: invalid directives */
static void test_invalid_directives(void) {
    ASSERT(parse_should_error("%YAML 2.0\n---\n"), "YAML 2.0 directive");
    ASSERT(parse_should_error("%TAG\n---\n"), "incomplete TAG directive");
}

/* Test: tab indentation */
static void test_tab_indentation(void) {
    ASSERT(parse_should_error("key:\n\tvalue\n"), "tab indentation");
}

/* Test: various valid inputs that should not error */
static void test_valid_inputs(void) {
    /* Empty */
    ASSERT(parse_should_succeed(""), "valid: empty");
    ASSERT(parse_should_succeed("\n"), "valid: newline");
    ASSERT(parse_should_succeed("   \n"), "valid: whitespace");
    ASSERT(parse_should_succeed("# comment"), "valid: comment");

    /* Scalars */
    ASSERT(parse_should_succeed("hello"), "valid: plain scalar");
    ASSERT(parse_should_succeed("'hello'"), "valid: single quoted");
    ASSERT(parse_should_succeed("\"hello\""), "valid: double quoted");

    /* Numbers */
    ASSERT(parse_should_succeed("42"), "valid: integer");
    ASSERT(parse_should_succeed("3.14"), "valid: float");
    ASSERT(parse_should_succeed("-1"), "valid: negative");
    ASSERT(parse_should_succeed("0"), "valid: zero");

    /* Boolean-like */
    ASSERT(parse_should_succeed("true"), "valid: true");
    ASSERT(parse_should_succeed("false"), "valid: false");
    ASSERT(parse_should_succeed("null"), "valid: null");
    ASSERT(parse_should_succeed("~"), "valid: tilde");

    /* Mappings */
    ASSERT(parse_should_succeed("a: b"), "valid: simple mapping");
    ASSERT(parse_should_succeed("a: b\nc: d"), "valid: two-key mapping");
    ASSERT(parse_should_succeed("{a: b}"), "valid: flow mapping");
    ASSERT(parse_should_succeed("{a: b, c: d}"), "valid: flow mapping 2");
    ASSERT(parse_should_succeed("{}"), "valid: empty flow mapping");

    /* Sequences */
    ASSERT(parse_should_succeed("- a"), "valid: simple sequence");
    ASSERT(parse_should_succeed("- a\n- b"), "valid: two-item sequence");
    ASSERT(parse_should_succeed("[a, b]"), "valid: flow sequence");
    ASSERT(parse_should_succeed("[]"), "valid: empty flow sequence");

    /* Nested */
    ASSERT(parse_should_succeed("a:\n  b: c"), "valid: nested mapping");
    ASSERT(parse_should_succeed("a:\n  - b\n  - c"), "valid: nested sequence");
    ASSERT(parse_should_succeed("- a:\n    b: c"), "valid: seq of map");

    /* Documents */
    ASSERT(parse_should_succeed("---\nhello"), "valid: explicit doc");
    ASSERT(parse_should_succeed("---\nhello\n..."), "valid: doc with end");
    ASSERT(parse_should_succeed("---\na\n---\nb"), "valid: multi doc");

    /* Anchors */
    ASSERT(parse_should_succeed("&a hello"), "valid: anchor");
    ASSERT(parse_should_succeed("- &a hello\n- *a"), "valid: anchor+alias");

    /* Tags */
    ASSERT(parse_should_succeed("!!str hello"), "valid: tag");
    ASSERT(parse_should_succeed("!tag hello"), "valid: local tag");

    /* Block scalars */
    ASSERT(parse_should_succeed("|\n  hello\n  world"), "valid: literal");
    ASSERT(parse_should_succeed(">\n  hello\n  world"), "valid: folded");

    /* Directives */
    ASSERT(parse_should_succeed("%YAML 1.1\n---\nhello"), "valid: YAML directive");
    ASSERT(parse_should_succeed("%YAML 1.2\n---\nhello"), "valid: YAML 1.2 directive");
}

/* Test error information */
static void test_error_info(void) {
    yaml_parser_t parser;
    yaml_event_t event;

    ASSERT(yaml_parser_initialize(&parser), "error info: init");
    yaml_parser_set_input_string(&parser, (const unsigned char *)"[unclosed", 9);

    /* Parse until error */
    while (yaml_parser_parse(&parser, &event)) {
        if (event.type == YAML_STREAM_END_EVENT) {
            yaml_event_delete(&event);
            break;
        }
        yaml_event_delete(&event);
    }

    /* After error, check error fields */
    if (parser.error != YAML_NO_ERROR) {
        ASSERT(parser.error == YAML_SCANNER_ERROR || parser.error == YAML_PARSER_ERROR,
               "error info: type is scanner or parser");
        ASSERT_NOT_NULL(parser.problem, "error info: has problem message");
    }

    yaml_parser_delete(&parser);
}

/* Test: mapping where value is missing before next key */
static void test_missing_colon(void) {
    /* "a b" is a plain scalar, not a mapping -- should succeed */
    ASSERT(parse_should_succeed("a b"), "missing colon: plain scalar");
}

/* Test: deeply nested error */
static void test_deep_error(void) {
    /* Build deeply nested flow sequence that's unclosed */
    char yaml[256];
    int pos = 0;
    for (int i = 0; i < 50; i++) {
        yaml[pos++] = '[';
    }
    yaml[pos++] = 'a';
    yaml[pos] = '\0';
    ASSERT(parse_should_error(yaml), "deep nested unclosed");
}

/* Test: invalid anchor names */
static void test_invalid_anchors(void) {
    /* Empty anchor */
    ASSERT(parse_should_error("& value"), "empty anchor");
    /* Anchor with invalid chars */
    ASSERT(parse_should_error("&[invalid value"), "anchor with [");
}

/* Test: scanner error details */
static void test_scanner_errors(void) {
    yaml_parser_t parser;
    yaml_token_t token;

    /* Tab character where not allowed */
    ASSERT(yaml_parser_initialize(&parser), "scanner err: init");
    yaml_parser_set_input_string(&parser, (const unsigned char *)"key:\n\tvalue", 11);

    int got_error = 0;
    while (1) {
        if (!yaml_parser_scan(&parser, &token)) {
            got_error = 1;
            break;
        }
        if (token.type == YAML_STREAM_END_TOKEN) {
            yaml_token_delete(&token);
            break;
        }
        yaml_token_delete(&token);
    }
    ASSERT(got_error, "scanner err: tab detected");
    yaml_parser_delete(&parser);
}

/* ==================================================================
 * Extended error tests: invalid YAML constructs
 * ================================================================== */

static void test_err_duplicate_yaml_directive(void) {
    ASSERT(parse_should_error("%YAML 1.1\n%YAML 1.1\n---\nhello\n"),
           "duplicate YAML directive");
}

static void test_err_duplicate_tag_directive(void) {
    ASSERT(parse_should_error("%TAG !e! tag:a\n%TAG !e! tag:b\n---\nhello\n"),
           "duplicate TAG directive");
}

static void test_err_invalid_yaml_version(void) {
    ASSERT(parse_should_error("%YAML 2.0\n---\nhello\n"), "YAML 2.0");
}

static void test_err_unterminated_double_quote(void) {
    ASSERT(parse_should_error("\"unterminated"), "unterminated double quote");
}

static void test_err_unterminated_single_quote(void) {
    ASSERT(parse_should_error("'unterminated"), "unterminated single quote");
}

static void test_err_tab_as_indentation(void) {
    ASSERT(parse_should_error("key:\n\tvalue\n"), "tab indentation error");
}

static void test_err_unclosed_flow_seq(void) {
    ASSERT(parse_should_error("[a, b"), "unclosed flow seq");
}

static void test_err_unclosed_flow_map(void) {
    ASSERT(parse_should_error("{a: b"), "unclosed flow map");
}

static void test_err_block_scalar_indent_zero(void) {
    ASSERT(parse_should_error("|0\n  text\n"), "block scalar indent 0");
}

static void test_err_block_scalar_bad_header(void) {
    ASSERT(parse_should_error("| extra\n  text\n"), "block scalar bad header");
}

static void test_err_empty_anchor(void) {
    ASSERT(parse_should_error("& value\n"), "empty anchor");
}

static void test_err_undefined_alias(void) {
    /* Undefined alias is a composer error (loader), not parser error.
     * At the parser level, *noexist is valid syntax. */
    yaml_parser_t parser;
    yaml_document_t doc;
    ASSERT(yaml_parser_initialize(&parser), "undef alias init");
    yaml_parser_set_input_string(&parser, (const unsigned char *)"*noexist\n", 9);
    int result = yaml_parser_load(&parser, &doc);
    ASSERT(!result, "undefined alias: load fails");
    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
}

static void test_err_unknown_directive(void) {
    ASSERT(parse_should_error("%UNKNOWN foo\n---\n"), "unknown directive");
}

static void test_err_empty_directive_name(void) {
    ASSERT(parse_should_error("% \n---\n"), "empty directive name");
}

static void test_err_verbatim_tag_eof(void) {
    ASSERT(parse_should_error("!<unclosed value\n"), "verbatim tag eof");
}

static void test_err_verbatim_tag_empty(void) {
    ASSERT(parse_should_error("!<> value\n"), "empty verbatim tag");
}

static void test_err_flow_scalar_null_byte(void) {
    /* NUL byte in flow scalar -- length-based parser handles it differently */
    yaml_parser_t parser;
    yaml_event_t event;
    const unsigned char input[] = {'\"', 'a', 0x00, 'b', '\"', 0};

    ASSERT(yaml_parser_initialize(&parser), "null byte init");
    yaml_parser_set_input_string(&parser, input, 5);

    int errored = 0;
    while (1) {
        if (!yaml_parser_parse(&parser, &event)) { errored = 1; break; }
        if (event.type == YAML_STREAM_END_EVENT) { yaml_event_delete(&event); break; }
        yaml_event_delete(&event);
    }
    /* May or may not error; just verify no crash */
    ASSERT(1, "null byte: completed");
    (void)errored;
    yaml_parser_delete(&parser);
}

static void test_err_deeply_nested_flow(void) {
    char yaml[256];
    int pos = 0;
    for (int i = 0; i < 100; i++) yaml[pos++] = '[';
    yaml[pos++] = 'a';
    yaml[pos] = '\0';
    ASSERT(parse_should_error(yaml), "100-deep unclosed flow");
}

static void test_err_unclosed_nested_map(void) {
    ASSERT(parse_should_error("{a: {b: c}"), "unclosed nested flow map");
}

static void test_err_extra_close_bracket(void) {
    ASSERT(parse_should_error("[a]]"), "extra close bracket");
}

static void test_err_extra_close_brace(void) {
    ASSERT(parse_should_error("{a: b}}"), "extra close brace");
}

static void test_err_mismatch_brackets(void) {
    ASSERT(parse_should_error("[a}"), "mismatched [ }");
}

static void test_err_mismatch_braces(void) {
    ASSERT(parse_should_error("{a]"), "mismatched { ]");
}

static void test_err_anchor_with_bracket(void) {
    ASSERT(parse_should_error("&[anchor value\n"), "anchor with [");
}

static void test_err_bad_escape_sequence(void) {
    ASSERT(parse_should_error("\"\\z\""), "bad escape \\z");
}

static void test_err_bad_hex_escape(void) {
    ASSERT(parse_should_error("\"\\xGG\""), "bad hex escape");
}

static void test_err_bad_unicode_escape(void) {
    ASSERT(parse_should_error("\"\\uGGGG\""), "bad unicode escape");
}

static void test_err_bad_unicode_escape_u8(void) {
    ASSERT(parse_should_error("\"\\UGGGGGGGG\""), "bad U+NNNNNNNN escape");
}

static void test_err_bad_version_format(void) {
    ASSERT(parse_should_error("%YAML abc\n---\n"), "bad version format");
}

static void test_err_bad_tag_directive_nohandle(void) {
    ASSERT(parse_should_error("%TAG \n---\n"), "TAG no handle");
}

static void test_err_doc_indicator_in_single_quote(void) {
    ASSERT(parse_should_error("'\n---\n'"), "--- in single quote");
}

static void test_err_doc_indicator_in_double_quote(void) {
    ASSERT(parse_should_error("\"\n---\n\""), "--- in double quote");
}

/* ==================================================================
 * Extended valid inputs: more YAML constructs that should succeed
 * ================================================================== */

static void test_valid_multiline_values(void) {
    ASSERT(parse_should_succeed("key: |\n  line1\n  line2\n"), "valid: literal block");
    ASSERT(parse_should_succeed("key: >\n  line1\n  line2\n"), "valid: folded block");
    ASSERT(parse_should_succeed("key: |+\n  text\n\n"), "valid: literal keep");
    ASSERT(parse_should_succeed("key: |-\n  text\n"), "valid: literal strip");
    ASSERT(parse_should_succeed("key: >2\n  text\n"), "valid: folded indent");
}

static void test_valid_complex_structures(void) {
    ASSERT(parse_should_succeed("a:\n  b:\n    c:\n      d: e\n"), "valid: deep nested");
    ASSERT(parse_should_succeed("- a: b\n  c: d\n- e: f\n"), "valid: seq of maps");
    ASSERT(parse_should_succeed("a:\n- b\n- c\n"), "valid: indentless seq");
    ASSERT(parse_should_succeed("[{a: b}, {c: d}]\n"), "valid: flow seq of maps");
    ASSERT(parse_should_succeed("{a: [1, 2], b: [3, 4]}\n"), "valid: flow map of seqs");
}

static void test_valid_special_scalars(void) {
    ASSERT(parse_should_succeed("key: ''"), "valid: empty single quoted");
    ASSERT(parse_should_succeed("key: \"\""), "valid: empty double quoted");
    ASSERT(parse_should_succeed("key: !!null ''"), "valid: tagged empty");
    ASSERT(parse_should_succeed("? key\n: value\n"), "valid: explicit key");
    ASSERT(parse_should_succeed("{? key : value}\n"), "valid: flow explicit key");
}

static void test_valid_unicode_content(void) {
    ASSERT(parse_should_succeed("key: \"\\u0041\""), "valid: unicode A");
    ASSERT(parse_should_succeed("key: \"\\U00000041\""), "valid: unicode U+0041");
    ASSERT(parse_should_succeed("key: \"\\n\\t\\\\\""), "valid: escape sequences");
    ASSERT(parse_should_succeed("key: \"\\x41\""), "valid: hex escape");
}

static void test_valid_anchors_aliases_extended(void) {
    ASSERT(parse_should_succeed("- &a [1, 2]\n- *a\n"), "valid: seq anchor");
    ASSERT(parse_should_succeed("- &a {x: 1}\n- *a\n"), "valid: map anchor");
    ASSERT(parse_should_succeed("a: &ref\n  x: 1\nb: *ref\n"), "valid: mapping anchor");
}

static void test_valid_multiple_documents(void) {
    ASSERT(parse_should_succeed("---\na\n...\n---\nb\n...\n"), "valid: 2 docs with end");
    ASSERT(parse_should_succeed("---\na\n---\nb\n"), "valid: 2 docs implicit end");
    /* "...\n" alone is not a valid document -- skip it */
}

static void test_valid_comments(void) {
    ASSERT(parse_should_succeed("# comment\n"), "valid: only comment");
    ASSERT(parse_should_succeed("key: value # comment\n"), "valid: inline comment");
    ASSERT(parse_should_succeed("# comment\nkey: value\n"), "valid: leading comment");
    ASSERT(parse_should_succeed("key:\n  # comment\n  value\n"), "valid: block comment");
}

static void test_valid_tag_directives(void) {
    ASSERT(parse_should_succeed("%TAG ! tag:example.com:\n---\n!foo bar\n"),
           "valid: primary tag directive");
    ASSERT(parse_should_succeed(
        "%TAG !e! tag:example.com,2000:\n---\n!e!foo bar\n"),
        "valid: custom tag directive");
}

/* ==================================================================
 * Error recovery behavior
 * ================================================================== */

static void test_error_type_fields(void) {
    yaml_parser_t parser;
    yaml_event_t event;

    ASSERT(yaml_parser_initialize(&parser), "err fields: init");
    yaml_parser_set_input_string(&parser,
        (const unsigned char *)"%UNKNOWN foo\n", 13);

    while (yaml_parser_parse(&parser, &event)) {
        if (event.type == YAML_STREAM_END_EVENT) {
            yaml_event_delete(&event);
            break;
        }
        yaml_event_delete(&event);
    }

    if (parser.error != YAML_NO_ERROR) {
        ASSERT(parser.error == YAML_SCANNER_ERROR ||
               parser.error == YAML_PARSER_ERROR ||
               parser.error == YAML_COMPOSER_ERROR,
               "err fields: known error type");
        ASSERT_NOT_NULL(parser.problem, "err fields: has problem");
        /* problem_mark should be set */
        ASSERT(parser.problem_mark.line < 100, "err fields: reasonable line");
    }
    yaml_parser_delete(&parser);
}

static void test_error_context_fields(void) {
    yaml_parser_t parser;
    yaml_event_t event;

    ASSERT(yaml_parser_initialize(&parser), "err context: init");
    yaml_parser_set_input_string(&parser,
        (const unsigned char *)"{a: b, c]", 9);

    while (yaml_parser_parse(&parser, &event)) {
        if (event.type == YAML_STREAM_END_EVENT) {
            yaml_event_delete(&event);
            break;
        }
        yaml_event_delete(&event);
    }

    if (parser.error != YAML_NO_ERROR) {
        ASSERT_NOT_NULL(parser.problem, "err context: has problem");
        /* Context may or may not be set depending on error type */
    }
    yaml_parser_delete(&parser);
}

int main(void) {
    TEST_SUITE_BEGIN("Errors");

    test_unmatched_flow();
    test_duplicate_keys();
    test_invalid_directives();
    test_tab_indentation();
    test_valid_inputs();
    test_error_info();
    test_missing_colon();
    test_deep_error();
    test_invalid_anchors();
    test_scanner_errors();

    /* Extended error cases */
    test_err_duplicate_yaml_directive();
    test_err_duplicate_tag_directive();
    test_err_invalid_yaml_version();
    test_err_unterminated_double_quote();
    test_err_unterminated_single_quote();
    test_err_tab_as_indentation();
    test_err_unclosed_flow_seq();
    test_err_unclosed_flow_map();
    test_err_block_scalar_indent_zero();
    test_err_block_scalar_bad_header();
    test_err_empty_anchor();
    test_err_undefined_alias();
    test_err_unknown_directive();
    test_err_empty_directive_name();
    test_err_verbatim_tag_eof();
    test_err_verbatim_tag_empty();
    test_err_flow_scalar_null_byte();
    test_err_deeply_nested_flow();
    test_err_unclosed_nested_map();
    test_err_extra_close_bracket();
    test_err_extra_close_brace();
    test_err_mismatch_brackets();
    test_err_mismatch_braces();
    test_err_anchor_with_bracket();
    test_err_bad_escape_sequence();
    test_err_bad_hex_escape();
    test_err_bad_unicode_escape();
    test_err_bad_unicode_escape_u8();
    test_err_bad_version_format();
    test_err_bad_tag_directive_nohandle();
    test_err_doc_indicator_in_single_quote();
    test_err_doc_indicator_in_double_quote();

    /* Extended valid inputs */
    test_valid_multiline_values();
    test_valid_complex_structures();
    test_valid_special_scalars();
    test_valid_unicode_content();
    test_valid_anchors_aliases_extended();
    test_valid_multiple_documents();
    test_valid_comments();
    test_valid_tag_directives();

    /* Error field inspection */
    test_error_type_fields();
    test_error_context_fields();

    TEST_SUITE_END();
}
