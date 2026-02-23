/*
 * YAML spec conformance tests targeting constructs that have historically
 * produced bugs in libqyaml, especially around flow collections, multiline
 * scalars, and indentation boundaries.
 *
 * These tests verify specific YAML spec behaviors at the scanner, parser,
 * and loader levels without requiring libyaml for comparison.
 */

#include "test_helper.h"

/* ========================================================================
 * Helper: parse and extract a specific scalar's value from an event stream
 * ======================================================================== */

static int nth_scalar_value(const char *input, int n, char *buf, size_t buflen) {
    yaml_parser_t parser;
    yaml_event_t event;
    int idx = 0;
    int found = 0;

    if (!yaml_parser_initialize(&parser)) return 0;
    yaml_parser_set_input_string(&parser, (const unsigned char *)input,
                                 strlen(input));

    while (1) {
        if (!yaml_parser_parse(&parser, &event)) break;
        if (event.type == YAML_SCALAR_EVENT) {
            if (idx == n) {
                size_t len = event.data.scalar.length;
                if (len >= buflen) len = buflen - 1;
                memcpy(buf, event.data.scalar.value, len);
                buf[len] = '\0';
                found = 1;
                yaml_event_delete(&event);
                break;
            }
            idx++;
        }
        int done = (event.type == YAML_STREAM_END_EVENT);
        yaml_event_delete(&event);
        if (done) break;
    }

    yaml_parser_delete(&parser);
    return found;
}

/* Helper: count total scalars in event stream */
static int count_scalars(const char *input) {
    yaml_parser_t parser;
    yaml_event_t event;
    int count = 0;

    if (!yaml_parser_initialize(&parser)) return -1;
    yaml_parser_set_input_string(&parser, (const unsigned char *)input,
                                 strlen(input));

    while (1) {
        if (!yaml_parser_parse(&parser, &event)) { count = -1; break; }
        if (event.type == YAML_SCALAR_EVENT) count++;
        int done = (event.type == YAML_STREAM_END_EVENT);
        yaml_event_delete(&event);
        if (done) break;
    }

    yaml_parser_delete(&parser);
    return count;
}

/* Helper: check that parsing succeeds */
static int parses_ok(const char *input) {
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

/* Helper: check that parsing fails */
static int parse_fails(const char *input) {
    return !parses_ok(input);
}

/* ========================================================================
 * Flow collection spec conformance
 * ======================================================================== */

/* Empty flow collections */
static void test_yaml_flow_empty_seq(void) {
    ASSERT(parses_ok("[]"), "flow: empty seq");
    ASSERT_EQ_INT(count_scalars("[]"), 0, "flow: empty seq has 0 scalars");
}

static void test_yaml_flow_empty_map(void) {
    ASSERT(parses_ok("{}"), "flow: empty map");
    ASSERT_EQ_INT(count_scalars("{}"), 0, "flow: empty map has 0 scalars");
}

/* Single entry flow collections */
static void test_yaml_flow_single_seq(void) {
    char buf[64];
    ASSERT(nth_scalar_value("[only]", 0, buf, sizeof(buf)),
           "flow: single seq item");
    ASSERT_EQ_STR(buf, "only", "flow: single seq value");
}

static void test_yaml_flow_single_map(void) {
    char buf[64];
    ASSERT(nth_scalar_value("{key: val}", 0, buf, sizeof(buf)),
           "flow: single map key");
    ASSERT_EQ_STR(buf, "key", "flow: single map key value");
    ASSERT(nth_scalar_value("{key: val}", 1, buf, sizeof(buf)),
           "flow: single map value");
    ASSERT_EQ_STR(buf, "val", "flow: single map value");
}

/* Flow collections with trailing commas (YAML allows these) */
static void test_yaml_flow_trailing_comma_seq(void) {
    ASSERT(parses_ok("[a, b, c,]"), "flow: trailing comma in seq");
    ASSERT_EQ_INT(count_scalars("[a, b, c,]"), 3,
                  "flow: trailing comma seq has 3 items");
}

static void test_yaml_flow_trailing_comma_map(void) {
    ASSERT(parses_ok("{a: 1, b: 2,}"), "flow: trailing comma in map");
    ASSERT_EQ_INT(count_scalars("{a: 1, b: 2,}"), 4,
                  "flow: trailing comma map has 4 scalars (2 k-v pairs)");
}

/* Flow collections spanning multiple lines */
static void test_yaml_flow_multiline_seq(void) {
    ASSERT(parses_ok("[\na,\nb,\nc\n]"), "flow: multiline seq");
    ASSERT_EQ_INT(count_scalars("[\na,\nb,\nc\n]"), 3,
                  "flow: multiline seq has 3 items");
}

static void test_yaml_flow_multiline_map(void) {
    ASSERT(parses_ok("{\na: 1,\nb: 2\n}"), "flow: multiline map");
    ASSERT_EQ_INT(count_scalars("{\na: 1,\nb: 2\n}"), 4,
                  "flow: multiline map has 4 scalars");
}

/* Implicit keys in flow sequences create single-pair mappings */
static void test_yaml_flow_seq_implicit_key(void) {
    yaml_parser_t parser;
    yaml_event_t event;
    int map_starts = 0;

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser,
        (const unsigned char *)"[a: b, c: d]", 12);

    while (1) {
        if (!yaml_parser_parse(&parser, &event)) break;
        if (event.type == YAML_MAPPING_START_EVENT) map_starts++;
        int done = (event.type == YAML_STREAM_END_EVENT);
        yaml_event_delete(&event);
        if (done) break;
    }
    yaml_parser_delete(&parser);
    ASSERT_EQ_INT(map_starts, 2, "flow seq implicit key: 2 mappings");
}

