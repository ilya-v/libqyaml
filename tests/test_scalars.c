#include "test_helper.h"

/* Helper to parse and extract the first non-key scalar from a "key: VALUE" mapping */
static int extract_mapping_value(const char *yaml, char *buf, size_t bufsize,
                                  yaml_scalar_style_t *style) {
    yaml_parser_t parser;
    yaml_event_t event;
    int found = 0;
    int past_key = 0;

    if (!yaml_parser_initialize(&parser)) return 0;
    yaml_parser_set_input_string(&parser, (const unsigned char *)yaml, strlen(yaml));

    while (1) {
        if (!yaml_parser_parse(&parser, &event)) break;
        if (event.type == YAML_SCALAR_EVENT) {
            if (past_key) {
                size_t len = event.data.scalar.length < bufsize - 1 ?
                             event.data.scalar.length : bufsize - 1;
                memcpy(buf, event.data.scalar.value, len);
                buf[len] = '\0';
                if (style) *style = event.data.scalar.style;
                found = 1;
                yaml_event_delete(&event);
                break;
            }
            past_key = 1;
        }
        if (event.type == YAML_STREAM_END_EVENT) {
            yaml_event_delete(&event);
            break;
        }
        yaml_event_delete(&event);
    }

    yaml_parser_delete(&parser);
    return found;
}

/* Helper to parse and extract first scalar value from input */
static int extract_scalar(const char *yaml, char *buf, size_t bufsize,
                           yaml_scalar_style_t *style) {
    yaml_parser_t parser;
    yaml_event_t event;
    int found = 0;

    if (!yaml_parser_initialize(&parser)) return 0;
    yaml_parser_set_input_string(&parser, (const unsigned char *)yaml, strlen(yaml));

    while (1) {
        if (!yaml_parser_parse(&parser, &event)) break;
        if (event.type == YAML_SCALAR_EVENT) {
            size_t len = event.data.scalar.length < bufsize - 1 ?
                         event.data.scalar.length : bufsize - 1;
            memcpy(buf, event.data.scalar.value, len);
            buf[len] = '\0';
            if (style) *style = event.data.scalar.style;
            found = 1;
            yaml_event_delete(&event);
            break;
        }
        if (event.type == YAML_STREAM_END_EVENT) {
            yaml_event_delete(&event);
            break;
        }
        yaml_event_delete(&event);
    }

    yaml_parser_delete(&parser);
    return found;
}

/* Test plain scalars */
static void test_plain_scalars(void) {
    char buf[256];
    yaml_scalar_style_t style;

    ASSERT(extract_scalar("hello", buf, sizeof(buf), &style), "plain: hello");
    ASSERT_EQ_STR(buf, "hello", "plain: value");
    ASSERT_EQ_INT(style, YAML_PLAIN_SCALAR_STYLE, "plain: style");

    ASSERT(extract_scalar("hello world", buf, sizeof(buf), &style), "plain: space");
    ASSERT_EQ_STR(buf, "hello world", "plain: space value");

    ASSERT(extract_scalar("123", buf, sizeof(buf), NULL), "plain: number");
    ASSERT_EQ_STR(buf, "123", "plain: number value");

    ASSERT(extract_scalar("true", buf, sizeof(buf), NULL), "plain: bool");
    ASSERT_EQ_STR(buf, "true", "plain: bool value");

    ASSERT(extract_scalar("null", buf, sizeof(buf), NULL), "plain: null");
    ASSERT_EQ_STR(buf, "null", "plain: null value");

    ASSERT(extract_scalar("3.14", buf, sizeof(buf), NULL), "plain: float");
    ASSERT_EQ_STR(buf, "3.14", "plain: float value");
}

/* Test single-quoted scalars */
static void test_single_quoted(void) {
    char buf[256];
    yaml_scalar_style_t style;

    ASSERT(extract_scalar("'hello'", buf, sizeof(buf), &style), "sq: simple");
    ASSERT_EQ_STR(buf, "hello", "sq: value");
    ASSERT_EQ_INT(style, YAML_SINGLE_QUOTED_SCALAR_STYLE, "sq: style");

    /* Escaped single quote */
    ASSERT(extract_scalar("'it''s'", buf, sizeof(buf), NULL), "sq: escape");
    ASSERT_EQ_STR(buf, "it's", "sq: escape value");

    /* Empty single-quoted */
    ASSERT(extract_scalar("''", buf, sizeof(buf), NULL), "sq: empty");
    ASSERT_EQ_STR(buf, "", "sq: empty value");

    /* Single-quoted with special chars */
    ASSERT(extract_scalar("'key: value'", buf, sizeof(buf), NULL), "sq: special");
    ASSERT_EQ_STR(buf, "key: value", "sq: special value");

    /* Multi-line single-quoted */
    ASSERT(extract_scalar("'line1\nline2'", buf, sizeof(buf), NULL), "sq: multiline");
    ASSERT_EQ_STR(buf, "line1 line2", "sq: multiline value");
}

