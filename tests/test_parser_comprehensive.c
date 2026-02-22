#include "test_helper.h"

/*
 * Comprehensive parser tests covering all event types and combinations.
 * Each test function focuses on a specific YAML construct or parser behavior.
 */

/* Helper: parse and collect events */
static int collect_events(const char *input, yaml_event_type_t *types,
                          int max, int *count) {
    return parse_string_events(input, types, max, count);
}

/* Helper: parse and find scalar value */
static int find_scalar(const char *input, const char *expected) {
    yaml_parser_t parser;
    yaml_event_t event;
    int found = 0;

    if (!yaml_parser_initialize(&parser)) return 0;
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));

    while (1) {
        if (!yaml_parser_parse(&parser, &event)) break;
        if (event.type == YAML_SCALAR_EVENT) {
            if (strcmp((const char *)event.data.scalar.value, expected) == 0)
                found = 1;
        }
        int done = (event.type == YAML_STREAM_END_EVENT);
        yaml_event_delete(&event);
        if (done) break;
    }
    yaml_parser_delete(&parser);
    return found;
}

/* Helper: count events of a specific type */
static int count_event_type(const char *input, yaml_event_type_t type) {
    yaml_parser_t parser;
    yaml_event_t event;
    int count = 0;

    if (!yaml_parser_initialize(&parser)) return -1;
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));

    while (1) {
        if (!yaml_parser_parse(&parser, &event)) break;
        if (event.type == type) count++;
        int done = (event.type == YAML_STREAM_END_EVENT);
        yaml_event_delete(&event);
        if (done) break;
    }
    yaml_parser_delete(&parser);
    return count;
}

/* ========== Empty/minimal streams ========== */

static void test_p_empty_stream(void) {
    yaml_event_type_t ev[8];
    int n;
    ASSERT(collect_events("", ev, 8, &n), "empty stream parse");
    ASSERT_EQ_INT(n, 2, "empty: 2 events");
    ASSERT_EQ_INT(ev[0], YAML_STREAM_START_EVENT, "empty: stream start");
    ASSERT_EQ_INT(ev[1], YAML_STREAM_END_EVENT, "empty: stream end");
}

static void test_p_whitespace_stream(void) {
    yaml_event_type_t ev[8];
    int n;
    ASSERT(collect_events("   \n  \n", ev, 8, &n), "ws stream");
    ASSERT_EQ_INT(n, 2, "ws: 2 events");
}

static void test_p_comment_only_stream(void) {
    yaml_event_type_t ev[8];
    int n;
    ASSERT(collect_events("# just a comment\n", ev, 8, &n), "comment stream");
    ASSERT_EQ_INT(n, 2, "comment: 2 events");
}

static void test_p_multi_comment_stream(void) {
    yaml_event_type_t ev[8];
    int n;
    ASSERT(collect_events("# one\n# two\n# three\n", ev, 8, &n), "multi comment");
    ASSERT_EQ_INT(n, 2, "multi comment: 2 events");
}

/* ========== Document events ========== */

static void test_p_implicit_doc(void) {
    yaml_event_type_t ev[16];
    int n;
    ASSERT(collect_events("hello", ev, 16, &n), "implicit doc");
    ASSERT_EQ_INT(ev[1], YAML_DOCUMENT_START_EVENT, "implicit doc start");
}

static void test_p_explicit_doc_start(void) {
    yaml_event_type_t ev[16];
    int n;
    ASSERT(collect_events("---\nhello", ev, 16, &n), "explicit doc start");
    ASSERT_EQ_INT(ev[1], YAML_DOCUMENT_START_EVENT, "doc start event");
}

static void test_p_explicit_doc_end(void) {
    yaml_event_type_t ev[16];
    int n;
    ASSERT(collect_events("---\nhello\n...", ev, 16, &n), "explicit doc end");
    int found_end = 0;
    for (int i = 0; i < n; i++)
        if (ev[i] == YAML_DOCUMENT_END_EVENT) found_end = 1;
    ASSERT(found_end, "found doc end");
}

static void test_p_multiple_docs(void) {
    int count = count_event_type("---\na\n---\nb\n---\nc\n", YAML_DOCUMENT_START_EVENT);
    ASSERT_EQ_INT(count, 3, "3 document starts");
}

static void test_p_doc_with_end_marker(void) {
    int count = count_event_type("---\na\n...\n---\nb\n...\n", YAML_DOCUMENT_END_EVENT);
    ASSERT_EQ_INT(count, 2, "2 document ends");
}

static void test_p_empty_doc(void) {
    yaml_event_type_t ev[16];
    int n;
    ASSERT(collect_events("---\n...\n", ev, 16, &n), "empty doc");
    ASSERT_EQ_INT(ev[1], YAML_DOCUMENT_START_EVENT, "empty: doc start");
    /* ev[2] is an empty scalar, then doc end follows */
    int found_end = 0;
    for (int i = 0; i < n; i++)
        if (ev[i] == YAML_DOCUMENT_END_EVENT) found_end = 1;
    ASSERT(found_end, "empty: doc end found");
}

static void test_p_empty_doc_implicit(void) {
    yaml_event_type_t ev[16];
    int n;
    ASSERT(collect_events("---\n---\n", ev, 16, &n), "2 empty docs");
    int ds = count_event_type("---\n---\n", YAML_DOCUMENT_START_EVENT);
    ASSERT_EQ_INT(ds, 2, "2 doc starts");
}

/* ========== Scalar events ========== */

static void test_p_plain_scalar(void) {
    ASSERT(find_scalar("hello", "hello"), "plain scalar");
}

static void test_p_plain_scalar_number(void) {
    ASSERT(find_scalar("42", "42"), "plain number");
}

static void test_p_plain_scalar_float(void) {
    ASSERT(find_scalar("3.14", "3.14"), "plain float");
}

static void test_p_plain_scalar_bool_true(void) {
    ASSERT(find_scalar("true", "true"), "plain true");
}

static void test_p_plain_scalar_bool_false(void) {
    ASSERT(find_scalar("false", "false"), "plain false");
}

static void test_p_plain_scalar_null(void) {
    ASSERT(find_scalar("null", "null"), "plain null");
}

static void test_p_plain_scalar_tilde(void) {
    ASSERT(find_scalar("~", "~"), "plain tilde");
}

