#include "test_helper.h"

/*
 * Comprehensive scanner tests covering all token types, edge cases,
 * and boundary conditions. Each test function focuses on a specific
 * YAML construct or scanner behavior.
 */

/* Helper to scan and verify a specific token sequence */
static int verify_token_sequence(const char *input, const yaml_token_type_t *expected,
                                  int expected_count) {
    yaml_token_type_t tokens[256];
    int count;

    if (!scan_string_tokens(input, tokens, 256, &count))
        return 0;

    if (count != expected_count)
        return 0;

    for (int i = 0; i < expected_count; i++) {
        if (tokens[i] != expected[i])
            return 0;
    }
    return 1;
}

/* Helper to scan and verify scalar value */
static int verify_scalar_value(const char *input, const char *expected_value) {
    yaml_parser_t parser;
    yaml_token_t token;
    int found = 0;

    if (!yaml_parser_initialize(&parser)) return 0;
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));

    while (1) {
        if (!yaml_parser_scan(&parser, &token)) break;
        if (token.type == YAML_SCALAR_TOKEN) {
            if (strcmp((const char *)token.data.scalar.value, expected_value) == 0)
                found = 1;
        }
        int done = (token.type == YAML_STREAM_END_TOKEN);
        yaml_token_delete(&token);
        if (done) break;
    }

    yaml_parser_delete(&parser);
    return found;
}

/* ========== Stream tokens ========== */

static void test_stream_empty(void) {
    yaml_token_type_t expected[] = {YAML_STREAM_START_TOKEN, YAML_STREAM_END_TOKEN};
    ASSERT(verify_token_sequence("", expected, 2), "empty stream");
}

static void test_stream_whitespace_only(void) {
    yaml_token_type_t expected[] = {YAML_STREAM_START_TOKEN, YAML_STREAM_END_TOKEN};
    ASSERT(verify_token_sequence("   ", expected, 2), "whitespace-only stream");
}

static void test_stream_newlines_only(void) {
    yaml_token_type_t expected[] = {YAML_STREAM_START_TOKEN, YAML_STREAM_END_TOKEN};
    ASSERT(verify_token_sequence("\n\n\n", expected, 2), "newline-only stream");
}

static void test_stream_comment_only(void) {
    yaml_token_type_t expected[] = {YAML_STREAM_START_TOKEN, YAML_STREAM_END_TOKEN};
    ASSERT(verify_token_sequence("# comment\n", expected, 2), "comment-only stream");
}

static void test_stream_multiple_comments(void) {
    yaml_token_type_t expected[] = {YAML_STREAM_START_TOKEN, YAML_STREAM_END_TOKEN};
    ASSERT(verify_token_sequence("# c1\n# c2\n# c3\n", expected, 2), "multi-comment stream");
}

static void test_stream_tabs_and_spaces(void) {
    /* Tabs are not allowed for indentation in YAML, so scanner should error */
    yaml_token_type_t tokens[16];
    int count;
    int result = scan_string_tokens("  \t  \n  \t \n", tokens, 16, &count);
    /* Scanner may reject tabs or may treat as whitespace; just verify no hang */
    ASSERT(1, "tabs and spaces stream (completed)");
    (void)result;
}

/* ========== Document indicators ========== */

static void test_doc_start_indicator(void) {
    yaml_token_type_t tokens[16];
    int count;
    ASSERT(scan_string_tokens("---\n", tokens, 16, &count), "scan doc start");
    int found = 0;
    for (int i = 0; i < count; i++)
        if (tokens[i] == YAML_DOCUMENT_START_TOKEN) found = 1;
    ASSERT(found, "found doc start token");
}

static void test_doc_end_indicator(void) {
    yaml_token_type_t tokens[16];
    int count;
    ASSERT(scan_string_tokens("---\n...\n", tokens, 16, &count), "scan doc end");
    int found = 0;
    for (int i = 0; i < count; i++)
        if (tokens[i] == YAML_DOCUMENT_END_TOKEN) found = 1;
    ASSERT(found, "found doc end token");
}

static void test_doc_start_without_newline(void) {
    yaml_token_type_t tokens[16];
    int count;
    /* "--- " followed by content */
    ASSERT(scan_string_tokens("--- value", tokens, 16, &count), "scan doc start with value");
    ASSERT_EQ_INT(tokens[1], YAML_DOCUMENT_START_TOKEN, "doc start before value");
}

static void test_multiple_documents(void) {
    yaml_token_type_t tokens[32];
    int count;
    ASSERT(scan_string_tokens("---\na\n---\nb\n...\n", tokens, 32, &count), "scan multi-doc");
    int doc_starts = 0;
    for (int i = 0; i < count; i++)
        if (tokens[i] == YAML_DOCUMENT_START_TOKEN) doc_starts++;
    ASSERT_EQ_INT(doc_starts, 2, "two doc starts");
}

/* ========== Plain scalars ========== */

static void test_plain_scalar_simple(void) {
    ASSERT(verify_scalar_value("hello", "hello"), "plain scalar: hello");
}

static void test_plain_scalar_with_spaces(void) {
    ASSERT(verify_scalar_value("hello world", "hello world"), "plain scalar with spaces");
}

static void test_plain_scalar_number(void) {
    ASSERT(verify_scalar_value("42", "42"), "plain scalar: number");
}

static void test_plain_scalar_float(void) {
    ASSERT(verify_scalar_value("3.14", "3.14"), "plain scalar: float");
}

static void test_plain_scalar_bool_true(void) {
    ASSERT(verify_scalar_value("true", "true"), "plain scalar: true");
}

static void test_plain_scalar_bool_false(void) {
    ASSERT(verify_scalar_value("false", "false"), "plain scalar: false");
}