/* Nested flow collections */
static void test_yaml_flow_deeply_nested(void) {
    ASSERT(parses_ok("[[[1]], [[2]]]"), "flow: deeply nested seq");
    ASSERT(parses_ok("{a: {b: {c: 1}}}"), "flow: deeply nested map");
    ASSERT(parses_ok("[{a: [1, 2]}, {b: [3, 4]}]"),
           "flow: mixed nested collections");
}

/* Flow collection with adjacent brackets/braces (no whitespace) */
static void test_yaml_flow_no_whitespace(void) {
    ASSERT(parses_ok("[a,b,c]"), "flow: no whitespace between items");
    ASSERT(parses_ok("{a:1,b:2}"), "flow: no whitespace in map");
}

/* ========================================================================
 * Plain scalar spec conformance
 * ======================================================================== */

/* Plain scalars end at flow indicators in flow context but not in block */
static void test_yaml_plain_flow_indicators_block(void) {
    char buf[64];
    /* In block context, [, ], {, } are allowed in plain scalars */
    ASSERT(nth_scalar_value("key: val[ue\n", 1, buf, sizeof(buf)),
           "plain: [ in block value");
    ASSERT_EQ_STR(buf, "val[ue", "plain: [ preserved in block");

    ASSERT(nth_scalar_value("key: val]ue\n", 1, buf, sizeof(buf)),
           "plain: ] in block value");
    ASSERT_EQ_STR(buf, "val]ue", "plain: ] preserved in block");

    ASSERT(nth_scalar_value("key: val{ue\n", 1, buf, sizeof(buf)),
           "plain: { in block value");
    ASSERT_EQ_STR(buf, "val{ue", "plain: { preserved in block");

    ASSERT(nth_scalar_value("key: val}ue\n", 1, buf, sizeof(buf)),
           "plain: } in block value");
    ASSERT_EQ_STR(buf, "val}ue", "plain: } preserved in block");
}

/* Plain scalars end at : followed by space */
static void test_yaml_plain_colon_space(void) {
    char buf[64];
    /* ":nospace" is NOT a mapping indicator */
    ASSERT(nth_scalar_value("key: abc:def\n", 1, buf, sizeof(buf)),
           "plain: colon without space");
    ASSERT_EQ_STR(buf, "abc:def", "plain: colon without space preserved");
}