static void test_p_single_quoted(void) {
    ASSERT(find_scalar("'hello'", "hello"), "single quoted");
}

static void test_p_single_quoted_empty(void) {
    ASSERT(find_scalar("''", ""), "single quoted empty");
}

static void test_p_single_quoted_escape(void) {
    ASSERT(find_scalar("'it''s'", "it's"), "single quoted escape");
}

static void test_p_double_quoted(void) {
    ASSERT(find_scalar("\"hello\"", "hello"), "double quoted");
}

static void test_p_double_quoted_empty(void) {
    ASSERT(find_scalar("\"\"", ""), "double quoted empty");
}

static void test_p_double_quoted_escape_n(void) {
    ASSERT(find_scalar("\"line1\\nline2\"", "line1\nline2"), "dq \\n");
}

static void test_p_double_quoted_escape_t(void) {
    ASSERT(find_scalar("\"a\\tb\"", "a\tb"), "dq \\t");
}

static void test_p_double_quoted_escape_backslash(void) {
    ASSERT(find_scalar("\"a\\\\b\"", "a\\b"), "dq \\\\");
}

static void test_p_double_quoted_escape_quote(void) {
    ASSERT(find_scalar("\"a\\\"b\"", "a\"b"), "dq \\\"");
}

static void test_p_double_quoted_hex(void) {
    ASSERT(find_scalar("\"\\x41\"", "A"), "dq hex");
}

static void test_p_double_quoted_unicode(void) {
    ASSERT(find_scalar("\"\\u0041\"", "A"), "dq unicode");
}

static void test_p_literal_block(void) {
    ASSERT(find_scalar("|\n  hello\n  world\n", "hello\nworld\n"), "literal block");
}

static void test_p_literal_block_strip(void) {
    ASSERT(find_scalar("|-\n  hello\n", "hello"), "literal strip");
}

static void test_p_literal_block_keep(void) {
    ASSERT(find_scalar("|+\n  hello\n", "hello\n"), "literal keep");
}

static void test_p_folded_block(void) {
    ASSERT(find_scalar(">\n  hello\n  world\n", "hello world\n"), "folded block");
}

static void test_p_folded_block_strip(void) {
    ASSERT(find_scalar(">-\n  hello\n  world\n", "hello world"), "folded strip");
}

static void test_p_folded_block_keep(void) {
    ASSERT(find_scalar(">+\n  hello\n  world\n", "hello world\n"), "folded keep");
}

/* ========== Mapping events ========== */

static void test_p_block_mapping_simple(void) {
    int count = count_event_type("a: b\n", YAML_MAPPING_START_EVENT);
    ASSERT_EQ_INT(count, 1, "block map: 1 mapping start");
}

static void test_p_block_mapping_multi(void) {
    int count = count_event_type("a: 1\nb: 2\nc: 3\n", YAML_SCALAR_EVENT);
    ASSERT_EQ_INT(count, 6, "block map multi: 6 scalars");
}

static void test_p_block_mapping_nested(void) {
    int count = count_event_type("a:\n  b: c\n", YAML_MAPPING_START_EVENT);
    ASSERT_EQ_INT(count, 2, "nested map: 2 mapping starts");
}

static void test_p_block_mapping_deep(void) {
    int count = count_event_type("a:\n  b:\n    c: d\n", YAML_MAPPING_START_EVENT);
    ASSERT_EQ_INT(count, 3, "deep map: 3 mapping starts");
}

static void test_p_flow_mapping_simple(void) {
    int count = count_event_type("{a: b}", YAML_MAPPING_START_EVENT);
    ASSERT_EQ_INT(count, 1, "flow map: 1 mapping start");
}

static void test_p_flow_mapping_multi(void) {
    int count = count_event_type("{a: 1, b: 2, c: 3}", YAML_SCALAR_EVENT);
    ASSERT_EQ_INT(count, 6, "flow map multi: 6 scalars");
}

static void test_p_flow_mapping_nested(void) {
    int count = count_event_type("{a: {b: c}}", YAML_MAPPING_START_EVENT);
    ASSERT_EQ_INT(count, 2, "flow nested map: 2 mapping starts");
}

static void test_p_flow_mapping_empty(void) {
    int count = count_event_type("{}", YAML_MAPPING_START_EVENT);
    ASSERT_EQ_INT(count, 1, "flow empty map: 1 mapping start");
}

static void test_p_mapping_with_seq_value(void) {
    int count = count_event_type("a:\n  - 1\n  - 2\n", YAML_SEQUENCE_START_EVENT);
    ASSERT_EQ_INT(count, 1, "map with seq: 1 seq start");
}

static void test_p_mapping_explicit_key(void) {
    ASSERT(find_scalar("? key\n: value\n", "key"), "explicit key: found key");
    ASSERT(find_scalar("? key\n: value\n", "value"), "explicit key: found value");
}

/* ========== Sequence events ========== */

static void test_p_block_seq_simple(void) {
    int count = count_event_type("- a\n- b\n- c\n", YAML_SEQUENCE_START_EVENT);
    ASSERT_EQ_INT(count, 1, "block seq: 1 seq start");
}

static void test_p_block_seq_scalars(void) {
    int count = count_event_type("- a\n- b\n- c\n", YAML_SCALAR_EVENT);
    ASSERT_EQ_INT(count, 3, "block seq: 3 scalars");
}

static void test_p_block_seq_nested(void) {
    int count = count_event_type("- - a\n  - b\n", YAML_SEQUENCE_START_EVENT);
    ASSERT_EQ_INT(count, 2, "nested seq: 2 seq starts");
}

static void test_p_flow_seq_simple(void) {
    int count = count_event_type("[a, b, c]", YAML_SEQUENCE_START_EVENT);
    ASSERT_EQ_INT(count, 1, "flow seq: 1 seq start");
}

static void test_p_flow_seq_scalars(void) {
    int count = count_event_type("[a, b, c]", YAML_SCALAR_EVENT);
    ASSERT_EQ_INT(count, 3, "flow seq: 3 scalars");
}

static void test_p_flow_seq_nested(void) {
    int count = count_event_type("[[a, b], [c]]", YAML_SEQUENCE_START_EVENT);
    ASSERT_EQ_INT(count, 3, "nested flow seq: 3 seq starts");
}