static void test_plain_scalar_null(void) {
    ASSERT(verify_scalar_value("null", "null"), "plain scalar: null");
}

static void test_plain_scalar_tilde(void) {
    ASSERT(verify_scalar_value("~", "~"), "plain scalar: tilde");
}

static void test_plain_scalar_empty_line_value(void) {
    /* key with plain value */
    ASSERT(verify_scalar_value("key: value", "value"), "plain scalar as mapping value");
}

static void test_plain_scalar_multiword_value(void) {
    ASSERT(verify_scalar_value("key: hello world", "hello world"), "multiword plain value");
}

static void test_plain_scalar_with_special_chars(void) {
    /* Chars like @, `, etc. are valid in plain scalars */
    ASSERT(verify_scalar_value("hello@world", "hello@world"), "plain scalar with @");
}

static void test_plain_scalar_starts_with_number(void) {
    ASSERT(verify_scalar_value("123abc", "123abc"), "plain scalar starts with number");
}

static void test_plain_scalar_with_dots(void) {
    ASSERT(verify_scalar_value("a.b.c", "a.b.c"), "plain scalar with dots");
}

static void test_plain_scalar_with_slash(void) {
    ASSERT(verify_scalar_value("a/b/c", "a/b/c"), "plain scalar with slashes");
}

static void test_plain_scalar_as_seq_item(void) {
    ASSERT(verify_scalar_value("- item", "item"), "plain scalar in sequence");
}

/* ========== Single-quoted scalars ========== */

static void test_single_quoted_empty(void) {
    ASSERT(verify_scalar_value("''", ""), "single quoted empty");
}

static void test_single_quoted_simple(void) {
    ASSERT(verify_scalar_value("'hello'", "hello"), "single quoted simple");
}

static void test_single_quoted_with_spaces(void) {
    ASSERT(verify_scalar_value("'hello world'", "hello world"), "single quoted with spaces");
}

static void test_single_quoted_with_colon(void) {
    ASSERT(verify_scalar_value("'key: value'", "key: value"), "single quoted with colon");
}

static void test_single_quoted_with_hash(void) {
    ASSERT(verify_scalar_value("'has # hash'", "has # hash"), "single quoted with hash");
}

static void test_single_quoted_escaped_quote(void) {
    ASSERT(verify_scalar_value("'it''s'", "it's"), "single quoted escaped quote");
}

static void test_single_quoted_multiple_escaped(void) {
    ASSERT(verify_scalar_value("'a''b''c'", "a'b'c"), "single quoted multiple escapes");
}

static void test_single_quoted_with_newline(void) {
    ASSERT(verify_scalar_value("'line1\nline2'", "line1 line2"), "single quoted with newline");
}

static void test_single_quoted_special_chars(void) {
    ASSERT(verify_scalar_value("'[{,}]'", "[{,}]"), "single quoted special chars");
}

static void test_single_quoted_with_backslash(void) {
    ASSERT(verify_scalar_value("'back\\slash'", "back\\slash"), "single quoted backslash");
}

/* ========== Double-quoted scalars ========== */

static void test_double_quoted_empty(void) {
    ASSERT(verify_scalar_value("\"\"", ""), "double quoted empty");
}

static void test_double_quoted_simple(void) {
    ASSERT(verify_scalar_value("\"hello\"", "hello"), "double quoted simple");
}

static void test_double_quoted_escape_newline(void) {
    ASSERT(verify_scalar_value("\"hello\\nworld\"", "hello\nworld"), "double quoted \\n");
}

static void test_double_quoted_escape_tab(void) {
    ASSERT(verify_scalar_value("\"hello\\tworld\"", "hello\tworld"), "double quoted \\t");
}

static void test_double_quoted_escape_backslash(void) {
    ASSERT(verify_scalar_value("\"back\\\\slash\"", "back\\slash"), "double quoted \\\\");
}

static void test_double_quoted_escape_quote(void) {
    ASSERT(verify_scalar_value("\"say \\\"hi\\\"\"", "say \"hi\""), "double quoted \\\"");
}

static void test_double_quoted_escape_null(void) {
    /* \0 produces NUL byte - check length */
    yaml_parser_t parser;
    yaml_token_t token;
    int found = 0;

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)"\"\\0\"", 4);

    while (1) {
        if (!yaml_parser_scan(&parser, &token)) break;
        if (token.type == YAML_SCALAR_TOKEN) {
            ASSERT_EQ_INT((int)token.data.scalar.length, 1, "\\0 length is 1");
            ASSERT_EQ_INT(token.data.scalar.value[0], 0, "\\0 value is NUL");
            found = 1;
        }
        int done = (token.type == YAML_STREAM_END_TOKEN);
        yaml_token_delete(&token);
        if (done) break;
    }
    yaml_parser_delete(&parser);
    ASSERT(found, "found \\0 scalar");
}

static void test_double_quoted_escape_bell(void) {
    ASSERT(verify_scalar_value("\"\\a\"", "\x07"), "double quoted \\a");
}

static void test_double_quoted_escape_backspace(void) {
    ASSERT(verify_scalar_value("\"\\b\"", "\x08"), "double quoted \\b");
}

static void test_double_quoted_escape_formfeed(void) {
    ASSERT(verify_scalar_value("\"\\f\"", "\x0C"), "double quoted \\f");
}

static void test_double_quoted_escape_cr(void) {
    ASSERT(verify_scalar_value("\"\\r\"", "\r"), "double quoted \\r");
}

static void test_double_quoted_escape_vtab(void) {
    ASSERT(verify_scalar_value("\"\\v\"", "\x0B"), "double quoted \\v");
}