/* Test double-quoted scalars */
static void test_double_quoted(void) {
    char buf[256];
    yaml_scalar_style_t style;

    ASSERT(extract_scalar("\"hello\"", buf, sizeof(buf), &style), "dq: simple");
    ASSERT_EQ_STR(buf, "hello", "dq: value");
    ASSERT_EQ_INT(style, YAML_DOUBLE_QUOTED_SCALAR_STYLE, "dq: style");

    /* Escape sequences */
    ASSERT(extract_scalar("\"hello\\nworld\"", buf, sizeof(buf), NULL), "dq: newline");
    ASSERT_EQ_STR(buf, "hello\nworld", "dq: newline value");

    ASSERT(extract_scalar("\"hello\\tworld\"", buf, sizeof(buf), NULL), "dq: tab");
    ASSERT_EQ_STR(buf, "hello\tworld", "dq: tab value");

    ASSERT(extract_scalar("\"hello\\\\world\"", buf, sizeof(buf), NULL), "dq: backslash");
    ASSERT_EQ_STR(buf, "hello\\world", "dq: backslash value");

    ASSERT(extract_scalar("\"hello\\\"world\"", buf, sizeof(buf), NULL), "dq: quote");
    ASSERT_EQ_STR(buf, "hello\"world", "dq: quote value");

    /* Empty */
    ASSERT(extract_scalar("\"\"", buf, sizeof(buf), NULL), "dq: empty");
    ASSERT_EQ_STR(buf, "", "dq: empty value");

    /* Unicode escapes */
    ASSERT(extract_scalar("\"\\x41\"", buf, sizeof(buf), NULL), "dq: hex escape");
    ASSERT_EQ_STR(buf, "A", "dq: hex escape value");

    /* Null character escape */
    ASSERT(extract_scalar("\"\\0\"", buf, sizeof(buf), NULL), "dq: null escape");
    /* The value should be a null byte - length should be 1 */

    /* Various escapes */
    ASSERT(extract_scalar("\"\\a\"", buf, sizeof(buf), NULL), "dq: bell");
    ASSERT_EQ_INT(buf[0], '\a', "dq: bell value");

    ASSERT(extract_scalar("\"\\b\"", buf, sizeof(buf), NULL), "dq: backspace");
    ASSERT_EQ_INT(buf[0], '\b', "dq: backspace value");

    ASSERT(extract_scalar("\"\\r\"", buf, sizeof(buf), NULL), "dq: cr");
    ASSERT_EQ_INT(buf[0], '\r', "dq: cr value");
}

/* Test literal block scalars */
static void test_literal_scalars(void) {
    char buf[1024];
    yaml_scalar_style_t style;

    /* Basic literal */
    ASSERT(extract_mapping_value("data: |\n  line1\n  line2\n", buf, sizeof(buf), &style),
           "literal: basic");
    ASSERT_EQ_STR(buf, "line1\nline2\n", "literal: basic value");
    ASSERT_EQ_INT(style, YAML_LITERAL_SCALAR_STYLE, "literal: style");

    /* Literal with strip */
    ASSERT(extract_mapping_value("data: |-\n  line1\n  line2\n", buf, sizeof(buf), NULL),
           "literal: strip");
    ASSERT_EQ_STR(buf, "line1\nline2", "literal: strip value");

    /* Literal with keep */
    ASSERT(extract_mapping_value("data: |+\n  line1\n  line2\n\n", buf, sizeof(buf), NULL),
           "literal: keep");
    ASSERT_EQ_STR(buf, "line1\nline2\n\n", "literal: keep value");

    /* Literal with explicit indent */
    ASSERT(extract_mapping_value("data: |2\n  line1\n  line2\n", buf, sizeof(buf), NULL),
           "literal: indent");
    ASSERT_EQ_STR(buf, "line1\nline2\n", "literal: indent value");
}

