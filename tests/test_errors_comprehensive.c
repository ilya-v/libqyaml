#include "test_helper.h"

/*
 * Comprehensive error handling tests. Verify that the parser correctly
 * detects and reports errors for all categories of invalid YAML input.
 */

/* Helper: check that parsing fails (returns error before stream end) */
static int parse_should_fail(const char *input) {
    yaml_parser_t parser;
    yaml_event_t event;
    int failed = 0;

    if (!yaml_parser_initialize(&parser)) return 0;
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));

    while (1) {
        if (!yaml_parser_parse(&parser, &event)) {
            failed = 1;
            break;
        }
        if (event.type == YAML_STREAM_END_EVENT) {
            yaml_event_delete(&event);
            break;
        }
        yaml_event_delete(&event);
    }
    yaml_parser_delete(&parser);
    return failed;
}

/* Helper: check that scanning fails */
static int scan_should_fail(const char *input) {
    yaml_parser_t parser;
    yaml_token_t token;
    int failed = 0;

    if (!yaml_parser_initialize(&parser)) return 0;
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));

    while (1) {
        if (!yaml_parser_scan(&parser, &token)) {
            failed = 1;
            break;
        }
        if (token.type == YAML_STREAM_END_TOKEN) {
            yaml_token_delete(&token);
            break;
        }
        yaml_token_delete(&token);
    }
    yaml_parser_delete(&parser);
    return failed;
}

/* Helper: check that parsing succeeds */
static int parse_should_succeed(const char *input) {
    yaml_parser_t parser;
    yaml_event_t event;
    int ok = 0;

    if (!yaml_parser_initialize(&parser)) return 0;
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));

    while (1) {
        if (!yaml_parser_parse(&parser, &event)) break;
        if (event.type == YAML_STREAM_END_EVENT) {
            yaml_event_delete(&event);
            ok = 1;
            break;
        }
        yaml_event_delete(&event);
    }
    yaml_parser_delete(&parser);
    return ok;
}

/* ========== Invalid characters ========== */

static void test_err_nul_in_plain(void) {
    const unsigned char input[] = {'a', ':', ' ', 0, 'b'};
    yaml_parser_t parser;
    yaml_token_t token;
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, input, 5);
    int failed = 0;
    while (1) {
        if (!yaml_parser_scan(&parser, &token)) { failed = 1; break; }
        if (token.type == YAML_STREAM_END_TOKEN) { yaml_token_delete(&token); break; }
        yaml_token_delete(&token);
    }
    yaml_parser_delete(&parser);
    ASSERT(failed, "NUL in plain scalar");
}

static void test_err_ctrl_0x01(void) {
    const unsigned char input[] = {0x01};
    yaml_parser_t parser;
    yaml_token_t token;
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, input, 1);
    int failed = 0;
    while (1) {
        if (!yaml_parser_scan(&parser, &token)) { failed = 1; break; }
        if (token.type == YAML_STREAM_END_TOKEN) { yaml_token_delete(&token); break; }
        yaml_token_delete(&token);
    }
    yaml_parser_delete(&parser);
    ASSERT(failed, "control 0x01");
}

static void test_err_ctrl_0x02(void) {
    const unsigned char input[] = {0x02};
    yaml_parser_t parser;
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, input, 1);
    yaml_token_t token;
    int failed = 0;
    while (!failed) {
        if (!yaml_parser_scan(&parser, &token)) { failed = 1; break; }
        if (token.type == YAML_STREAM_END_TOKEN) { yaml_token_delete(&token); break; }
        yaml_token_delete(&token);
    }
    yaml_parser_delete(&parser);
    ASSERT(failed, "control 0x02");
}

static void test_err_ctrl_0x7f(void) {
    const unsigned char input[] = {0x7F};
    yaml_parser_t parser;
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, input, 1);
    yaml_token_t token;
    int failed = 0;
    while (!failed) {
        if (!yaml_parser_scan(&parser, &token)) { failed = 1; break; }
        if (token.type == YAML_STREAM_END_TOKEN) { yaml_token_delete(&token); break; }
        yaml_token_delete(&token);
    }
    yaml_parser_delete(&parser);
    ASSERT(failed, "control 0x7F (DEL)");
}