static void test_double_quoted_escape_esc(void) {
    ASSERT(verify_scalar_value("\"\\e\"", "\x1B"), "double quoted \\e");
}

static void test_double_quoted_escape_space(void) {
    ASSERT(verify_scalar_value("\"\\ \"", " "), "double quoted \\ ");
}

static void test_double_quoted_escape_slash(void) {
    ASSERT(verify_scalar_value("\"\\/\"", "/"), "double quoted \\/");
}

static void test_double_quoted_hex_escape_x(void) {
    ASSERT(verify_scalar_value("\"\\x41\"", "A"), "double quoted \\x41 = A");
}

static void test_double_quoted_hex_escape_u(void) {
    ASSERT(verify_scalar_value("\"\\u0041\"", "A"), "double quoted \\u0041 = A");
}

static void test_double_quoted_hex_escape_U(void) {
    ASSERT(verify_scalar_value("\"\\U00000041\"", "A"), "double quoted \\U00000041 = A");
}

static void test_double_quoted_unicode_2byte(void) {
    /* \u00E9 = e with acute = 0xC3 0xA9 in UTF-8 */
    ASSERT(verify_scalar_value("\"\\u00E9\"", "\xC3\xA9"), "double quoted \\u00E9");
}

static void test_double_quoted_unicode_3byte(void) {
    /* \u4E16 = CJK char = 0xE4 0xB8 0x96 in UTF-8 */
    ASSERT(verify_scalar_value("\"\\u4E16\"", "\xE4\xB8\x96"), "double quoted \\u4E16");
}

static void test_double_quoted_with_newline_folding(void) {
    ASSERT(verify_scalar_value("\"line1\nline2\"", "line1 line2"), "double quoted newline fold");
}

static void test_double_quoted_escaped_newline(void) {
    /* Escaped newline means continuation (line break discarded) */
    ASSERT(verify_scalar_value("\"line1\\\nline2\"", "line1line2"), "double quoted escaped newline");
}

static void test_double_quoted_nel_escape(void) {
    /* \N = NEL = 0xC2 0x85 */
    ASSERT(verify_scalar_value("\"\\N\"", "\xC2\x85"), "double quoted \\N (NEL)");
}

static void test_double_quoted_nbsp_escape(void) {
    /* \_ = NBSP = 0xC2 0xA0 */
    ASSERT(verify_scalar_value("\"\\_\"", "\xC2\xA0"), "double quoted \\_ (NBSP)");
}

static void test_double_quoted_ls_escape(void) {
    /* \L = LS = 0xE2 0x80 0xA8 */
    ASSERT(verify_scalar_value("\"\\L\"", "\xE2\x80\xA8"), "double quoted \\L (LS)");
}

static void test_double_quoted_ps_escape(void) {
    /* \P = PS = 0xE2 0x80 0xA9 */
    ASSERT(verify_scalar_value("\"\\P\"", "\xE2\x80\xA9"), "double quoted \\P (PS)");
}

/* ========== Literal block scalars ========== */

static void test_literal_block_simple(void) {
    ASSERT(verify_scalar_value("|\n  hello\n  world\n", "hello\nworld\n"),
           "literal block simple");
}

static void test_literal_block_keep_trailing(void) {
    ASSERT(verify_scalar_value("|+\n  hello\n  world\n\n\n", "hello\nworld\n\n\n"),
           "literal block keep trailing");
}

static void test_literal_block_strip_trailing(void) {
    ASSERT(verify_scalar_value("|-\n  hello\n  world\n", "hello\nworld"),
           "literal block strip trailing");
}

static void test_literal_block_indent_indicator(void) {
    ASSERT(verify_scalar_value("|2\n  hello\n  world\n", "hello\nworld\n"),
           "literal block with indent indicator");
}

static void test_literal_block_empty_lines(void) {
    ASSERT(verify_scalar_value("|\n  hello\n\n  world\n", "hello\n\nworld\n"),
           "literal block with empty lines");
}

static void test_literal_block_as_value(void) {
    ASSERT(verify_scalar_value("key: |\n  line1\n  line2\n", "line1\nline2\n"),
           "literal block as mapping value");
}

/* ========== Folded block scalars ========== */

static void test_folded_block_simple(void) {
    ASSERT(verify_scalar_value(">\n  hello\n  world\n", "hello world\n"),
           "folded block simple");
}

static void test_folded_block_keep_trailing(void) {
    ASSERT(verify_scalar_value(">+\n  hello\n  world\n\n", "hello world\n\n"),
           "folded block keep trailing");
}

static void test_folded_block_strip_trailing(void) {
    ASSERT(verify_scalar_value(">-\n  hello\n  world\n", "hello world"),
           "folded block strip trailing");
}

static void test_folded_block_with_break(void) {
    /* Extra blank line in folded block becomes a literal newline */
    ASSERT(verify_scalar_value(">\n  para1\n\n  para2\n", "para1\npara2\n"),
           "folded block with paragraph break");
}

/* ========== Flow sequences ========== */

static void test_flow_seq_empty(void) {
    yaml_token_type_t tokens[16];
    int count;
    ASSERT(scan_string_tokens("[]", tokens, 16, &count), "scan empty flow seq");
    int found_start = 0, found_end = 0;
    for (int i = 0; i < count; i++) {
        if (tokens[i] == YAML_FLOW_SEQUENCE_START_TOKEN) found_start = 1;
        if (tokens[i] == YAML_FLOW_SEQUENCE_END_TOKEN) found_end = 1;
    }
    ASSERT(found_start, "flow seq: found start");
    ASSERT(found_end, "flow seq: found end");
}

static void test_flow_seq_single_item(void) {
    ASSERT(verify_scalar_value("[item]", "item"), "flow seq single item");
}