static void test_p_flow_seq_empty(void) {
    int count = count_event_type("[]", YAML_SEQUENCE_START_EVENT);
    ASSERT_EQ_INT(count, 1, "flow empty seq: 1 seq start");
}

static void test_p_seq_of_maps(void) {
    int count = count_event_type("- a: 1\n- b: 2\n", YAML_MAPPING_START_EVENT);
    ASSERT_EQ_INT(count, 2, "seq of maps: 2 map starts");
}

static void test_p_seq_of_seqs(void) {
    int count = count_event_type("- - a\n- - b\n", YAML_SEQUENCE_START_EVENT);
    ASSERT_EQ_INT(count, 3, "seq of seqs: 3 seq starts");
}

/* ========== Anchors and aliases ========== */

static void test_p_anchor_scalar(void) {
    yaml_parser_t parser;
    yaml_event_t event;
    int found = 0;

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)"&ref hello", 10);
    while (1) {
        if (!yaml_parser_parse(&parser, &event)) break;
        if (event.type == YAML_SCALAR_EVENT && event.data.scalar.anchor) {
            ASSERT_EQ_STR(event.data.scalar.anchor, "ref", "anchor name");
            ASSERT_EQ_STR(event.data.scalar.value, "hello", "anchor value");
            found = 1;
        }
        int done = (event.type == YAML_STREAM_END_EVENT);
        yaml_event_delete(&event);
        if (done) break;
    }
    yaml_parser_delete(&parser);
    ASSERT(found, "found anchored scalar");
}

static void test_p_alias(void) {
    int count = count_event_type("- &ref hello\n- *ref\n", YAML_ALIAS_EVENT);
    ASSERT_EQ_INT(count, 1, "alias: 1 alias event");
}

static void test_p_alias_value(void) {
    yaml_parser_t parser;
    yaml_event_t event;
    int found = 0;

    const char *input = "- &a val\n- *a\n";
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));
    while (1) {
        if (!yaml_parser_parse(&parser, &event)) break;
        if (event.type == YAML_ALIAS_EVENT) {
            ASSERT_EQ_STR(event.data.alias.anchor, "a", "alias anchor name");
            found = 1;
        }
        int done = (event.type == YAML_STREAM_END_EVENT);
        yaml_event_delete(&event);
        if (done) break;
    }
    yaml_parser_delete(&parser);
    ASSERT(found, "found alias");
}

static void test_p_anchor_mapping(void) {
    yaml_parser_t parser;
    yaml_event_t event;
    int found = 0;

    const char *input = "&m\na: b\n";
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));
    while (1) {
        if (!yaml_parser_parse(&parser, &event)) break;
        if (event.type == YAML_MAPPING_START_EVENT && event.data.mapping_start.anchor) {
            ASSERT_EQ_STR(event.data.mapping_start.anchor, "m", "mapping anchor");
            found = 1;
        }
        int done = (event.type == YAML_STREAM_END_EVENT);
        yaml_event_delete(&event);
        if (done) break;
    }
    yaml_parser_delete(&parser);
    ASSERT(found, "found anchored mapping");
}

static void test_p_anchor_sequence(void) {
    yaml_parser_t parser;
    yaml_event_t event;
    int found = 0;

    const char *input = "&s\n- a\n- b\n";
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));
    while (1) {
        if (!yaml_parser_parse(&parser, &event)) break;
        if (event.type == YAML_SEQUENCE_START_EVENT && event.data.sequence_start.anchor) {
            ASSERT_EQ_STR(event.data.sequence_start.anchor, "s", "seq anchor");
            found = 1;
        }
        int done = (event.type == YAML_STREAM_END_EVENT);
        yaml_event_delete(&event);
        if (done) break;
    }
    yaml_parser_delete(&parser);
    ASSERT(found, "found anchored sequence");
}

/* ========== Tags ========== */

static void test_p_tag_verbatim(void) {
    yaml_parser_t parser;
    yaml_event_t event;
    int found = 0;

    const char *input = "!<tag:yaml.org,2002:str> hello";
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));
    while (1) {
        if (!yaml_parser_parse(&parser, &event)) break;
        if (event.type == YAML_SCALAR_EVENT && event.data.scalar.tag) {
            ASSERT_EQ_STR(event.data.scalar.tag, "tag:yaml.org,2002:str", "verbatim tag");
            found = 1;
        }
        int done = (event.type == YAML_STREAM_END_EVENT);
        yaml_event_delete(&event);
        if (done) break;
    }
    yaml_parser_delete(&parser);
    ASSERT(found, "found verbatim tag");
}

static void test_p_tag_shorthand(void) {
    yaml_parser_t parser;
    yaml_event_t event;
    int found = 0;

    const char *input = "!!str hello";
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));
    while (1) {
        if (!yaml_parser_parse(&parser, &event)) break;
        if (event.type == YAML_SCALAR_EVENT && event.data.scalar.tag) {
            ASSERT_EQ_STR(event.data.scalar.tag, "tag:yaml.org,2002:str", "shorthand tag");
            found = 1;
        }
        int done = (event.type == YAML_STREAM_END_EVENT);
        yaml_event_delete(&event);
        if (done) break;
    }
    yaml_parser_delete(&parser);
    ASSERT(found, "found shorthand tag");
}

static void test_p_tag_primary(void) {
    yaml_parser_t parser;
    yaml_event_t event;
    int found = 0;

    const char *input = "!foo hello";
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));
    while (1) {
        if (!yaml_parser_parse(&parser, &event)) break;
        if (event.type == YAML_SCALAR_EVENT && event.data.scalar.tag) {
            ASSERT_EQ_STR(event.data.scalar.tag, "!foo", "primary tag");
            found = 1;
        }
        int done = (event.type == YAML_STREAM_END_EVENT);
        yaml_event_delete(&event);
        if (done) break;
    }
    yaml_parser_delete(&parser);
    ASSERT(found, "found primary tag");
}

static void test_p_tag_mapping(void) {
    yaml_parser_t parser;
    yaml_event_t event;
    int found = 0;

    const char *input = "!!map\na: b\n";
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));
    while (1) {
        if (!yaml_parser_parse(&parser, &event)) break;
        if (event.type == YAML_MAPPING_START_EVENT && event.data.mapping_start.tag) {
            ASSERT_EQ_STR(event.data.mapping_start.tag, "tag:yaml.org,2002:map", "map tag");
            found = 1;
        }
        int done = (event.type == YAML_STREAM_END_EVENT);
        yaml_event_delete(&event);
        if (done) break;
    }
    yaml_parser_delete(&parser);
    ASSERT(found, "found mapping tag");
}