/* Test folded block scalars */
static void test_folded_scalars(void) {
    char buf[1024];
    yaml_scalar_style_t style;

    /* Basic folded */
    ASSERT(extract_mapping_value("data: >\n  line1\n  line2\n", buf, sizeof(buf), &style),
           "folded: basic");
    ASSERT_EQ_STR(buf, "line1 line2\n", "folded: basic value");
    ASSERT_EQ_INT(style, YAML_FOLDED_SCALAR_STYLE, "folded: style");

    /* Folded with strip */
    ASSERT(extract_mapping_value("data: >-\n  line1\n  line2\n", buf, sizeof(buf), NULL),
           "folded: strip");
    ASSERT_EQ_STR(buf, "line1 line2", "folded: strip value");
}

/* Test scalars in different contexts */
static void test_scalar_contexts(void) {
    char buf[256];

    /* Scalar as mapping key */
    {
        yaml_parser_t parser;
        yaml_event_t event;
        const char *yaml = "key: value\n";

        ASSERT(yaml_parser_initialize(&parser), "ctx: map init");
        yaml_parser_set_input_string(&parser, (const unsigned char *)yaml, strlen(yaml));

        int scalar_count = 0;
        while (1) {
            ASSERT(yaml_parser_parse(&parser, &event), "ctx: parse");
            if (event.type == YAML_SCALAR_EVENT) {
                if (scalar_count == 0) {
                    ASSERT_EQ_STR((const char *)event.data.scalar.value, "key", "ctx: key");
                } else {
                    ASSERT_EQ_STR((const char *)event.data.scalar.value, "value", "ctx: value");
                }
                scalar_count++;
            }
            if (event.type == YAML_STREAM_END_EVENT) {
                yaml_event_delete(&event);
                break;
            }
            yaml_event_delete(&event);
        }
        ASSERT_EQ_INT(scalar_count, 2, "ctx: 2 scalars");
        yaml_parser_delete(&parser);
    }

    /* Scalar in flow sequence */
    ASSERT(extract_scalar("[hello, world]", buf, sizeof(buf), NULL), "ctx: flow seq");
    ASSERT_EQ_STR(buf, "hello", "ctx: flow seq first");

    /* Scalar in flow mapping */
    ASSERT(extract_scalar("{key: val}", buf, sizeof(buf), NULL), "ctx: flow map");
    ASSERT_EQ_STR(buf, "key", "ctx: flow map key");
}

/* Test special scalar values */
static void test_special_values(void) {
    char buf[256];

    /* Various YAML special values (should be plain scalars) */
    ASSERT(extract_scalar("~", buf, sizeof(buf), NULL), "special: tilde");
    ASSERT_EQ_STR(buf, "~", "special: tilde value");

    ASSERT(extract_scalar(".inf", buf, sizeof(buf), NULL), "special: inf");
    ASSERT_EQ_STR(buf, ".inf", "special: inf value");

    ASSERT(extract_scalar("-.inf", buf, sizeof(buf), NULL), "special: neg inf");
    ASSERT_EQ_STR(buf, "-.inf", "special: neg inf value");

    ASSERT(extract_scalar(".nan", buf, sizeof(buf), NULL), "special: nan");
    ASSERT_EQ_STR(buf, ".nan", "special: nan value");
}

/* Test scalar with various whitespace */
static void test_whitespace_handling(void) {
    char buf[256];

    /* Trailing spaces in quoted strings */
    ASSERT(extract_scalar("\"  hello  \"", buf, sizeof(buf), NULL), "ws: quoted spaces");
    ASSERT_EQ_STR(buf, "  hello  ", "ws: preserved in quoted");

    /* Leading/trailing in plain scalars - depends on context */
    ASSERT(extract_scalar("hello", buf, sizeof(buf), NULL), "ws: plain");
    ASSERT_EQ_STR(buf, "hello", "ws: plain no extra");
}

/* Test multi-byte UTF8 scalar values */
static void test_utf8_scalars(void) {
    char buf[256];

    /* Simple UTF8 */
    ASSERT(extract_scalar("\xc3\xa9", buf, sizeof(buf), NULL), "utf8: e-acute");
    ASSERT_EQ_STR(buf, "\xc3\xa9", "utf8: e-acute value");

    /* CJK character */
    ASSERT(extract_scalar("\xe4\xb8\xad", buf, sizeof(buf), NULL), "utf8: cjk");
    ASSERT_EQ_STR(buf, "\xe4\xb8\xad", "utf8: cjk value");

    /* Emoji (4-byte) */
    ASSERT(extract_scalar("\xf0\x9f\x98\x80", buf, sizeof(buf), NULL), "utf8: emoji");
    ASSERT_EQ_STR(buf, "\xf0\x9f\x98\x80", "utf8: emoji value");
}