/* ========== Invalid UTF-8 sequences ========== */

static void test_err_utf8_c0(void) {
    const unsigned char input[] = {0xC0, 0x80}; /* overlong NUL */
    yaml_parser_t parser;
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, input, 2);
    yaml_token_t token;
    int failed = 0;
    while (!failed) {
        if (!yaml_parser_scan(&parser, &token)) { failed = 1; break; }
        if (token.type == YAML_STREAM_END_TOKEN) { yaml_token_delete(&token); break; }
        yaml_token_delete(&token);
    }
    yaml_parser_delete(&parser);
    ASSERT(failed, "overlong C0 80");
}

static void test_err_utf8_c1(void) {
    const unsigned char input[] = {0xC1, 0xBF}; /* overlong */
    yaml_parser_t parser;
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, input, 2);
    yaml_token_t token;
    int failed = 0;
    while (!failed) {
        if (!yaml_parser_scan(&parser, &token)) { failed = 1; break; }
        if (token.type == YAML_STREAM_END_TOKEN) { yaml_token_delete(&token); break; }
        yaml_token_delete(&token);
    }
    yaml_parser_delete(&parser);
    ASSERT(failed, "overlong C1 BF");
}

static void test_err_utf8_80_alone(void) {
    const unsigned char input[] = {0x80};
    yaml_parser_t parser;
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, input, 1);
    yaml_token_t token;
    int failed = 0;
    while (!failed) {
        if (!yaml_parser_scan(&parser, &token)) { failed = 1; break; }
        if (token.type == YAML_STREAM_END_TOKEN) { yaml_token_delete(&token); break; }
        yaml_token_delete(&token);
    }
    yaml_parser_delete(&parser);
    ASSERT(failed, "lone continuation 0x80");
}

static void test_err_utf8_bf_alone(void) {
    const unsigned char input[] = {0xBF};
    yaml_parser_t parser;
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, input, 1);
    yaml_token_t token;
    int failed = 0;
    while (!failed) {
        if (!yaml_parser_scan(&parser, &token)) { failed = 1; break; }
        if (token.type == YAML_STREAM_END_TOKEN) { yaml_token_delete(&token); break; }
        yaml_token_delete(&token);
    }
    yaml_parser_delete(&parser);
    ASSERT(failed, "lone continuation 0xBF");
}

static void test_err_utf8_f5(void) {
    const unsigned char input[] = {0xF5, 0x80, 0x80, 0x80}; /* > U+10FFFF */
    yaml_parser_t parser;
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, input, 4);
    yaml_token_t token;
    int failed = 0;
    while (!failed) {
        if (!yaml_parser_scan(&parser, &token)) { failed = 1; break; }
        if (token.type == YAML_STREAM_END_TOKEN) { yaml_token_delete(&token); break; }
        yaml_token_delete(&token);
    }
    yaml_parser_delete(&parser);
    ASSERT(failed, "F5 out of range");
}

static void test_err_utf8_truncated_e0(void) {
    const unsigned char input[] = {0xE0, 0x80}; /* truncated + overlong */
    yaml_parser_t parser;
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, input, 2);
    yaml_token_t token;
    int failed = 0;
    while (!failed) {
        if (!yaml_parser_scan(&parser, &token)) { failed = 1; break; }
        if (token.type == YAML_STREAM_END_TOKEN) { yaml_token_delete(&token); break; }
        yaml_token_delete(&token);
    }
    yaml_parser_delete(&parser);
    ASSERT(failed, "truncated E0");
}

/* ========== Scanner errors ========== */

static void test_err_tab_indentation(void) {
    ASSERT(scan_should_fail("\tkey: value"), "tab indentation");
}

static void test_err_bad_directive(void) {
    ASSERT(scan_should_fail("%BADDIR\n---\n"), "bad directive");
}