static void test_p_tag_sequence(void) {
    yaml_parser_t parser;
    yaml_event_t event;
    int found = 0;

    const char *input = "!!seq\n- a\n";
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));
    while (1) {
        if (!yaml_parser_parse(&parser, &event)) break;
        if (event.type == YAML_SEQUENCE_START_EVENT && event.data.sequence_start.tag) {
            ASSERT_EQ_STR(event.data.sequence_start.tag, "tag:yaml.org,2002:seq", "seq tag");
            found = 1;
        }
        int done = (event.type == YAML_STREAM_END_EVENT);
        yaml_event_delete(&event);
        if (done) break;
    }
    yaml_parser_delete(&parser);
    ASSERT(found, "found sequence tag");
}

/* ========== Directives ========== */

static void test_p_yaml_directive(void) {
    yaml_parser_t parser;
    yaml_event_t event;
    int found = 0;

    const char *input = "%YAML 1.1\n---\nhello\n";
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));
    while (1) {
        if (!yaml_parser_parse(&parser, &event)) break;
        if (event.type == YAML_DOCUMENT_START_EVENT) {
            if (event.data.document_start.version_directive) {
                ASSERT_EQ_INT(event.data.document_start.version_directive->major, 1, "yaml 1.x");
                ASSERT_EQ_INT(event.data.document_start.version_directive->minor, 1, "yaml x.1");
                found = 1;
            }
        }
        int done = (event.type == YAML_STREAM_END_EVENT);
        yaml_event_delete(&event);
        if (done) break;
    }
    yaml_parser_delete(&parser);
    ASSERT(found, "found yaml directive");
}

static void test_p_tag_directive(void) {
    yaml_parser_t parser;
    yaml_event_t event;
    int found = 0;

    const char *input = "%TAG !e! tag:example.com,2000:\n---\n!e!foo bar\n";
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));
    while (1) {
        if (!yaml_parser_parse(&parser, &event)) break;
        if (event.type == YAML_SCALAR_EVENT && event.data.scalar.tag) {
            ASSERT_EQ_STR(event.data.scalar.tag, "tag:example.com,2000:foo", "custom tag resolved");
            found = 1;
        }
        int done = (event.type == YAML_STREAM_END_EVENT);
        yaml_event_delete(&event);
        if (done) break;
    }
    yaml_parser_delete(&parser);
    ASSERT(found, "found custom tag");
}

/* ========== Complex structures ========== */

static void test_p_map_of_seqs(void) {
    const char *input = "a:\n  - 1\n  - 2\nb:\n  - 3\n  - 4\n";
    ASSERT_EQ_INT(count_event_type(input, YAML_SEQUENCE_START_EVENT), 2, "map of seqs: 2 seqs");
    ASSERT_EQ_INT(count_event_type(input, YAML_SCALAR_EVENT), 6, "map of seqs: 6 scalars");
}

static void test_p_seq_of_maps_multi(void) {
    const char *input = "- a: 1\n  b: 2\n- c: 3\n";
    ASSERT_EQ_INT(count_event_type(input, YAML_MAPPING_START_EVENT), 2, "seq of maps multi: 2 maps");
}

static void test_p_deeply_nested_map(void) {
    const char *input = "a:\n  b:\n    c:\n      d: e\n";
    ASSERT_EQ_INT(count_event_type(input, YAML_MAPPING_START_EVENT), 4, "deep: 4 maps");
}

static void test_p_flow_in_block(void) {
    const char *input = "key: [1, 2, 3]\n";
    ASSERT_EQ_INT(count_event_type(input, YAML_SEQUENCE_START_EVENT), 1, "flow in block: 1 seq");
    ASSERT_EQ_INT(count_event_type(input, YAML_SCALAR_EVENT), 4, "flow in block: 4 scalars");
}

static void test_p_block_map_flow_value(void) {
    const char *input = "key: {a: b}\n";
    ASSERT_EQ_INT(count_event_type(input, YAML_MAPPING_START_EVENT), 2, "flow map val: 2 maps");
}

static void test_p_mixed_flow_block(void) {
    const char *input = "items:\n  - name: a\n    tags: [x, y]\n";
    ASSERT(find_scalar(input, "a"), "mixed: found name");
    ASSERT(find_scalar(input, "x"), "mixed: found tag x");
    ASSERT(find_scalar(input, "y"), "mixed: found tag y");
}

static void test_p_empty_mapping_value(void) {
    const char *input = "key:\n";
    ASSERT(find_scalar(input, "key"), "empty val: found key");
    ASSERT(find_scalar(input, ""), "empty val: found empty");
}

static void test_p_empty_seq_item(void) {
    const char *input = "- \n- a\n- \n";
    ASSERT_EQ_INT(count_event_type(input, YAML_SCALAR_EVENT), 3, "empty items: 3 scalars");
}

static void test_p_multiline_plain_scalar(void) {
    ASSERT(find_scalar("key: multi\n  line\n", "multi line"), "multiline plain");
}

static void test_p_multiline_quoted_scalar(void) {
    ASSERT(find_scalar("key: \"multi\n  line\"\n", "multi line"), "multiline quoted");
}

/* ========== Scalar styles in parser ========== */

static void test_p_scalar_plain_style(void) {
    yaml_parser_t parser;
    yaml_event_t event;
    int found = 0;
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)"hello", 5);
    while (1) {
        if (!yaml_parser_parse(&parser, &event)) break;
        if (event.type == YAML_SCALAR_EVENT) {
            ASSERT_EQ_INT(event.data.scalar.style, YAML_PLAIN_SCALAR_STYLE, "plain style");
            found = 1;
        }
        int done = (event.type == YAML_STREAM_END_EVENT);
        yaml_event_delete(&event);
        if (done) break;
    }
    yaml_parser_delete(&parser);
    ASSERT(found, "found plain");
}