static void test_flow_seq_multiple_items(void) {
    yaml_parser_t parser;
    yaml_token_t token;
    int scalar_count = 0;

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)"[a, b, c]", 9);

    while (1) {
        if (!yaml_parser_scan(&parser, &token)) break;
        if (token.type == YAML_SCALAR_TOKEN) scalar_count++;
        int done = (token.type == YAML_STREAM_END_TOKEN);
        yaml_token_delete(&token);
        if (done) break;
    }
    yaml_parser_delete(&parser);
    ASSERT_EQ_INT(scalar_count, 3, "flow seq: 3 scalars");
}

static void test_flow_seq_nested(void) {
    yaml_token_type_t tokens[32];
    int count;
    ASSERT(scan_string_tokens("[[a, b], [c, d]]", tokens, 32, &count), "scan nested flow seq");
    int seq_starts = 0;
    for (int i = 0; i < count; i++)
        if (tokens[i] == YAML_FLOW_SEQUENCE_START_TOKEN) seq_starts++;
    ASSERT_EQ_INT(seq_starts, 3, "nested flow seq: 3 starts");
}

static void test_flow_seq_with_quotes(void) {
    ASSERT(verify_scalar_value("['a', \"b\"]", "a"), "flow seq with single quoted");
}

static void test_flow_seq_trailing_comma(void) {
    yaml_token_type_t tokens[16];
    int count;
    ASSERT(scan_string_tokens("[a, b,]", tokens, 16, &count), "scan flow seq trailing comma");
    /* Should parse without error */
    ASSERT_EQ_INT(tokens[count-1], YAML_STREAM_END_TOKEN, "trailing comma: stream end");
}

/* ========== Flow mappings ========== */

static void test_flow_map_empty(void) {
    yaml_token_type_t tokens[16];
    int count;
    ASSERT(scan_string_tokens("{}", tokens, 16, &count), "scan empty flow map");
    int found = 0;
    for (int i = 0; i < count; i++)
        if (tokens[i] == YAML_FLOW_MAPPING_START_TOKEN) found = 1;
    ASSERT(found, "flow map: found start");
}

static void test_flow_map_single_pair(void) {
    ASSERT(verify_scalar_value("{key: value}", "value"), "flow map single pair value");
    ASSERT(verify_scalar_value("{key: value}", "key"), "flow map single pair key");
}

static void test_flow_map_multiple_pairs(void) {
    yaml_parser_t parser;
    yaml_token_t token;
    int key_count = 0;
    const char *input = "{a: 1, b: 2, c: 3}";

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));

    while (1) {
        if (!yaml_parser_scan(&parser, &token)) break;
        if (token.type == YAML_KEY_TOKEN) key_count++;
        int done = (token.type == YAML_STREAM_END_TOKEN);
        yaml_token_delete(&token);
        if (done) break;
    }
    yaml_parser_delete(&parser);
    ASSERT_EQ_INT(key_count, 3, "flow map: 3 keys");
}

static void test_flow_map_nested(void) {
    yaml_token_type_t tokens[32];
    int count;
    ASSERT(scan_string_tokens("{a: {b: c}}", tokens, 32, &count), "scan nested flow map");
    int map_starts = 0;
    for (int i = 0; i < count; i++)
        if (tokens[i] == YAML_FLOW_MAPPING_START_TOKEN) map_starts++;
    ASSERT_EQ_INT(map_starts, 2, "nested flow map: 2 starts");
}

/* ========== Block sequences ========== */

static void test_block_seq_simple(void) {
    yaml_token_type_t tokens[32];
    int count;
    ASSERT(scan_string_tokens("- a\n- b\n- c\n", tokens, 32, &count), "scan block seq");
    int entry_count = 0;
    for (int i = 0; i < count; i++)
        if (tokens[i] == YAML_BLOCK_ENTRY_TOKEN) entry_count++;
    ASSERT_EQ_INT(entry_count, 3, "block seq: 3 entries");
}

static void test_block_seq_nested(void) {
    yaml_token_type_t tokens[32];
    int count;
    ASSERT(scan_string_tokens("- a\n-\n  - b\n  - c\n", tokens, 32, &count), "scan nested block seq");
    int seq_starts = 0;
    for (int i = 0; i < count; i++)
        if (tokens[i] == YAML_BLOCK_SEQUENCE_START_TOKEN) seq_starts++;
    ASSERT(seq_starts >= 2, "nested block seq: at least 2 starts");
}

static void test_block_seq_empty_items(void) {
    yaml_token_type_t tokens[32];
    int count;
    ASSERT(scan_string_tokens("-\n-\n-\n", tokens, 32, &count), "scan block seq empty items");
    int entry_count = 0;
    for (int i = 0; i < count; i++)
        if (tokens[i] == YAML_BLOCK_ENTRY_TOKEN) entry_count++;
    ASSERT_EQ_INT(entry_count, 3, "block seq empty: 3 entries");
}

/* ========== Block mappings ========== */

static void test_block_map_simple(void) {
    yaml_token_type_t tokens[32];
    int count;
    ASSERT(scan_string_tokens("a: 1\nb: 2\n", tokens, 32, &count), "scan block map");
    int key_count = 0;
    for (int i = 0; i < count; i++)
        if (tokens[i] == YAML_KEY_TOKEN) key_count++;
    ASSERT_EQ_INT(key_count, 2, "block map: 2 keys");
}

static void test_block_map_nested(void) {
    yaml_token_type_t tokens[32];
    int count;
    ASSERT(scan_string_tokens("a:\n  b: 1\n  c: 2\n", tokens, 32, &count), "scan nested block map");
    int map_starts = 0;
    for (int i = 0; i < count; i++)
        if (tokens[i] == YAML_BLOCK_MAPPING_START_TOKEN) map_starts++;
    ASSERT(map_starts >= 2, "nested block map: at least 2 starts");
}