static void test_err_duplicate_yaml_directive(void) {
    /* Duplicate YAML directive is a parser error, not a scanner error */
    ASSERT(parse_should_fail("%YAML 1.1\n%YAML 1.1\n---\n"), "duplicate YAML directive");
}

static void test_err_bad_yaml_version(void) {
    /* Bad YAML version is a parser error, not a scanner error */
    ASSERT(parse_should_fail("%YAML 2.0\n---\n"), "YAML 2.0");
}

static void test_err_unclosed_single_quote(void) {
    ASSERT(scan_should_fail("'unclosed"), "unclosed single quote");
}

static void test_err_unclosed_double_quote(void) {
    ASSERT(scan_should_fail("\"unclosed"), "unclosed double quote");
}

static void test_err_bad_escape_sequence(void) {
    ASSERT(scan_should_fail("\"\\q\""), "bad escape \\q");
}

static void test_err_bad_hex_escape(void) {
    ASSERT(scan_should_fail("\"\\xZZ\""), "bad hex escape");
}

static void test_err_bad_unicode_escape(void) {
    ASSERT(scan_should_fail("\"\\uZZZZ\""), "bad unicode escape");
}

static void test_err_unclosed_flow_seq(void) {
    ASSERT(parse_should_fail("[a, b"), "unclosed flow seq");
}

static void test_err_unclosed_flow_map(void) {
    ASSERT(parse_should_fail("{a: b"), "unclosed flow map");
}

static void test_err_extra_close_bracket(void) {
    ASSERT(parse_should_fail("]"), "extra close bracket");
}

static void test_err_extra_close_brace(void) {
    ASSERT(parse_should_fail("}"), "extra close brace");
}

static void test_err_unexpected_mapping_key(void) {
    /* Starting a mapping with value indicator at wrong level */
    ASSERT(parse_should_fail("a: b\n c: d\n"), "bad indentation");
}

static void test_err_bad_anchor_chars(void) {
    ASSERT(scan_should_fail("&an[chor value"), "bad anchor chars");
}

static void test_err_empty_anchor(void) {
    ASSERT(scan_should_fail("& value"), "empty anchor");
}

static void test_err_bad_tag_handle(void) {
    ASSERT(scan_should_fail("!{bad} value"), "bad tag handle");
}

static void test_err_unclosed_verbatim_tag(void) {
    ASSERT(scan_should_fail("!<unclosed value"), "unclosed verbatim tag");
}

/* ========== Parser errors ========== */

static void test_err_duplicate_anchor(void) {
    /* libyaml doesn't error on duplicate anchors (last one wins) */
    ASSERT(parse_should_succeed("- &a x\n- &a y\n- *a\n"), "dup anchor ok");
}

static void test_err_undefined_alias(void) {
    /* libyaml defers alias resolution to the loader, not the parser.
     * The parser accepts undefined aliases; the loader rejects them. */
    yaml_parser_t parser;
    yaml_document_t doc;
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)"*undefined", 10);
    int load_ok = yaml_parser_load(&parser, &doc);
    if (load_ok) yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
    ASSERT(!load_ok, "undefined alias rejected by loader");
}

static void test_err_bad_indentation_increase(void) {
    ASSERT(parse_should_fail("a: b\n c: d\n"), "bad indent increase");
}

static void test_err_mapping_in_flow_seq(void) {
    /* Implicit key in flow is limited */
    ASSERT(parse_should_succeed("[a: b]"), "implicit key in flow seq");
}

/* ========== Block scalar errors ========== */

static void test_err_literal_no_content(void) {
    /* Literal block with no content is valid (empty scalar) */
    ASSERT(parse_should_succeed("|\n"), "literal no content");
}

static void test_err_bad_block_indent_indicator(void) {
    ASSERT(scan_should_fail("|0\n  hello\n"), "block indent 0");
}

static void test_err_bad_block_chomp(void) {
    ASSERT(scan_should_fail("|x\n  hello\n"), "bad block chomp char");
}

/* ========== Error message quality ========== */