/* Plain scalars end at # preceded by space */
static void test_yaml_plain_hash_comment(void) {
    char buf[64];
    /* "abc#def" is NOT a comment start */
    ASSERT(nth_scalar_value("key: abc#def\n", 1, buf, sizeof(buf)),
           "plain: hash without space");
    ASSERT_EQ_STR(buf, "abc#def", "plain: hash without space preserved");
}

/* Plain scalar line folding */
static void test_yaml_plain_line_folding(void) {
    char buf[256];
    /* Adjacent lines fold to a single space */
    ASSERT(nth_scalar_value("key:\n  line1\n  line2\n", 1, buf, sizeof(buf)),
           "plain: line folding");
    ASSERT_EQ_STR(buf, "line1 line2", "plain: lines fold to space");
}

/* Plain scalar blank line becomes newline */
static void test_yaml_plain_blank_line_newline(void) {
    char buf[256];
    ASSERT(nth_scalar_value("key:\n  para1\n\n  para2\n", 1, buf, sizeof(buf)),
           "plain: blank line");
    ASSERT_EQ_STR(buf, "para1\npara2", "plain: blank line becomes newline");
}

/* Plain scalar with multiple blank lines */
static void test_yaml_plain_multiple_blank_lines(void) {
    char buf[256];
    ASSERT(nth_scalar_value("key:\n  a\n\n\n  b\n", 1, buf, sizeof(buf)),
           "plain: multiple blank lines");
    ASSERT_EQ_STR(buf, "a\n\nb", "plain: 2 blank lines become 2 newlines");
}

/* ========================================================================
 * Double-quoted scalar spec conformance
 * ======================================================================== */

/* Escape sequences */
static void test_yaml_dquote_escapes(void) {
    char buf[64];

    ASSERT(nth_scalar_value("\"\\0\"", 0, buf, sizeof(buf)),
           "dquote: null escape");
    ASSERT_EQ_INT(buf[0], '\0', "dquote: \\0 is NUL");

    ASSERT(nth_scalar_value("\"\\a\"", 0, buf, sizeof(buf)),
           "dquote: bell escape");
    ASSERT_EQ_INT(buf[0], '\x07', "dquote: \\a is BEL");

    ASSERT(nth_scalar_value("\"\\b\"", 0, buf, sizeof(buf)),
           "dquote: backspace escape");
    ASSERT_EQ_INT(buf[0], '\x08', "dquote: \\b is BS");

    ASSERT(nth_scalar_value("\"\\t\"", 0, buf, sizeof(buf)),
           "dquote: tab escape");
    ASSERT_EQ_INT(buf[0], '\t', "dquote: \\t is TAB");

    ASSERT(nth_scalar_value("\"\\n\"", 0, buf, sizeof(buf)),
           "dquote: newline escape");
    ASSERT_EQ_INT(buf[0], '\n', "dquote: \\n is LF");

    ASSERT(nth_scalar_value("\"\\r\"", 0, buf, sizeof(buf)),
           "dquote: return escape");
    ASSERT_EQ_INT(buf[0], '\r', "dquote: \\r is CR");

    ASSERT(nth_scalar_value("\"\\\\\"", 0, buf, sizeof(buf)),
           "dquote: backslash escape");
    ASSERT_EQ_INT(buf[0], '\\', "dquote: \\\\ is backslash");

    ASSERT(nth_scalar_value("\"\\\"\"", 0, buf, sizeof(buf)),
           "dquote: quote escape");
    ASSERT_EQ_INT(buf[0], '"', "dquote: \\\" is quote");
}