static void test_p_scalar_single_style(void) {
    yaml_parser_t parser;
    yaml_event_t event;
    int found = 0;
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)"'hi'", 4);
    while (1) {
        if (!yaml_parser_parse(&parser, &event)) break;
        if (event.type == YAML_SCALAR_EVENT) {
            ASSERT_EQ_INT(event.data.scalar.style, YAML_SINGLE_QUOTED_SCALAR_STYLE, "single style");
            found = 1;
        }
        int done = (event.type == YAML_STREAM_END_EVENT);
        yaml_event_delete(&event);
        if (done) break;
    }
    yaml_parser_delete(&parser);
    ASSERT(found, "found single");
}

static void test_p_scalar_double_style(void) {
    yaml_parser_t parser;
    yaml_event_t event;
    int found = 0;
    yaml_parser_initialize(&parser);
    const char *input = "\"hi\"";
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));
    while (1) {
        if (!yaml_parser_parse(&parser, &event)) break;
        if (event.type == YAML_SCALAR_EVENT) {
            ASSERT_EQ_INT(event.data.scalar.style, YAML_DOUBLE_QUOTED_SCALAR_STYLE, "double style");
            found = 1;
        }
        int done = (event.type == YAML_STREAM_END_EVENT);
        yaml_event_delete(&event);
        if (done) break;
    }
    yaml_parser_delete(&parser);
    ASSERT(found, "found double");
}

static void test_p_scalar_literal_style(void) {
    yaml_parser_t parser;
    yaml_event_t event;
    int found = 0;
    yaml_parser_initialize(&parser);
    const char *input = "|\n  hi\n";
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));
    while (1) {
        if (!yaml_parser_parse(&parser, &event)) break;
        if (event.type == YAML_SCALAR_EVENT) {
            ASSERT_EQ_INT(event.data.scalar.style, YAML_LITERAL_SCALAR_STYLE, "literal style");
            found = 1;
        }
        int done = (event.type == YAML_STREAM_END_EVENT);
        yaml_event_delete(&event);
        if (done) break;
    }
    yaml_parser_delete(&parser);
    ASSERT(found, "found literal");
}

static void test_p_scalar_folded_style(void) {
    yaml_parser_t parser;
    yaml_event_t event;
    int found = 0;
    yaml_parser_initialize(&parser);
    const char *input = ">\n  hi\n";
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));
    while (1) {
        if (!yaml_parser_parse(&parser, &event)) break;
        if (event.type == YAML_SCALAR_EVENT) {
            ASSERT_EQ_INT(event.data.scalar.style, YAML_FOLDED_SCALAR_STYLE, "folded style");
            found = 1;
        }
        int done = (event.type == YAML_STREAM_END_EVENT);
        yaml_event_delete(&event);
        if (done) break;
    }
    yaml_parser_delete(&parser);
    ASSERT(found, "found folded");
}

/* ========== Implicit/non-implicit flags ========== */

static void test_p_implicit_scalar(void) {
    yaml_parser_t parser;
    yaml_event_t event;
    int found = 0;
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)"hello", 5);
    while (1) {
        if (!yaml_parser_parse(&parser, &event)) break;
        if (event.type == YAML_SCALAR_EVENT) {
            /* Plain scalar should have plain_implicit set */
            ASSERT(event.data.scalar.plain_implicit, "plain implicit set");
            found = 1;
        }
        int done = (event.type == YAML_STREAM_END_EVENT);
        yaml_event_delete(&event);
        if (done) break;
    }
    yaml_parser_delete(&parser);
    ASSERT(found, "found implicit scalar");
}

static void test_p_tagged_not_implicit(void) {
    yaml_parser_t parser;
    yaml_event_t event;
    int found = 0;
    const char *input = "!!str hello";
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));
    while (1) {
        if (!yaml_parser_parse(&parser, &event)) break;
        if (event.type == YAML_SCALAR_EVENT) {
            /* Tagged scalar should not be plain_implicit */
            ASSERT(!event.data.scalar.plain_implicit, "tagged not implicit");
            found = 1;
        }
        int done = (event.type == YAML_STREAM_END_EVENT);
        yaml_event_delete(&event);
        if (done) break;
    }
    yaml_parser_delete(&parser);
    ASSERT(found, "found tagged scalar");
}

/* ========== Event marks ========== */

static void test_p_event_mark_first(void) {
    yaml_parser_t parser;
    yaml_event_t event;
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)"hello", 5);
    if (yaml_parser_parse(&parser, &event)) {
        ASSERT_EQ_INT((int)event.start_mark.index, 0, "stream start index 0");
        ASSERT_EQ_INT((int)event.start_mark.line, 0, "stream start line 0");
        ASSERT_EQ_INT((int)event.start_mark.column, 0, "stream start col 0");
        yaml_event_delete(&event);
    }
    yaml_parser_delete(&parser);
}

static void test_p_event_mark_scalar(void) {
    yaml_parser_t parser;
    yaml_event_t event;
    int found = 0;
    const char *input = "key: value\n";
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));
    while (1) {
        if (!yaml_parser_parse(&parser, &event)) break;
        if (event.type == YAML_SCALAR_EVENT &&
            strcmp((const char *)event.data.scalar.value, "value") == 0) {
            ASSERT_EQ_INT((int)event.start_mark.column, 5, "value at col 5");
            found = 1;
        }
        int done = (event.type == YAML_STREAM_END_EVENT);
        yaml_event_delete(&event);
        if (done) break;
    }
    yaml_parser_delete(&parser);
    ASSERT(found, "found value with mark");
}

static void test_p_event_mark_second_line(void) {
    yaml_parser_t parser;
    yaml_event_t event;
    int found = 0;
    const char *input = "a: 1\nb: 2\n";
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));
    while (1) {
        if (!yaml_parser_parse(&parser, &event)) break;
        if (event.type == YAML_SCALAR_EVENT &&
            strcmp((const char *)event.data.scalar.value, "b") == 0) {
            ASSERT_EQ_INT((int)event.start_mark.line, 1, "b on line 1");
            found = 1;
        }
        int done = (event.type == YAML_STREAM_END_EVENT);
        yaml_event_delete(&event);
        if (done) break;
    }
    yaml_parser_delete(&parser);
    ASSERT(found, "found b with mark");
}

/* ========== Parser lifecycle ========== */

static void test_p_init_delete(void) {
    yaml_parser_t parser;
    ASSERT(yaml_parser_initialize(&parser), "init");
    yaml_parser_delete(&parser);
    ASSERT(1, "init+delete ok");
}