static void test_err_has_problem_string(void) {
    yaml_parser_t parser;
    yaml_event_t event;
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)"[", 1);
    while (1) {
        if (!yaml_parser_parse(&parser, &event)) break;
        if (event.type == YAML_STREAM_END_EVENT) { yaml_event_delete(&event); break; }
        yaml_event_delete(&event);
    }
    ASSERT_NOT_NULL(parser.problem, "has problem string");
    ASSERT(strlen(parser.problem) > 0, "problem string not empty");
    yaml_parser_delete(&parser);
}

static void test_err_has_problem_mark(void) {
    yaml_parser_t parser;
    yaml_event_t event;
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)"\tkey: val", 9);
    while (1) {
        if (!yaml_parser_parse(&parser, &event)) break;
        if (event.type == YAML_STREAM_END_EVENT) { yaml_event_delete(&event); break; }
        yaml_event_delete(&event);
    }
    /* Problem mark should point to the tab */
    ASSERT_EQ_INT((int)parser.problem_mark.line, 0, "error on line 0");
    yaml_parser_delete(&parser);
}

static void test_err_has_context(void) {
    yaml_parser_t parser;
    yaml_event_t event;
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)"[a, b", 5);
    while (1) {
        if (!yaml_parser_parse(&parser, &event)) break;
        if (event.type == YAML_STREAM_END_EVENT) { yaml_event_delete(&event); break; }
        yaml_event_delete(&event);
    }
    /* Context provides additional info about where the error started */
    ASSERT(parser.problem != NULL, "has problem for unclosed seq");
    yaml_parser_delete(&parser);
}

/* ========== Valid edge cases (should NOT error) ========== */

static void test_ok_empty_stream(void) {
    ASSERT(parse_should_succeed(""), "empty stream ok");
}

static void test_ok_comment_only(void) {
    ASSERT(parse_should_succeed("# comment\n"), "comment only ok");
}

static void test_ok_explicit_doc(void) {
    ASSERT(parse_should_succeed("---\nhello\n...\n"), "explicit doc ok");
}

static void test_ok_nested_flow(void) {
    ASSERT(parse_should_succeed("[[[]]]"), "nested flow ok");
}

static void test_ok_deep_nesting(void) {
    ASSERT(parse_should_succeed("a:\n  b:\n    c:\n      d: e\n"), "deep nesting ok");
}

static void test_ok_all_scalar_styles(void) {
    ASSERT(parse_should_succeed("plain"), "plain ok");
    ASSERT(parse_should_succeed("'single'"), "single ok");
    ASSERT(parse_should_succeed("\"double\""), "double ok");
    ASSERT(parse_should_succeed("|\n  literal\n"), "literal ok");
    ASSERT(parse_should_succeed(">\n  folded\n"), "folded ok");
}

static void test_ok_anchors_aliases(void) {
    ASSERT(parse_should_succeed("- &a x\n- *a\n"), "anchor alias ok");
}

static void test_ok_tags(void) {
    ASSERT(parse_should_succeed("!!str hello"), "tag ok");
}

static void test_ok_flow_mapping_space(void) {
    ASSERT(parse_should_succeed("{a: b}"), "flow map ok");
}

static void test_ok_multiline_key(void) {
    ASSERT(parse_should_succeed("? |\n  key\n: value\n"), "multiline key ok");
}

static void test_ok_empty_value(void) {
    ASSERT(parse_should_succeed("key:\n"), "empty value ok");
}

static void test_ok_empty_seq_item(void) {
    ASSERT(parse_should_succeed("- \n- a\n"), "empty seq item ok");
}

static void test_ok_complex_key(void) {
    ASSERT(parse_should_succeed("? [a, b]\n: value\n"), "complex key ok");
}

static void test_ok_utf8_keys(void) {
    ASSERT(parse_should_succeed("\xc3\xa9: value\n"), "utf8 key ok");
}

static void test_ok_utf8_values(void) {
    ASSERT(parse_should_succeed("key: \xc3\xa9\n"), "utf8 value ok");
}

static void test_ok_long_scalar(void) {
    char buf[4096];
    memset(buf, 'a', 4095);
    buf[4095] = '\0';
    ASSERT(parse_should_succeed(buf), "long scalar ok");
}