/* Unicode escape sequences */
static void test_yaml_dquote_unicode_escapes(void) {
    char buf[64];

    /* \x41 = 'A' */
    ASSERT(nth_scalar_value("\"\\x41\"", 0, buf, sizeof(buf)),
           "dquote: \\x escape");
    ASSERT_EQ_STR(buf, "A", "dquote: \\x41 is A");

    /* \u0041 = 'A' */
    ASSERT(nth_scalar_value("\"\\u0041\"", 0, buf, sizeof(buf)),
           "dquote: \\u escape");
    ASSERT_EQ_STR(buf, "A", "dquote: \\u0041 is A");

    /* \u00E9 = e-acute (2-byte UTF-8) */
    ASSERT(nth_scalar_value("\"\\u00E9\"", 0, buf, sizeof(buf)),
           "dquote: \\u 2-byte UTF-8");
    ASSERT_EQ_INT((unsigned char)buf[0], 0xC3, "dquote: \\u00E9 byte 0");
    ASSERT_EQ_INT((unsigned char)buf[1], 0xA9, "dquote: \\u00E9 byte 1");

    /* \u4E2D = CJK character (3-byte UTF-8) */
    ASSERT(nth_scalar_value("\"\\u4E2D\"", 0, buf, sizeof(buf)),
           "dquote: \\u 3-byte UTF-8");
    ASSERT_EQ_INT((unsigned char)buf[0], 0xE4, "dquote: \\u4E2D byte 0");
    ASSERT_EQ_INT((unsigned char)buf[1], 0xB8, "dquote: \\u4E2D byte 1");
    ASSERT_EQ_INT((unsigned char)buf[2], 0xAD, "dquote: \\u4E2D byte 2");
}

/* Double-quoted line folding */
static void test_yaml_dquote_line_folding(void) {
    char buf[256];

    /* Adjacent lines fold to a single space */
    ASSERT(nth_scalar_value("\"hello\n  world\"", 0, buf, sizeof(buf)),
           "dquote: line folding");
    ASSERT_EQ_STR(buf, "hello world", "dquote: lines fold to space");
}

/* Double-quoted escaped newline (line continuation) */
static void test_yaml_dquote_escaped_newline(void) {
    char buf[256];

    /* Backslash-newline is line continuation (whitespace stripped) */
    ASSERT(nth_scalar_value("\"hello\\\n  world\"", 0, buf, sizeof(buf)),
           "dquote: escaped newline");
    ASSERT_EQ_STR(buf, "helloworld", "dquote: escaped newline joins lines");
}

/* ========================================================================
 * Single-quoted scalar spec conformance
 * ======================================================================== */

static void test_yaml_squote_basic(void) {
    char buf[64];
    ASSERT(nth_scalar_value("'hello world'", 0, buf, sizeof(buf)),
           "squote: basic");
    ASSERT_EQ_STR(buf, "hello world", "squote: basic value");
}

static void test_yaml_squote_escaped_quote(void) {
    char buf[64];
    /* '' inside single quotes becomes a single ' */
    ASSERT(nth_scalar_value("'it''s'", 0, buf, sizeof(buf)),
           "squote: escaped quote");
    ASSERT_EQ_STR(buf, "it's", "squote: '' becomes single quote");
}

static void test_yaml_squote_no_escapes(void) {
    char buf[64];
    /* Backslash is literal in single-quoted strings */
    ASSERT(nth_scalar_value("'hello\\nworld'", 0, buf, sizeof(buf)),
           "squote: no escapes");
    ASSERT_EQ_STR(buf, "hello\\nworld",
                  "squote: backslash-n is literal");
}

static void test_yaml_squote_line_folding(void) {
    char buf[256];
    ASSERT(nth_scalar_value("'hello\n  world'", 0, buf, sizeof(buf)),
           "squote: line folding");
    ASSERT_EQ_STR(buf, "hello world", "squote: lines fold to space");
}

/* ========================================================================
 * Block scalar spec conformance
 * ======================================================================== */

/* Literal block scalar preserves newlines */
static void test_yaml_literal_preserves_newlines(void) {
    char buf[256];
    ASSERT(nth_scalar_value("|\n  line1\n  line2\n", 0, buf, sizeof(buf)),
           "literal: preserves newlines");
    ASSERT_EQ_STR(buf, "line1\nline2\n",
                  "literal: newlines preserved");
}

/* Folded block scalar folds lines */
static void test_yaml_folded_folds_lines(void) {
    char buf[256];
    ASSERT(nth_scalar_value(">\n  line1\n  line2\n", 0, buf, sizeof(buf)),
           "folded: folds lines");
    ASSERT_EQ_STR(buf, "line1 line2\n",
                  "folded: adjacent lines fold to space");
}