/* Test very long scalars */
static void test_long_scalars(void) {
    /* Build a long plain scalar */
    char input[8192];
    memset(input, 'a', 4000);
    input[4000] = '\0';

    char buf[8192];
    ASSERT(extract_scalar(input, buf, sizeof(buf), NULL), "long: 4000 chars");
    ASSERT_EQ_INT((int)strlen(buf), 4000, "long: length 4000");

    /* Long double-quoted scalar */
    char quoted[8192];
    quoted[0] = '"';
    memset(quoted + 1, 'b', 4000);
    quoted[4001] = '"';
    quoted[4002] = '\0';

    ASSERT(extract_scalar(quoted, buf, sizeof(buf), NULL), "long: quoted 4000");
    ASSERT_EQ_INT((int)strlen(buf), 4000, "long: quoted length 4000");
}

/* ==================================================================
 * Extended scalar tests
 * ================================================================== */

static void test_escape_sequences_comprehensive(void) {
    char buf[256];

    /* All single-char escapes */
    ASSERT(extract_scalar("\"\\0\"", buf, sizeof(buf), NULL), "esc: null");
    ASSERT_EQ_INT((unsigned char)buf[0], 0, "esc: null value");

    ASSERT(extract_scalar("\"\\a\"", buf, sizeof(buf), NULL), "esc: bell");
    ASSERT_EQ_INT(buf[0], '\a', "esc: bell value");

    ASSERT(extract_scalar("\"\\b\"", buf, sizeof(buf), NULL), "esc: backspace");
    ASSERT_EQ_INT(buf[0], '\b', "esc: backspace value");

    ASSERT(extract_scalar("\"\\t\"", buf, sizeof(buf), NULL), "esc: tab");
    ASSERT_EQ_INT(buf[0], '\t', "esc: tab value");

    ASSERT(extract_scalar("\"\\n\"", buf, sizeof(buf), NULL), "esc: newline");
    ASSERT_EQ_INT(buf[0], '\n', "esc: newline value");

    ASSERT(extract_scalar("\"\\v\"", buf, sizeof(buf), NULL), "esc: vtab");
    ASSERT_EQ_INT(buf[0], '\v', "esc: vtab value");

    ASSERT(extract_scalar("\"\\f\"", buf, sizeof(buf), NULL), "esc: formfeed");
    ASSERT_EQ_INT(buf[0], '\f', "esc: formfeed value");

    ASSERT(extract_scalar("\"\\r\"", buf, sizeof(buf), NULL), "esc: cr");
    ASSERT_EQ_INT(buf[0], '\r', "esc: cr value");

    ASSERT(extract_scalar("\"\\e\"", buf, sizeof(buf), NULL), "esc: escape");
    ASSERT_EQ_INT(buf[0], 0x1B, "esc: escape value");

    ASSERT(extract_scalar("\"\\\\\"", buf, sizeof(buf), NULL), "esc: backslash");
    ASSERT_EQ_INT(buf[0], '\\', "esc: backslash value");

    ASSERT(extract_scalar("\"\\\"\"", buf, sizeof(buf), NULL), "esc: dquote");
    ASSERT_EQ_INT(buf[0], '"', "esc: dquote value");

    ASSERT(extract_scalar("\"\\/\"", buf, sizeof(buf), NULL), "esc: slash");
    ASSERT_EQ_INT(buf[0], '/', "esc: slash value");

    ASSERT(extract_scalar("\" \"", buf, sizeof(buf), NULL), "esc: space");
    ASSERT_EQ_INT(buf[0], ' ', "esc: space value");
}

static void test_hex_unicode_escapes(void) {
    char buf[256];

    /* \\xNN */
    ASSERT(extract_scalar("\"\\x41\"", buf, sizeof(buf), NULL), "hex: 41");
    ASSERT_EQ_STR(buf, "A", "hex: A");

    ASSERT(extract_scalar("\"\\x61\"", buf, sizeof(buf), NULL), "hex: 61");
    ASSERT_EQ_STR(buf, "a", "hex: a");

    /* \\uNNNN */
    ASSERT(extract_scalar("\"\\u0041\"", buf, sizeof(buf), NULL), "u4: 0041");
    ASSERT_EQ_STR(buf, "A", "u4: A");

    ASSERT(extract_scalar("\"\\u00E9\"", buf, sizeof(buf), NULL), "u4: 00E9");
    ASSERT_EQ_STR(buf, "\xc3\xa9", "u4: e-acute");

    /* \\UNNNNNNNN */
    ASSERT(extract_scalar("\"\\U00000041\"", buf, sizeof(buf), NULL), "u8: 41");
    ASSERT_EQ_STR(buf, "A", "u8: A");

    ASSERT(extract_scalar("\"\\U0001F600\"", buf, sizeof(buf), NULL), "u8: emoji");
    ASSERT_EQ_STR(buf, "\xf0\x9f\x98\x80", "u8: emoji");
}