static void test_ok_many_items(void) {
    char buf[4096];
    int pos = 0;
    for (int i = 0; i < 100 && pos < 4090; i++) {
        pos += snprintf(buf + pos, 4096 - pos, "- item%d\n", i);
    }
    ASSERT(parse_should_succeed(buf), "many items ok");
}

static void test_ok_yaml_directive(void) {
    ASSERT(parse_should_succeed("%YAML 1.1\n---\nhello\n"), "yaml directive ok");
}

static void test_ok_tag_directive(void) {
    ASSERT(parse_should_succeed("%TAG !e! tag:example.com,\n---\n!e!foo bar\n"), "tag directive ok");
}

static void test_ok_windows_newlines(void) {
    ASSERT(parse_should_succeed("a: b\r\nc: d\r\n"), "windows newlines ok");
}

static void test_ok_cr_only_newlines(void) {
    ASSERT(parse_should_succeed("a: b\rc: d\r"), "cr only ok");
}

static void test_ok_mixed_newlines(void) {
    ASSERT(parse_should_succeed("a: b\nc: d\r\ne: f\r"), "mixed newlines ok");
}

static void test_ok_trailing_whitespace(void) {
    ASSERT(parse_should_succeed("key: value   \n"), "trailing ws ok");
}

static void test_ok_indented_comment(void) {
    ASSERT(parse_should_succeed("key: value # comment\n"), "inline comment ok");
}

int main(void) {
    TEST_SUITE_BEGIN("Errors Comprehensive");

    /* Invalid characters */
    test_err_nul_in_plain();
    test_err_ctrl_0x01();
    test_err_ctrl_0x02();
    test_err_ctrl_0x7f();

    /* Invalid UTF-8 */
    test_err_utf8_c0();
    test_err_utf8_c1();
    test_err_utf8_80_alone();
    test_err_utf8_bf_alone();
    test_err_utf8_f5();
    test_err_utf8_truncated_e0();

    /* Scanner errors */
    test_err_tab_indentation();
    test_err_bad_directive();
    test_err_duplicate_yaml_directive();
    test_err_bad_yaml_version();
    test_err_unclosed_single_quote();
    test_err_unclosed_double_quote();
    test_err_bad_escape_sequence();
    test_err_bad_hex_escape();
    test_err_bad_unicode_escape();
    test_err_unclosed_flow_seq();
    test_err_unclosed_flow_map();
    test_err_extra_close_bracket();
    test_err_extra_close_brace();
    test_err_unexpected_mapping_key();
    test_err_bad_anchor_chars();
    test_err_empty_anchor();
    test_err_bad_tag_handle();
    test_err_unclosed_verbatim_tag();

    /* Parser errors */
    test_err_duplicate_anchor();
    test_err_undefined_alias();
    test_err_bad_indentation_increase();
    test_err_mapping_in_flow_seq();

    /* Block scalar errors */
    test_err_literal_no_content();
    test_err_bad_block_indent_indicator();
    test_err_bad_block_chomp();

    /* Error message quality */
    test_err_has_problem_string();
    test_err_has_problem_mark();
    test_err_has_context();

    /* Valid edge cases */
    test_ok_empty_stream();
    test_ok_comment_only();
    test_ok_explicit_doc();
    test_ok_nested_flow();
    test_ok_deep_nesting();
    test_ok_all_scalar_styles();
    test_ok_anchors_aliases();
    test_ok_tags();
    test_ok_flow_mapping_space();
    test_ok_multiline_key();
    test_ok_empty_value();
    test_ok_empty_seq_item();
    test_ok_complex_key();
    test_ok_utf8_keys();
    test_ok_utf8_values();
    test_ok_long_scalar();
    test_ok_many_items();
    test_ok_yaml_directive();
    test_ok_tag_directive();
    test_ok_windows_newlines();
    test_ok_cr_only_newlines();
    test_ok_mixed_newlines();
    test_ok_trailing_whitespace();
    test_ok_indented_comment();

    TEST_SUITE_END();
}