/* Block scalar chomp indicators */
static void test_yaml_block_chomp_strip(void) {
    char buf[256];
    ASSERT(nth_scalar_value("|-\n  text\n", 0, buf, sizeof(buf)),
           "literal strip: no trailing newline");
    ASSERT_EQ_STR(buf, "text", "literal strip: value");
}

static void test_yaml_block_chomp_keep(void) {
    char buf[256];
    ASSERT(nth_scalar_value("|+\n  text\n\n", 0, buf, sizeof(buf)),
           "literal keep: trailing newlines kept");
    ASSERT_EQ_STR(buf, "text\n\n", "literal keep: value");
}

static void test_yaml_block_chomp_clip(void) {
    char buf[256];
    /* Default chomp (clip): single trailing newline */
    ASSERT(nth_scalar_value("|\n  text\n\n\n", 0, buf, sizeof(buf)),
           "literal clip: single trailing newline");
    ASSERT_EQ_STR(buf, "text\n", "literal clip: value");
}

/* ========================================================================
 * Indentation boundary tests
 * ======================================================================== */

/* Indentation determines scope in block context */
static void test_yaml_indent_scope(void) {
    char buf[64];
    /* "a:\n  b: c\nd: e\n" - 'd' is at same level as 'a'
     * Produces mapping {a: {b: c}, d: e} -> scalars: a, b, c, d, e = 5 */
    ASSERT(parses_ok("a:\n  b: c\nd: e\n"), "indent: scope");
    ASSERT_EQ_INT(count_scalars("a:\n  b: c\nd: e\n"), 5,
                  "indent: 5 scalars (a, b, c, d, e)");
}

/* Minimum indentation for block scalars */
static void test_yaml_indent_block_scalar(void) {
    char buf[256];
    /* Block scalar content must be indented relative to current level */
    ASSERT(nth_scalar_value("key:\n  |\n    text\n", 1, buf, sizeof(buf)),
           "indent: block scalar in mapping");
    ASSERT_EQ_STR(buf, "text\n", "indent: block scalar value");
}

/* Indentless sequence under mapping key */
static void test_yaml_indent_indentless_seq(void) {
    ASSERT(parses_ok("key:\n- item1\n- item2\n"),
           "indent: indentless seq");

    yaml_parser_t parser;
    yaml_event_t event;
    int seq_start = 0;

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser,
        (const unsigned char *)"key:\n- item1\n- item2\n", 21);

    while (1) {
        if (!yaml_parser_parse(&parser, &event)) break;
        if (event.type == YAML_SEQUENCE_START_EVENT) seq_start++;
        int done = (event.type == YAML_STREAM_END_EVENT);
        yaml_event_delete(&event);
        if (done) break;
    }
    yaml_parser_delete(&parser);
    ASSERT_EQ_INT(seq_start, 1, "indent: indentless seq produces sequence");
}

/* ========================================================================
 * Document boundary spec conformance
 * ======================================================================== */

/* Explicit document start */
static void test_yaml_doc_explicit_start(void) {
    yaml_parser_t parser;
    yaml_event_t event;
    int doc_starts = 0;
    int doc_start_implicit = -1;

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser,
        (const unsigned char *)"---\nhello\n", 10);

    while (1) {
        if (!yaml_parser_parse(&parser, &event)) break;
        if (event.type == YAML_DOCUMENT_START_EVENT) {
            doc_starts++;
            doc_start_implicit = event.data.document_start.implicit;
        }
        int done = (event.type == YAML_STREAM_END_EVENT);
        yaml_event_delete(&event);
        if (done) break;
    }
    yaml_parser_delete(&parser);
    ASSERT_EQ_INT(doc_starts, 1, "doc: 1 explicit start");
    ASSERT_EQ_INT(doc_start_implicit, 0, "doc: start is not implicit");
}