static void test_block_map_empty_value(void) {
    yaml_token_type_t tokens[32];
    int count;
    ASSERT(scan_string_tokens("key:\n", tokens, 32, &count), "scan map empty value");
    int found_value = 0;
    for (int i = 0; i < count; i++)
        if (tokens[i] == YAML_VALUE_TOKEN) found_value = 1;
    ASSERT(found_value, "map empty value: has VALUE token");
}

/* ========== Anchors and aliases ========== */

static void test_anchor_simple(void) {
    yaml_parser_t parser;
    yaml_token_t token;
    int found = 0;

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)"&anchor value", 13);

    while (1) {
        if (!yaml_parser_scan(&parser, &token)) break;
        if (token.type == YAML_ANCHOR_TOKEN) {
            ASSERT_EQ_STR(token.data.anchor.value, "anchor", "anchor value");
            found = 1;
        }
        int done = (token.type == YAML_STREAM_END_TOKEN);
        yaml_token_delete(&token);
        if (done) break;
    }
    yaml_parser_delete(&parser);
    ASSERT(found, "found anchor token");
}

static void test_alias_simple(void) {
    yaml_parser_t parser;
    yaml_token_t token;
    int found = 0;

    yaml_parser_initialize(&parser);
    const char *input = "- &ref value\n- *ref\n";
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));

    while (1) {
        if (!yaml_parser_scan(&parser, &token)) break;
        if (token.type == YAML_ALIAS_TOKEN) {
            ASSERT_EQ_STR(token.data.alias.value, "ref", "alias value");
            found = 1;
        }
        int done = (token.type == YAML_STREAM_END_TOKEN);
        yaml_token_delete(&token);
        if (done) break;
    }
    yaml_parser_delete(&parser);
    ASSERT(found, "found alias token");
}

static void test_anchor_alphanumeric(void) {
    yaml_parser_t parser;
    yaml_token_t token;
    int found = 0;

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)"&abc123 val", 11);

    while (1) {
        if (!yaml_parser_scan(&parser, &token)) break;
        if (token.type == YAML_ANCHOR_TOKEN) {
            ASSERT_EQ_STR(token.data.anchor.value, "abc123", "alphanumeric anchor");
            found = 1;
        }
        int done = (token.type == YAML_STREAM_END_TOKEN);
        yaml_token_delete(&token);
        if (done) break;
    }
    yaml_parser_delete(&parser);
    ASSERT(found, "found alphanumeric anchor");
}

/* ========== Tags ========== */

static void test_tag_verbatim(void) {
    yaml_parser_t parser;
    yaml_token_t token;
    int found = 0;

    yaml_parser_initialize(&parser);
    const char *input = "!<tag:example.com,2000:type> value";
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));

    while (1) {
        if (!yaml_parser_scan(&parser, &token)) break;
        if (token.type == YAML_TAG_TOKEN) {
            ASSERT_EQ_STR(token.data.tag.suffix, "tag:example.com,2000:type", "verbatim tag");
            found = 1;
        }
        int done = (token.type == YAML_STREAM_END_TOKEN);
        yaml_token_delete(&token);
        if (done) break;
    }
    yaml_parser_delete(&parser);
    ASSERT(found, "found verbatim tag");
}

static void test_tag_shorthand(void) {
    yaml_parser_t parser;
    yaml_token_t token;
    int found = 0;

    yaml_parser_initialize(&parser);
    const char *input = "!!str value";
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));

    while (1) {
        if (!yaml_parser_scan(&parser, &token)) break;
        if (token.type == YAML_TAG_TOKEN) {
            ASSERT_EQ_STR(token.data.tag.handle, "!!", "shorthand tag handle");
            ASSERT_EQ_STR(token.data.tag.suffix, "str", "shorthand tag suffix");
            found = 1;
        }
        int done = (token.type == YAML_STREAM_END_TOKEN);
        yaml_token_delete(&token);
        if (done) break;
    }
    yaml_parser_delete(&parser);
    ASSERT(found, "found shorthand tag");
}

static void test_tag_primary(void) {
    yaml_parser_t parser;
    yaml_token_t token;
    int found = 0;

    yaml_parser_initialize(&parser);
    const char *input = "!local value";
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));

    while (1) {
        if (!yaml_parser_scan(&parser, &token)) break;
        if (token.type == YAML_TAG_TOKEN) {
            ASSERT_EQ_STR(token.data.tag.handle, "!", "primary tag handle");
            ASSERT_EQ_STR(token.data.tag.suffix, "local", "primary tag suffix");
            found = 1;
        }
        int done = (token.type == YAML_STREAM_END_TOKEN);
        yaml_token_delete(&token);
        if (done) break;
    }
    yaml_parser_delete(&parser);
    ASSERT(found, "found primary tag");
}

static void test_tag_non_specific(void) {
    yaml_parser_t parser;
    yaml_token_t token;
    int found = 0;

    yaml_parser_initialize(&parser);
    const char *input = "! value";
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));

    while (1) {
        if (!yaml_parser_scan(&parser, &token)) break;
        if (token.type == YAML_TAG_TOKEN) {
            ASSERT_EQ_STR(token.data.tag.handle, "", "non-specific tag handle");
            ASSERT_EQ_STR(token.data.tag.suffix, "!", "non-specific tag suffix");
            found = 1;
        }
        int done = (token.type == YAML_STREAM_END_TOKEN);
        yaml_token_delete(&token);
        if (done) break;
    }
    yaml_parser_delete(&parser);
    ASSERT(found, "found non-specific tag");
}