static void test_multiline_plain_scalar(void) {
    char buf[256];
    ASSERT(extract_mapping_value("data: hello\n  world\n", buf, sizeof(buf), NULL),
           "ml plain");
    ASSERT_EQ_STR(buf, "hello world", "ml plain: value");
}

static void test_literal_block_empty_lines(void) {
    char buf[1024];
    ASSERT(extract_mapping_value("data: |\n  line1\n\n  line2\n", buf, sizeof(buf), NULL),
           "literal empty line");
    ASSERT_EQ_STR(buf, "line1\n\nline2\n", "literal empty line: value");
}

static void test_folded_block_paragraphs(void) {
    char buf[1024];
    ASSERT(extract_mapping_value("data: >\n  para1\n\n  para2\n", buf, sizeof(buf), NULL),
           "folded paragraphs");
    ASSERT_EQ_STR(buf, "para1\npara2\n", "folded paragraphs: value");
}

static void test_literal_block_indent_indicator(void) {
    char buf[1024];
    /* Indent indicator 4 */
    ASSERT(extract_mapping_value("data: |4\n    text\n    more\n", buf, sizeof(buf), NULL),
           "literal indent 4");
    ASSERT_EQ_STR(buf, "text\nmore\n", "literal indent 4: value");
}

static void test_folded_block_strip(void) {
    char buf[1024];
    ASSERT(extract_mapping_value("data: >-\n  text\n\n", buf, sizeof(buf), NULL),
           "folded strip");
    ASSERT_EQ_STR(buf, "text", "folded strip: value");
}

static void test_folded_block_keep(void) {
    char buf[1024];
    ASSERT(extract_mapping_value("data: >+\n  text\n\n", buf, sizeof(buf), NULL),
           "folded keep");
    ASSERT_EQ_STR(buf, "text\n\n", "folded keep: value");
}

static void test_single_quoted_edge_cases(void) {
    char buf[256];

    /* Single quote at start */
    ASSERT(extract_scalar("''''", buf, sizeof(buf), NULL), "sq: only quote");
    ASSERT_EQ_STR(buf, "'", "sq: only quote value");

    /* Multiple escaped quotes */
    ASSERT(extract_scalar("'a''b''c'", buf, sizeof(buf), NULL), "sq: multi escape");
    ASSERT_EQ_STR(buf, "a'b'c", "sq: multi escape value");

    /* Spaces in single quoted */
    ASSERT(extract_scalar("'  spaces  '", buf, sizeof(buf), NULL), "sq: spaces");
    ASSERT_EQ_STR(buf, "  spaces  ", "sq: spaces value");
}

static void test_double_quoted_edge_cases(void) {
    char buf[256];

    /* Multiple escapes in sequence */
    ASSERT(extract_scalar("\"\\n\\t\\r\"", buf, sizeof(buf), NULL), "dq: multi esc");
    ASSERT_EQ_INT(buf[0], '\n', "dq: multi esc 0");
    ASSERT_EQ_INT(buf[1], '\t', "dq: multi esc 1");
    ASSERT_EQ_INT(buf[2], '\r', "dq: multi esc 2");

    /* Mixed content and escapes */
    ASSERT(extract_scalar("\"hello\\nworld\"", buf, sizeof(buf), NULL), "dq: mixed");
    ASSERT_EQ_STR(buf, "hello\nworld", "dq: mixed value");

    /* Spaces in double quoted */
    ASSERT(extract_scalar("\"  spaces  \"", buf, sizeof(buf), NULL), "dq: spaces");
    ASSERT_EQ_STR(buf, "  spaces  ", "dq: spaces value");
}