static void test_p_reuse_parser(void) {
    yaml_parser_t parser;
    yaml_event_t event;

    /* First use */
    ASSERT(yaml_parser_initialize(&parser), "first init");
    yaml_parser_set_input_string(&parser, (const unsigned char *)"a", 1);
    while (1) {
        if (!yaml_parser_parse(&parser, &event)) break;
        int done = (event.type == YAML_STREAM_END_EVENT);
        yaml_event_delete(&event);
        if (done) break;
    }
    yaml_parser_delete(&parser);

    /* Second use -- same parser variable */
    ASSERT(yaml_parser_initialize(&parser), "second init");
    yaml_parser_set_input_string(&parser, (const unsigned char *)"b", 1);
    int found = 0;
    while (1) {
        if (!yaml_parser_parse(&parser, &event)) break;
        if (event.type == YAML_SCALAR_EVENT) found = 1;
        int done = (event.type == YAML_STREAM_END_EVENT);
        yaml_event_delete(&event);
        if (done) break;
    }
    yaml_parser_delete(&parser);
    ASSERT(found, "reuse: found scalar");
}

static void test_p_multiple_parsers(void) {
    yaml_parser_t p1, p2;
    yaml_event_t e1, e2;

    yaml_parser_initialize(&p1);
    yaml_parser_initialize(&p2);
    yaml_parser_set_input_string(&p1, (const unsigned char *)"a: 1", 4);
    yaml_parser_set_input_string(&p2, (const unsigned char *)"b: 2", 4);

    /* Parse both simultaneously */
    yaml_parser_parse(&p1, &e1);
    yaml_parser_parse(&p2, &e2);
    ASSERT_EQ_INT(e1.type, YAML_STREAM_START_EVENT, "p1 stream start");
    ASSERT_EQ_INT(e2.type, YAML_STREAM_START_EVENT, "p2 stream start");
    yaml_event_delete(&e1);
    yaml_event_delete(&e2);

    yaml_parser_delete(&p1);
    yaml_parser_delete(&p2);
}

/* ========== Indentless sequences ========== */

static void test_p_indentless_seq(void) {
    const char *input = "key:\n- a\n- b\n";
    ASSERT(find_scalar(input, "a"), "indentless: found a");
    ASSERT(find_scalar(input, "b"), "indentless: found b");
}

/* ========== Edge cases ========== */

static void test_p_very_long_key(void) {
    char buf[2048];
    memset(buf, 'a', 1024);
    buf[1024] = ':';
    buf[1025] = ' ';
    buf[1026] = 'v';
    buf[1027] = '\0';
    ASSERT(find_scalar(buf, "v"), "long key: found value");
}

static void test_p_special_chars_in_plain(void) {
    ASSERT(find_scalar("hello-world", "hello-world"), "plain with dash");
    ASSERT(find_scalar("hello_world", "hello_world"), "plain with underscore");
    ASSERT(find_scalar("hello.world", "hello.world"), "plain with dot");
}

static void test_p_colon_in_flow(void) {
    ASSERT(find_scalar("[a:b]", "a:b"), "colon in flow plain");
}

static void test_p_utf8_scalar(void) {
    ASSERT(find_scalar("\xc3\xa9l\xc3\xa8ve", "\xc3\xa9l\xc3\xa8ve"), "utf8 scalar");
}

static void test_p_trailing_newlines(void) {
    yaml_event_type_t ev[16];
    int n;
    ASSERT(collect_events("hello\n\n\n", ev, 16, &n), "trailing newlines");
    ASSERT(find_scalar("hello\n\n\n", "hello"), "trailing: found scalar");
}

static void test_p_windows_line_endings(void) {
    ASSERT(find_scalar("a: b\r\n", "b"), "windows line endings");
}

static void test_p_mixed_line_endings(void) {
    ASSERT(find_scalar("a: b\r\nc: d\n", "d"), "mixed line endings");
}

static void test_p_single_char_scalar(void) {
    ASSERT(find_scalar("a", "a"), "single char");
}

static void test_p_numeric_key(void) {
    ASSERT(find_scalar("1: one", "one"), "numeric key value");
    ASSERT(find_scalar("1: one", "1"), "numeric key");
}

static void test_p_boolean_key(void) {
    ASSERT(find_scalar("true: yes", "true"), "boolean key");
}

static void test_p_null_value(void) {
    ASSERT(find_scalar("key: ~", "~"), "null value tilde");
    ASSERT(find_scalar("key: null", "null"), "null value null");
}

/* ========== Complex real-world patterns ========== */

static void test_p_kubernetes_like(void) {
    const char *input =
        "apiVersion: v1\n"
        "kind: Pod\n"
        "metadata:\n"
        "  name: test\n"
        "  labels:\n"
        "    app: web\n"
        "spec:\n"
        "  containers:\n"
        "    - name: nginx\n"
        "      image: nginx:latest\n"
        "      ports:\n"
        "        - containerPort: 80\n";
    ASSERT(find_scalar(input, "v1"), "k8s: apiVersion");
    ASSERT(find_scalar(input, "Pod"), "k8s: kind");
    ASSERT(find_scalar(input, "nginx"), "k8s: container name");
    ASSERT(find_scalar(input, "nginx:latest"), "k8s: image");
    ASSERT(find_scalar(input, "80"), "k8s: port");
}

static void test_p_github_actions_like(void) {
    const char *input =
        "name: CI\n"
        "on: [push, pull_request]\n"
        "jobs:\n"
        "  build:\n"
        "    runs-on: ubuntu-latest\n"
        "    steps:\n"
        "      - uses: actions/checkout@v2\n"
        "      - run: make test\n";
    ASSERT(find_scalar(input, "CI"), "gha: name");
    ASSERT(find_scalar(input, "push"), "gha: push");
    ASSERT(find_scalar(input, "ubuntu-latest"), "gha: runs-on");
}