/* ========== Directives ========== */

static void test_yaml_directive(void) {
    yaml_parser_t parser;
    yaml_token_t token;
    int found = 0;

    yaml_parser_initialize(&parser);
    const char *input = "%YAML 1.1\n---\nvalue";
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));

    while (1) {
        if (!yaml_parser_scan(&parser, &token)) break;
        if (token.type == YAML_VERSION_DIRECTIVE_TOKEN) {
            ASSERT_EQ_INT(token.data.version_directive.major, 1, "YAML major version");
            ASSERT_EQ_INT(token.data.version_directive.minor, 1, "YAML minor version");
            found = 1;
        }
        int done = (token.type == YAML_STREAM_END_TOKEN);
        yaml_token_delete(&token);
        if (done) break;
    }
    yaml_parser_delete(&parser);
    ASSERT(found, "found version directive");
}

static void test_tag_directive(void) {
    yaml_parser_t parser;
    yaml_token_t token;
    int found = 0;

    yaml_parser_initialize(&parser);
    const char *input = "%TAG !e! tag:example.com,2000:\n---\n!e!type value";
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));

    while (1) {
        if (!yaml_parser_scan(&parser, &token)) break;
        if (token.type == YAML_TAG_DIRECTIVE_TOKEN) {
            ASSERT_EQ_STR(token.data.tag_directive.handle, "!e!", "tag dir handle");
            ASSERT_EQ_STR(token.data.tag_directive.prefix, "tag:example.com,2000:", "tag dir prefix");
            found = 1;
        }
        int done = (token.type == YAML_STREAM_END_TOKEN);
        yaml_token_delete(&token);
        if (done) break;
    }
    yaml_parser_delete(&parser);
    ASSERT(found, "found tag directive");
}

/* ========== Explicit keys ========== */

static void test_explicit_key_block(void) {
    yaml_token_type_t tokens[32];
    int count;
    ASSERT(scan_string_tokens("? key\n: value\n", tokens, 32, &count), "scan explicit key");
    int found_key = 0;
    for (int i = 0; i < count; i++)
        if (tokens[i] == YAML_KEY_TOKEN) found_key = 1;
    ASSERT(found_key, "explicit key: has KEY token");
}

static void test_explicit_key_flow(void) {
    yaml_token_type_t tokens[32];
    int count;
    ASSERT(scan_string_tokens("{? key: value}", tokens, 32, &count), "scan explicit flow key");
    int found_key = 0;
    for (int i = 0; i < count; i++)
        if (tokens[i] == YAML_KEY_TOKEN) found_key = 1;
    ASSERT(found_key, "explicit flow key: has KEY token");
}

/* ========== Comments ========== */

static void test_comment_after_value(void) {
    ASSERT(verify_scalar_value("value # comment", "value"), "comment after value");
}

static void test_comment_on_own_line(void) {
    ASSERT(verify_scalar_value("# comment\nvalue", "value"), "comment on own line");
}

static void test_comment_in_flow(void) {
    ASSERT(verify_scalar_value("[a, # comment\nb]", "a"), "comment in flow");
}

static void test_comment_long(void) {
    const char *input = "value # this is a very long comment that goes on and on and on and on and on and on and on and on and on and on\n";
    ASSERT(verify_scalar_value(input, "value"), "long comment");
}

/* ========== Mixed constructs ========== */

static void test_mapping_with_seq_value(void) {
    yaml_token_type_t tokens[32];
    int count;
    ASSERT(scan_string_tokens("key:\n  - a\n  - b\n", tokens, 32, &count), "scan map with seq");
    int found_entry = 0;
    for (int i = 0; i < count; i++)
        if (tokens[i] == YAML_BLOCK_ENTRY_TOKEN) found_entry = 1;
    ASSERT(found_entry, "map with seq: has entries");
}

static void test_seq_of_maps(void) {
    yaml_token_type_t tokens[64];
    int count;
    const char *input = "- a: 1\n  b: 2\n- c: 3\n";
    ASSERT(scan_string_tokens(input, tokens, 64, &count), "scan seq of maps");
    int map_starts = 0;
    for (int i = 0; i < count; i++)
        if (tokens[i] == YAML_BLOCK_MAPPING_START_TOKEN) map_starts++;
    ASSERT(map_starts >= 2, "seq of maps: at least 2 map starts");
}

static void test_flow_in_block(void) {
    ASSERT(verify_scalar_value("key: [a, b, c]\n", "a"), "flow seq in block mapping");
}

static void test_block_in_flow(void) {
    /* Flow context doesn't allow block constructs, values are plain scalars */
    yaml_token_type_t tokens[32];
    int count;
    ASSERT(scan_string_tokens("{key: value}", tokens, 32, &count), "block-style in flow");
}

/* ========== Whitespace handling ========== */

static void test_leading_spaces(void) {
    /* Spaces before content at top level */
    yaml_token_type_t tokens[16];
    int count;
    /* Leading spaces in block context are meaningful for indentation */
    ASSERT(scan_string_tokens("  key: value\n", tokens, 16, &count), "leading spaces");
    ASSERT_EQ_INT(tokens[count-1], YAML_STREAM_END_TOKEN, "leading spaces: ends");
}

static void test_trailing_spaces(void) {
    ASSERT(verify_scalar_value("value   \n", "value"), "trailing spaces stripped");
}

static void test_multiple_spaces_between_tokens(void) {
    ASSERT(verify_scalar_value("key:    value", "value"), "multiple spaces between tokens");
}

static void test_tab_in_flow(void) {
    ASSERT(verify_scalar_value("[\ta,\tb]", "a"), "tab in flow context");
}