/* Implicit document start */
static void test_yaml_doc_implicit_start(void) {
    yaml_parser_t parser;
    yaml_event_t event;
    int doc_start_implicit = -1;

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser,
        (const unsigned char *)"hello\n", 6);

    while (1) {
        if (!yaml_parser_parse(&parser, &event)) break;
        if (event.type == YAML_DOCUMENT_START_EVENT) {
            doc_start_implicit = event.data.document_start.implicit;
        }
        int done = (event.type == YAML_STREAM_END_EVENT);
        yaml_event_delete(&event);
        if (done) break;
    }
    yaml_parser_delete(&parser);
    ASSERT_EQ_INT(doc_start_implicit, 1, "doc: start is implicit");
}

/* Document end marker */
static void test_yaml_doc_end_marker(void) {
    yaml_parser_t parser;
    yaml_event_t event;
    int doc_ends = 0;
    int doc_end_implicit = -1;

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser,
        (const unsigned char *)"hello\n...\n", 10);

    while (1) {
        if (!yaml_parser_parse(&parser, &event)) break;
        if (event.type == YAML_DOCUMENT_END_EVENT) {
            doc_ends++;
            doc_end_implicit = event.data.document_end.implicit;
        }
        int done = (event.type == YAML_STREAM_END_EVENT);
        yaml_event_delete(&event);
        if (done) break;
    }
    yaml_parser_delete(&parser);
    ASSERT(doc_ends >= 1, "doc: at least 1 end event");
    ASSERT_EQ_INT(doc_end_implicit, 0, "doc: end is not implicit (has ...)");
}

/* ========================================================================
 * Tag spec conformance
 * ======================================================================== */

/* Verbatim tags */
static void test_yaml_tag_verbatim(void) {
    yaml_parser_t parser;
    yaml_event_t event;
    char tag_buf[128] = "";

    yaml_parser_initialize(&parser);
    const char *input = "!<tag:example.com,2000:foo> bar\n";
    yaml_parser_set_input_string(&parser,
        (const unsigned char *)input, strlen(input));

    while (1) {
        if (!yaml_parser_parse(&parser, &event)) break;
        if (event.type == YAML_SCALAR_EVENT && event.data.scalar.tag) {
            strncpy(tag_buf, (const char *)event.data.scalar.tag,
                    sizeof(tag_buf) - 1);
        }
        int done = (event.type == YAML_STREAM_END_EVENT);
        yaml_event_delete(&event);
        if (done) break;
    }
    yaml_parser_delete(&parser);
    ASSERT_EQ_STR(tag_buf, "tag:example.com,2000:foo",
                  "tag: verbatim tag value");
}

/* Shorthand tags with directive */
static void test_yaml_tag_shorthand(void) {
    yaml_parser_t parser;
    yaml_event_t event;
    char tag_buf[128] = "";

    yaml_parser_initialize(&parser);
    const char *input = "%TAG !e! tag:example.com,2000:\n---\n!e!foo bar\n";
    yaml_parser_set_input_string(&parser,
        (const unsigned char *)input, strlen(input));

    while (1) {
        if (!yaml_parser_parse(&parser, &event)) break;
        if (event.type == YAML_SCALAR_EVENT && event.data.scalar.tag) {
            strncpy(tag_buf, (const char *)event.data.scalar.tag,
                    sizeof(tag_buf) - 1);
        }
        int done = (event.type == YAML_STREAM_END_EVENT);
        yaml_event_delete(&event);
        if (done) break;
    }
    yaml_parser_delete(&parser);
    ASSERT_EQ_STR(tag_buf, "tag:example.com,2000:foo",
                  "tag: shorthand resolves to full tag");
}