static void test_scalar_as_seq_item(void) {
    /* Extract first scalar from sequence context */
    yaml_parser_t parser;
    yaml_event_t event;
    const char *yaml = "- plain\n- 'single'\n- \"double\"\n";

    ASSERT(yaml_parser_initialize(&parser), "seq item init");
    yaml_parser_set_input_string(&parser, (const unsigned char *)yaml, strlen(yaml));

    int count = 0;
    yaml_scalar_style_t styles[3];
    while (1) {
        if (!yaml_parser_parse(&parser, &event)) break;
        if (event.type == YAML_SCALAR_EVENT && count < 3) {
            styles[count] = event.data.scalar.style;
            count++;
        }
        if (event.type == YAML_STREAM_END_EVENT) { yaml_event_delete(&event); break; }
        yaml_event_delete(&event);
    }
    ASSERT_EQ_INT(count, 3, "seq item: 3 scalars");
    ASSERT_EQ_INT(styles[0], YAML_PLAIN_SCALAR_STYLE, "seq item: plain");
    ASSERT_EQ_INT(styles[1], YAML_SINGLE_QUOTED_SCALAR_STYLE, "seq item: single");
    ASSERT_EQ_INT(styles[2], YAML_DOUBLE_QUOTED_SCALAR_STYLE, "seq item: double");
    yaml_parser_delete(&parser);
}

static void test_scalar_types_in_mapping(void) {
    yaml_parser_t parser;
    yaml_event_t event;
    const char *yaml = "plain: value\n'single': 'value'\n\"double\": \"value\"\n";

    ASSERT(yaml_parser_initialize(&parser), "map styles init");
    yaml_parser_set_input_string(&parser, (const unsigned char *)yaml, strlen(yaml));

    int scalar_count = 0;
    while (1) {
        if (!yaml_parser_parse(&parser, &event)) break;
        if (event.type == YAML_SCALAR_EVENT) scalar_count++;
        if (event.type == YAML_STREAM_END_EVENT) { yaml_event_delete(&event); break; }
        yaml_event_delete(&event);
    }
    ASSERT_EQ_INT(scalar_count, 6, "map styles: 6 scalars");
    yaml_parser_delete(&parser);
}

static void test_empty_scalars(void) {
    char buf[256];

    /* Empty plain scalar (mapping value) */
    ASSERT(extract_mapping_value("key:\n", buf, sizeof(buf), NULL), "empty: plain");
    ASSERT_EQ_STR(buf, "", "empty: plain value");

    /* Empty single quoted */
    ASSERT(extract_scalar("''", buf, sizeof(buf), NULL), "empty: sq");
    ASSERT_EQ_STR(buf, "", "empty: sq value");

    /* Empty double quoted */
    ASSERT(extract_scalar("\"\"", buf, sizeof(buf), NULL), "empty: dq");
    ASSERT_EQ_STR(buf, "", "empty: dq value");
}

static void test_scalar_with_colons(void) {
    char buf[256];
    ASSERT(extract_scalar("'a:b:c'", buf, sizeof(buf), NULL), "colons: sq");
    ASSERT_EQ_STR(buf, "a:b:c", "colons: sq value");

    ASSERT(extract_scalar("\"a:b:c\"", buf, sizeof(buf), NULL), "colons: dq");
    ASSERT_EQ_STR(buf, "a:b:c", "colons: dq value");
}

static void test_scalar_with_hashes(void) {
    char buf[256];
    ASSERT(extract_scalar("'a # b'", buf, sizeof(buf), NULL), "hash: sq");
    ASSERT_EQ_STR(buf, "a # b", "hash: sq value");

    ASSERT(extract_scalar("\"a # b\"", buf, sizeof(buf), NULL), "hash: dq");
    ASSERT_EQ_STR(buf, "a # b", "hash: dq value");
}

int main(void) {
    TEST_SUITE_BEGIN("Scalars");

    test_plain_scalars();
    test_single_quoted();
    test_double_quoted();
    test_literal_scalars();
    test_folded_scalars();
    test_scalar_contexts();
    test_special_values();
    test_whitespace_handling();
    test_utf8_scalars();
    test_long_scalars();

    /* Extended */
    test_escape_sequences_comprehensive();
    test_hex_unicode_escapes();
    test_multiline_plain_scalar();
    test_literal_block_empty_lines();
    test_folded_block_paragraphs();
    test_literal_block_indent_indicator();
    test_folded_block_strip();
    test_folded_block_keep();
    test_single_quoted_edge_cases();
    test_double_quoted_edge_cases();
    test_scalar_as_seq_item();
    test_scalar_types_in_mapping();
    test_empty_scalars();
    test_scalar_with_colons();
    test_scalar_with_hashes();

    TEST_SUITE_END();
}