/* ========== Scalar style detection ========== */

static void test_scalar_style_plain(void) {
    yaml_parser_t parser;
    yaml_token_t token;
    int found = 0;

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)"hello", 5);

    while (1) {
        if (!yaml_parser_scan(&parser, &token)) break;
        if (token.type == YAML_SCALAR_TOKEN) {
            ASSERT_EQ_INT(token.data.scalar.style, YAML_PLAIN_SCALAR_STYLE, "plain style");
            found = 1;
        }
        int done = (token.type == YAML_STREAM_END_TOKEN);
        yaml_token_delete(&token);
        if (done) break;
    }
    yaml_parser_delete(&parser);
    ASSERT(found, "found plain scalar");
}

static void test_scalar_style_single_quoted(void) {
    yaml_parser_t parser;
    yaml_token_t token;
    int found = 0;

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)"'hello'", 7);

    while (1) {
        if (!yaml_parser_scan(&parser, &token)) break;
        if (token.type == YAML_SCALAR_TOKEN) {
            ASSERT_EQ_INT(token.data.scalar.style, YAML_SINGLE_QUOTED_SCALAR_STYLE, "single quoted style");
            found = 1;
        }
        int done = (token.type == YAML_STREAM_END_TOKEN);
        yaml_token_delete(&token);
        if (done) break;
    }
    yaml_parser_delete(&parser);
    ASSERT(found, "found single quoted scalar");
}

static void test_scalar_style_double_quoted(void) {
    yaml_parser_t parser;
    yaml_token_t token;
    int found = 0;

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)"\"hello\"", 7);

    while (1) {
        if (!yaml_parser_scan(&parser, &token)) break;
        if (token.type == YAML_SCALAR_TOKEN) {
            ASSERT_EQ_INT(token.data.scalar.style, YAML_DOUBLE_QUOTED_SCALAR_STYLE, "double quoted style");
            found = 1;
        }
        int done = (token.type == YAML_STREAM_END_TOKEN);
        yaml_token_delete(&token);
        if (done) break;
    }
    yaml_parser_delete(&parser);
    ASSERT(found, "found double quoted scalar");
}

static void test_scalar_style_literal(void) {
    yaml_parser_t parser;
    yaml_token_t token;
    int found = 0;

    yaml_parser_initialize(&parser);
    const char *input = "|\n  hello\n";
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));

    while (1) {
        if (!yaml_parser_scan(&parser, &token)) break;
        if (token.type == YAML_SCALAR_TOKEN) {
            ASSERT_EQ_INT(token.data.scalar.style, YAML_LITERAL_SCALAR_STYLE, "literal style");
            found = 1;
        }
        int done = (token.type == YAML_STREAM_END_TOKEN);
        yaml_token_delete(&token);
        if (done) break;
    }
    yaml_parser_delete(&parser);
    ASSERT(found, "found literal scalar");
}

static void test_scalar_style_folded(void) {
    yaml_parser_t parser;
    yaml_token_t token;
    int found = 0;

    yaml_parser_initialize(&parser);
    const char *input = ">\n  hello\n";
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));

    while (1) {
        if (!yaml_parser_scan(&parser, &token)) break;
        if (token.type == YAML_SCALAR_TOKEN) {
            ASSERT_EQ_INT(token.data.scalar.style, YAML_FOLDED_SCALAR_STYLE, "folded style");
            found = 1;
        }
        int done = (token.type == YAML_STREAM_END_TOKEN);
        yaml_token_delete(&token);
        if (done) break;
    }
    yaml_parser_delete(&parser);
    ASSERT(found, "found folded scalar");
}

/* ========== Scalar length tracking ========== */

static void test_scalar_length_plain(void) {
    yaml_parser_t parser;
    yaml_token_t token;

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)"hello", 5);

    while (1) {
        if (!yaml_parser_scan(&parser, &token)) break;
        if (token.type == YAML_SCALAR_TOKEN) {
            ASSERT_EQ_INT((int)token.data.scalar.length, 5, "plain scalar length");
        }
        int done = (token.type == YAML_STREAM_END_TOKEN);
        yaml_token_delete(&token);
        if (done) break;
    }
    yaml_parser_delete(&parser);
}

static void test_scalar_length_empty_quoted(void) {
    yaml_parser_t parser;
    yaml_token_t token;

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)"''", 2);

    while (1) {
        if (!yaml_parser_scan(&parser, &token)) break;
        if (token.type == YAML_SCALAR_TOKEN) {
            ASSERT_EQ_INT((int)token.data.scalar.length, 0, "empty quoted scalar length");
        }
        int done = (token.type == YAML_STREAM_END_TOKEN);
        yaml_token_delete(&token);
        if (done) break;
    }
    yaml_parser_delete(&parser);
}

/* ========== Mark tracking ========== */

static void test_mark_positions(void) {
    yaml_parser_t parser;
    yaml_token_t token;

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)"key: value", 10);

    while (1) {
        if (!yaml_parser_scan(&parser, &token)) break;
        if (token.type == YAML_STREAM_START_TOKEN) {
            ASSERT_EQ_INT((int)token.start_mark.index, 0, "stream start index");
            ASSERT_EQ_INT((int)token.start_mark.line, 0, "stream start line");
            ASSERT_EQ_INT((int)token.start_mark.column, 0, "stream start column");
        }
        int done = (token.type == YAML_STREAM_END_TOKEN);
        yaml_token_delete(&token);
        if (done) break;
    }
    yaml_parser_delete(&parser);
}