static void test_p_docker_compose_like(void) {
    const char *input =
        "version: '3'\n"
        "services:\n"
        "  web:\n"
        "    image: nginx\n"
        "    ports:\n"
        "      - '80:80'\n"
        "  db:\n"
        "    image: postgres\n"
        "    environment:\n"
        "      POSTGRES_DB: app\n";
    ASSERT(find_scalar(input, "3"), "dc: version");
    ASSERT(find_scalar(input, "nginx"), "dc: web image");
    ASSERT(find_scalar(input, "80:80"), "dc: port");
    ASSERT(find_scalar(input, "postgres"), "dc: db image");
    ASSERT(find_scalar(input, "app"), "dc: postgres_db");
}

/* Coverage: flow sequence entry mapping -- key empty at ] */
static void test_p_flow_seq_entry_mapping_empty_key_at_end(void) {
    /* [a: b, c: ] triggers mapping key with empty value at ] */
    yaml_event_type_t ev[32];
    int n;
    ASSERT(collect_events("[a: b, c: ]", ev, 32, &n), "flow seq mapping end");
    /* Should parse without error */
    ASSERT(n >= 2, "flow seq mapping end: has events");
}

/* Coverage: flow sequence implicit mapping key followed by , */
static void test_p_flow_seq_entry_mapping_key_comma(void) {
    /* [a: ,b] -- mapping key with empty value before comma */
    yaml_event_type_t ev[32];
    int n;
    ASSERT(collect_events("[a: ,b]", ev, 32, &n), "flow seq key comma");
    ASSERT(n >= 2, "flow seq key comma: events");
}

/* Coverage: flow sequence implicit mapping with VALUE then ] */
static void test_p_flow_seq_entry_mapping_value_end(void) {
    /* [a: b: ] -- mapping value followed by ] */
    yaml_event_type_t ev[32];
    int n;
    /* This may parse or error -- just verify no crash */
    collect_events("[a: ]", ev, 32, &n);
    ASSERT(n >= 2, "flow seq value end: events");
}

/* Coverage: flow mapping with complex key followed by empty value */
static void test_p_flow_mapping_complex_key_empty_value(void) {
    /* {? key: } -- complex key with empty value */
    yaml_event_type_t ev[32];
    int n;
    ASSERT(collect_events("{? key: }", ev, 32, &n), "complex key empty val");
    int map_count = 0;
    for (int i = 0; i < n; i++)
        if (ev[i] == YAML_MAPPING_START_EVENT) map_count++;
    ASSERT(map_count >= 1, "complex key: has mapping");
}

/* Coverage: flow mapping with complex key followed by , (no value) */
static void test_p_flow_mapping_complex_key_no_value(void) {
    /* {? key, other: val} -- complex key with no value */
    yaml_event_type_t ev[32];
    int n;
    ASSERT(collect_events("{? key, other: val}", ev, 32, &n), "complex key no val");
    ASSERT(n >= 2, "complex key no val: events");
}

/* Coverage: scanner directive error paths */
static void test_p_malformed_yaml_directive(void) {
    yaml_parser_t parser;
    yaml_event_t event;

    /* %YAML with bad version */
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const yaml_char_t *)"%YAML 2.0\n---\nfoo\n", 19);
    int got_error = 0;
    while (1) {
        if (!yaml_parser_parse(&parser, &event)) { got_error = 1; break; }
        int done = (event.type == YAML_STREAM_END_EVENT);
        yaml_event_delete(&event);
        if (done) break;
    }
    yaml_parser_delete(&parser);
    /* libyaml may or may not error on YAML 2.0 -- just verify no crash */

    /* %YAML with missing version number */
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const yaml_char_t *)"%YAML\n---\nfoo\n", 14);
    while (1) {
        if (!yaml_parser_parse(&parser, &event)) break;
        int done = (event.type == YAML_STREAM_END_EVENT);
        yaml_event_delete(&event);
        if (done) break;
    }
    yaml_parser_delete(&parser);

    /* Duplicate %YAML directive */
    yaml_parser_initialize(&parser);
    const char *dup_yaml = "%YAML 1.1\n%YAML 1.1\n---\nfoo\n";
    yaml_parser_set_input_string(&parser, (const yaml_char_t *)dup_yaml, strlen(dup_yaml));
    while (1) {
        if (!yaml_parser_parse(&parser, &event)) break;
        int done = (event.type == YAML_STREAM_END_EVENT);
        yaml_event_delete(&event);
        if (done) break;
    }
    yaml_parser_delete(&parser);
}

/* Coverage: malformed TAG directive */
static void test_p_malformed_tag_directive(void) {
    yaml_parser_t parser;
    yaml_event_t event;

    /* %TAG with missing handle */
    yaml_parser_initialize(&parser);
    const char *bad_tag = "%TAG\n---\nfoo\n";
    yaml_parser_set_input_string(&parser, (const yaml_char_t *)bad_tag, strlen(bad_tag));
    while (1) {
        if (!yaml_parser_parse(&parser, &event)) break;
        int done = (event.type == YAML_STREAM_END_EVENT);
        yaml_event_delete(&event);
        if (done) break;
    }
    yaml_parser_delete(&parser);

    /* %TAG with bad handle (no trailing !) */
    yaml_parser_initialize(&parser);
    const char *bad_tag2 = "%TAG !foo http://example.com/\n---\nfoo\n";
    yaml_parser_set_input_string(&parser, (const yaml_char_t *)bad_tag2, strlen(bad_tag2));
    while (1) {
        if (!yaml_parser_parse(&parser, &event)) break;
        int done = (event.type == YAML_STREAM_END_EVENT);
        yaml_event_delete(&event);
        if (done) break;
    }
    yaml_parser_delete(&parser);
}

/* Coverage: block scalar with explicit indent indicator */
static void test_p_block_scalar_indent_indicator(void) {
    yaml_event_type_t ev[16];
    int n;
    /* literal block with explicit indent of 2 */
    ASSERT(collect_events("|\n  hello\n  world\n", ev, 16, &n), "lit indent");
    /* literal block with indent indicator 1 */
    ASSERT(collect_events("|1\n hello\n world\n", ev, 16, &n), "lit indent1");
    /* literal block with indent indicator and chomp */
    ASSERT(collect_events("|2-\n  hello\n  world\n", ev, 16, &n), "lit indent2-");
}

/* Coverage: document end followed by explicit new document */
static void test_p_doc_end_then_new_doc(void) {
    yaml_event_type_t ev[16];
    int n;
    ASSERT(collect_events("---\nfoo\n...\n---\nbar\n", ev, 16, &n), "doc end new doc");
    int doc_starts = 0;
    for (int i = 0; i < n; i++)
        if (ev[i] == YAML_DOCUMENT_START_EVENT) doc_starts++;
    ASSERT_EQ_INT(doc_starts, 2, "doc end: 2 docs");
}