/* Primary tag handle ! */
static void test_yaml_tag_primary(void) {
    yaml_parser_t parser;
    yaml_event_t event;
    char tag_buf[128] = "";

    yaml_parser_initialize(&parser);
    const char *input = "!local bar\n";
    yaml_parser_set_input_string(&parser,
        (const unsigned char *)input, strlen(input));

    while (1) {
        if (!yaml_parser_parse(&parser, &event)) break;
        if (event.type == YAML_SCALAR_EVENT && event.data.scalar.tag) {
            strncpy(tag_buf, (const char *)event.data.scalar.tag,
                    sizeof(tag_buf) - 1);
        }
        int done = (event.type == YAML_STREAM_END_EVENT);
        yaml_event_delete(&event);
        if (done) break;
    }
    yaml_parser_delete(&parser);
    ASSERT_EQ_STR(tag_buf, "!local", "tag: primary tag");
}

/* ========================================================================
 * Anchor/alias spec conformance
 * ======================================================================== */

/* Anchors can be applied to any node type */
static void test_yaml_anchor_on_scalar(void) {
    ASSERT(parses_ok("&a hello\n"), "anchor: on scalar");
}

static void test_yaml_anchor_on_sequence(void) {
    ASSERT(parses_ok("&a\n- 1\n- 2\n"), "anchor: on sequence");
}

static void test_yaml_anchor_on_mapping(void) {
    ASSERT(parses_ok("&a\nkey: val\n"), "anchor: on mapping");
}

static void test_yaml_anchor_on_flow(void) {
    ASSERT(parses_ok("&a [1, 2]\n"), "anchor: on flow seq");
    ASSERT(parses_ok("&a {k: v}\n"), "anchor: on flow map");
}

/* Alias must reference a previously defined anchor */
static void test_yaml_alias_forward_ref_fails(void) {
    yaml_parser_t parser;
    yaml_document_t doc;

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser,
        (const unsigned char *)"- *forward\n- &forward val\n", 26);

    int ok = yaml_parser_load(&parser, &doc);
    ASSERT(!ok, "alias: forward reference fails at load");
    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
}

/* ========================================================================
 * main
 * ======================================================================== */

int main(void) {
    TEST_SUITE_BEGIN("YAML Spec Conformance");

    /* Flow collections */
    test_yaml_flow_empty_seq();
    test_yaml_flow_empty_map();
    test_yaml_flow_single_seq();
    test_yaml_flow_single_map();
    test_yaml_flow_trailing_comma_seq();
    test_yaml_flow_trailing_comma_map();
    test_yaml_flow_multiline_seq();
    test_yaml_flow_multiline_map();
    test_yaml_flow_seq_implicit_key();
    test_yaml_flow_deeply_nested();
    test_yaml_flow_no_whitespace();

    /* Plain scalars */
    test_yaml_plain_flow_indicators_block();
    test_yaml_plain_colon_space();
    test_yaml_plain_hash_comment();
    test_yaml_plain_line_folding();
    test_yaml_plain_blank_line_newline();
    test_yaml_plain_multiple_blank_lines();

    /* Double-quoted scalars */
    test_yaml_dquote_escapes();
    test_yaml_dquote_unicode_escapes();
    test_yaml_dquote_line_folding();
    test_yaml_dquote_escaped_newline();

    /* Single-quoted scalars */
    test_yaml_squote_basic();
    test_yaml_squote_escaped_quote();
    test_yaml_squote_no_escapes();
    test_yaml_squote_line_folding();

    /* Block scalars */
    test_yaml_literal_preserves_newlines();
    test_yaml_folded_folds_lines();
    test_yaml_block_chomp_strip();
    test_yaml_block_chomp_keep();
    test_yaml_block_chomp_clip();

    /* Indentation */
    test_yaml_indent_scope();
    test_yaml_indent_block_scalar();
    test_yaml_indent_indentless_seq();

    /* Document boundaries */
    test_yaml_doc_explicit_start();
    test_yaml_doc_implicit_start();
    test_yaml_doc_end_marker();

    /* Tags */
    test_yaml_tag_verbatim();
    test_yaml_tag_shorthand();
    test_yaml_tag_primary();

    /* Anchors and aliases */
    test_yaml_anchor_on_scalar();
    test_yaml_anchor_on_sequence();
    test_yaml_anchor_on_mapping();
    test_yaml_anchor_on_flow();
    test_yaml_alias_forward_ref_fails();

    TEST_SUITE_END();
}