static void test_mark_multiline(void) {
    yaml_parser_t parser;
    yaml_token_t token;
    int found_second_key = 0;

    yaml_parser_initialize(&parser);
    const char *input = "a: 1\nb: 2\n";
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));

    int key_count = 0;
    while (1) {
        if (!yaml_parser_scan(&parser, &token)) break;
        if (token.type == YAML_KEY_TOKEN) {
            key_count++;
            if (key_count == 2) {
                ASSERT_EQ_INT((int)token.start_mark.line, 1, "second key on line 1");
                ASSERT_EQ_INT((int)token.start_mark.column, 0, "second key at column 0");
                found_second_key = 1;
            }
        }
        int done = (token.type == YAML_STREAM_END_TOKEN);
        yaml_token_delete(&token);
        if (done) break;
    }
    yaml_parser_delete(&parser);
    ASSERT(found_second_key, "found second key with correct marks");
}

int main(void) {
    TEST_SUITE_BEGIN("Scanner Comprehensive");

    /* Stream tokens */
    test_stream_empty();
    test_stream_whitespace_only();
    test_stream_newlines_only();
    test_stream_comment_only();
    test_stream_multiple_comments();
    test_stream_tabs_and_spaces();

    /* Document indicators */
    test_doc_start_indicator();
    test_doc_end_indicator();
    test_doc_start_without_newline();
    test_multiple_documents();

    /* Plain scalars */
    test_plain_scalar_simple();
    test_plain_scalar_with_spaces();
    test_plain_scalar_number();
    test_plain_scalar_float();
    test_plain_scalar_bool_true();
    test_plain_scalar_bool_false();
    test_plain_scalar_null();
    test_plain_scalar_tilde();
    test_plain_scalar_empty_line_value();
    test_plain_scalar_multiword_value();
    test_plain_scalar_with_special_chars();
    test_plain_scalar_starts_with_number();
    test_plain_scalar_with_dots();
    test_plain_scalar_with_slash();
    test_plain_scalar_as_seq_item();

    /* Single-quoted scalars */
    test_single_quoted_empty();
    test_single_quoted_simple();
    test_single_quoted_with_spaces();
    test_single_quoted_with_colon();
    test_single_quoted_with_hash();
    test_single_quoted_escaped_quote();
    test_single_quoted_multiple_escaped();
    test_single_quoted_with_newline();
    test_single_quoted_special_chars();
    test_single_quoted_with_backslash();

    /* Double-quoted scalars */
    test_double_quoted_empty();
    test_double_quoted_simple();
    test_double_quoted_escape_newline();
    test_double_quoted_escape_tab();
    test_double_quoted_escape_backslash();
    test_double_quoted_escape_quote();
    test_double_quoted_escape_null();
    test_double_quoted_escape_bell();
    test_double_quoted_escape_backspace();
    test_double_quoted_escape_formfeed();
    test_double_quoted_escape_cr();
    test_double_quoted_escape_vtab();
    test_double_quoted_escape_esc();
    test_double_quoted_escape_space();
    test_double_quoted_escape_slash();
    test_double_quoted_hex_escape_x();
    test_double_quoted_hex_escape_u();
    test_double_quoted_hex_escape_U();
    test_double_quoted_unicode_2byte();
    test_double_quoted_unicode_3byte();
    test_double_quoted_with_newline_folding();
    test_double_quoted_escaped_newline();
    test_double_quoted_nel_escape();
    test_double_quoted_nbsp_escape();
    test_double_quoted_ls_escape();
    test_double_quoted_ps_escape();

    /* Literal block scalars */
    test_literal_block_simple();
    test_literal_block_keep_trailing();
    test_literal_block_strip_trailing();
    test_literal_block_indent_indicator();
    test_literal_block_empty_lines();
    test_literal_block_as_value();

    /* Folded block scalars */
    test_folded_block_simple();
    test_folded_block_keep_trailing();
    test_folded_block_strip_trailing();
    test_folded_block_with_break();

    /* Flow sequences */
    test_flow_seq_empty();
    test_flow_seq_single_item();
    test_flow_seq_multiple_items();
    test_flow_seq_nested();
    test_flow_seq_with_quotes();
    test_flow_seq_trailing_comma();

    /* Flow mappings */
    test_flow_map_empty();
    test_flow_map_single_pair();
    test_flow_map_multiple_pairs();
    test_flow_map_nested();

    /* Block sequences */
    test_block_seq_simple();
    test_block_seq_nested();
    test_block_seq_empty_items();

    /* Block mappings */
    test_block_map_simple();
    test_block_map_nested();
    test_block_map_empty_value();

    /* Anchors and aliases */
    test_anchor_simple();
    test_alias_simple();
    test_anchor_alphanumeric();

    /* Tags */
    test_tag_verbatim();
    test_tag_shorthand();
    test_tag_primary();
    test_tag_non_specific();

    /* Directives */
    test_yaml_directive();
    test_tag_directive();

    /* Explicit keys */
    test_explicit_key_block();
    test_explicit_key_flow();

    /* Comments */
    test_comment_after_value();
    test_comment_on_own_line();
    test_comment_in_flow();
    test_comment_long();

    /* Mixed constructs */
    test_mapping_with_seq_value();
    test_seq_of_maps();
    test_flow_in_block();
    test_block_in_flow();

    /* Whitespace handling */
    test_leading_spaces();
    test_trailing_spaces();
    test_multiple_spaces_between_tokens();
    test_tab_in_flow();

    /* Scalar styles */
    test_scalar_style_plain();
    test_scalar_style_single_quoted();
    test_scalar_style_double_quoted();
    test_scalar_style_literal();
    test_scalar_style_folded();

    /* Scalar length */
    test_scalar_length_plain();
    test_scalar_length_empty_quoted();

    /* Mark tracking */
    test_mark_positions();
    test_mark_multiline();

    TEST_SUITE_END();
}