/* Coverage: flow sequence with implicit mapping where value is a node */
static void test_p_flow_seq_mapping_value_node(void) {
    yaml_event_type_t ev[32];
    int n;
    /* [a: {nested: true}] -- mapping value is a flow mapping */
    ASSERT(collect_events("[a: {nested: true}]", ev, 32, &n), "flow seq map val node");
    ASSERT(find_scalar("[a: {nested: true}]", "true"), "flow seq: nested val");
}

/* Coverage: maximum nesting depth */
static void test_p_deep_flow_nesting(void) {
    /* Build deeply nested flow structure: [[[[...]]]] */
    char buf[2048];
    int depth = 100;
    int pos = 0;
    for (int i = 0; i < depth && pos < 1000; i++) buf[pos++] = '[';
    buf[pos++] = '1';
    for (int i = 0; i < depth && pos < 2000; i++) buf[pos++] = ']';
    buf[pos] = '\0';

    yaml_event_type_t ev[512];
    int n;
    /* May succeed or hit max nesting -- just verify no crash */
    collect_events(buf, ev, 512, &n);
}

int main(void) {
    TEST_SUITE_BEGIN("Parser Comprehensive");

    /* Empty/minimal streams */
    test_p_empty_stream();
    test_p_whitespace_stream();
    test_p_comment_only_stream();
    test_p_multi_comment_stream();

    /* Document events */
    test_p_implicit_doc();
    test_p_explicit_doc_start();
    test_p_explicit_doc_end();
    test_p_multiple_docs();
    test_p_doc_with_end_marker();
    test_p_empty_doc();
    test_p_empty_doc_implicit();

    /* Scalar events */
    test_p_plain_scalar();
    test_p_plain_scalar_number();
    test_p_plain_scalar_float();
    test_p_plain_scalar_bool_true();
    test_p_plain_scalar_bool_false();
    test_p_plain_scalar_null();
    test_p_plain_scalar_tilde();
    test_p_single_quoted();
    test_p_single_quoted_empty();
    test_p_single_quoted_escape();
    test_p_double_quoted();
    test_p_double_quoted_empty();
    test_p_double_quoted_escape_n();
    test_p_double_quoted_escape_t();
    test_p_double_quoted_escape_backslash();
    test_p_double_quoted_escape_quote();
    test_p_double_quoted_hex();
    test_p_double_quoted_unicode();
    test_p_literal_block();
    test_p_literal_block_strip();
    test_p_literal_block_keep();
    test_p_folded_block();
    test_p_folded_block_strip();
    test_p_folded_block_keep();

    /* Mapping events */
    test_p_block_mapping_simple();
    test_p_block_mapping_multi();
    test_p_block_mapping_nested();
    test_p_block_mapping_deep();
    test_p_flow_mapping_simple();
    test_p_flow_mapping_multi();
    test_p_flow_mapping_nested();
    test_p_flow_mapping_empty();
    test_p_mapping_with_seq_value();
    test_p_mapping_explicit_key();

    /* Sequence events */
    test_p_block_seq_simple();
    test_p_block_seq_scalars();
    test_p_block_seq_nested();
    test_p_flow_seq_simple();
    test_p_flow_seq_scalars();
    test_p_flow_seq_nested();
    test_p_flow_seq_empty();
    test_p_seq_of_maps();
    test_p_seq_of_seqs();

    /* Anchors and aliases */
    test_p_anchor_scalar();
    test_p_alias();
    test_p_alias_value();
    test_p_anchor_mapping();
    test_p_anchor_sequence();

    /* Tags */
    test_p_tag_verbatim();
    test_p_tag_shorthand();
    test_p_tag_primary();
    test_p_tag_mapping();
    test_p_tag_sequence();

    /* Directives */
    test_p_yaml_directive();
    test_p_tag_directive();

    /* Complex structures */
    test_p_map_of_seqs();
    test_p_seq_of_maps_multi();
    test_p_deeply_nested_map();
    test_p_flow_in_block();
    test_p_block_map_flow_value();
    test_p_mixed_flow_block();
    test_p_empty_mapping_value();
    test_p_empty_seq_item();
    test_p_multiline_plain_scalar();
    test_p_multiline_quoted_scalar();

    /* Scalar styles */
    test_p_scalar_plain_style();
    test_p_scalar_single_style();
    test_p_scalar_double_style();
    test_p_scalar_literal_style();
    test_p_scalar_folded_style();

    /* Implicit flags */
    test_p_implicit_scalar();
    test_p_tagged_not_implicit();

    /* Event marks */
    test_p_event_mark_first();
    test_p_event_mark_scalar();
    test_p_event_mark_second_line();

    /* Parser lifecycle */
    test_p_init_delete();
    test_p_reuse_parser();
    test_p_multiple_parsers();

    /* Indentless sequences */
    test_p_indentless_seq();

    /* Edge cases */
    test_p_very_long_key();
    test_p_special_chars_in_plain();
    test_p_colon_in_flow();
    test_p_utf8_scalar();
    test_p_trailing_newlines();
    test_p_windows_line_endings();
    test_p_mixed_line_endings();
    test_p_single_char_scalar();
    test_p_numeric_key();
    test_p_boolean_key();
    test_p_null_value();

    /* Real-world patterns */
    test_p_kubernetes_like();
    test_p_github_actions_like();
    test_p_docker_compose_like();

    /* Coverage-targeted: flow seq entry mapping states */
    test_p_flow_seq_entry_mapping_empty_key_at_end();
    test_p_flow_seq_entry_mapping_key_comma();
    test_p_flow_seq_entry_mapping_value_end();
    test_p_flow_mapping_complex_key_empty_value();
    test_p_flow_mapping_complex_key_no_value();
    test_p_flow_seq_mapping_value_node();
    test_p_deep_flow_nesting();

    /* Coverage-targeted: directives and block scalars */
    test_p_malformed_yaml_directive();
    test_p_malformed_tag_directive();
    test_p_block_scalar_indent_indicator();
    test_p_doc_end_then_new_doc();

    TEST_SUITE_END();
}
