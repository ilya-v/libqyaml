#include "test_helper.h"

/* Test scanning empty documents */
static void test_empty_stream(void) {
    yaml_token_type_t tokens[16];
    int count;

    ASSERT(scan_string_tokens("", tokens, 16, &count), "scan empty");
    ASSERT_EQ_INT(count, 2, "empty: 2 tokens");
    ASSERT_EQ_INT(tokens[0], YAML_STREAM_START_TOKEN, "empty: stream start");
    ASSERT_EQ_INT(tokens[1], YAML_STREAM_END_TOKEN, "empty: stream end");
}

/* Test scanning simple scalars */
static void test_simple_scalar(void) {
    yaml_token_type_t tokens[16];
    int count;

    ASSERT(scan_string_tokens("hello", tokens, 16, &count), "scan scalar");
    /* STREAM_START, BLOCK_MAPPING_START(?), SCALAR, STREAM_END or similar */
    ASSERT(count >= 2, "scalar: at least 2 tokens");
    ASSERT_EQ_INT(tokens[0], YAML_STREAM_START_TOKEN, "scalar: stream start");
    ASSERT_EQ_INT(tokens[count-1], YAML_STREAM_END_TOKEN, "scalar: stream end");
}

/* Test scanning block mapping */
static void test_block_mapping(void) {
    yaml_token_type_t tokens[32];
    int count;

    ASSERT(scan_string_tokens("key: value", tokens, 32, &count), "scan mapping");
    ASSERT_EQ_INT(tokens[0], YAML_STREAM_START_TOKEN, "mapping: stream start");

    /* Should contain KEY and VALUE tokens */
    int found_key = 0, found_value = 0;
    for (int i = 0; i < count; i++) {
        if (tokens[i] == YAML_KEY_TOKEN) found_key = 1;
        if (tokens[i] == YAML_VALUE_TOKEN) found_value = 1;
    }
    ASSERT(found_key, "mapping: has KEY token");
    ASSERT(found_value, "mapping: has VALUE token");
}

/* Test scanning block sequence */
static void test_block_sequence(void) {
    yaml_token_type_t tokens[32];
    int count;

    ASSERT(scan_string_tokens("- item1\n- item2\n- item3\n", tokens, 32, &count),
           "scan sequence");

    int block_entry_count = 0;
    for (int i = 0; i < count; i++) {
        if (tokens[i] == YAML_BLOCK_ENTRY_TOKEN) block_entry_count++;
    }
    ASSERT_EQ_INT(block_entry_count, 3, "sequence: 3 entries");
}

/* Test scanning flow sequence */
static void test_flow_sequence(void) {
    yaml_token_type_t tokens[32];
    int count;

    ASSERT(scan_string_tokens("[a, b, c]", tokens, 32, &count), "scan flow seq");

    int found_start = 0, found_end = 0, flow_entry_count = 0;
    for (int i = 0; i < count; i++) {
        if (tokens[i] == YAML_FLOW_SEQUENCE_START_TOKEN) found_start = 1;
        if (tokens[i] == YAML_FLOW_SEQUENCE_END_TOKEN) found_end = 1;
        if (tokens[i] == YAML_FLOW_ENTRY_TOKEN) flow_entry_count++;
    }
    ASSERT(found_start, "flow seq: start token");
    ASSERT(found_end, "flow seq: end token");
    ASSERT_EQ_INT(flow_entry_count, 2, "flow seq: 2 separators");
}

/* Test scanning flow mapping */
static void test_flow_mapping(void) {
    yaml_token_type_t tokens[32];
    int count;

    ASSERT(scan_string_tokens("{a: 1, b: 2}", tokens, 32, &count), "scan flow map");

    int found_start = 0, found_end = 0;
    for (int i = 0; i < count; i++) {
        if (tokens[i] == YAML_FLOW_MAPPING_START_TOKEN) found_start = 1;
        if (tokens[i] == YAML_FLOW_MAPPING_END_TOKEN) found_end = 1;
    }
    ASSERT(found_start, "flow map: start token");
    ASSERT(found_end, "flow map: end token");
}

/* Test scanning quoted scalars */
static void test_quoted_scalars(void) {
    yaml_parser_t parser;
    yaml_token_t token;
    int found_scalar = 0;

    /* Single-quoted */
    ASSERT(yaml_parser_initialize(&parser), "init single-quoted");
    yaml_parser_set_input_string(&parser, (const unsigned char *)"'hello world'", 13);
    while (1) {
        ASSERT(yaml_parser_scan(&parser, &token), "scan single-quoted token");
        if (token.type == YAML_SCALAR_TOKEN) {
            ASSERT_EQ_STR((const char *)token.data.scalar.value, "hello world",
                         "single-quoted value");
            ASSERT_EQ_INT(token.data.scalar.style, YAML_SINGLE_QUOTED_SCALAR_STYLE,
                         "single-quoted style");
            found_scalar = 1;
        }
        if (token.type == YAML_STREAM_END_TOKEN) {
            yaml_token_delete(&token);
            break;
        }
        yaml_token_delete(&token);
    }
    ASSERT(found_scalar, "found single-quoted scalar");
    yaml_parser_delete(&parser);

    /* Double-quoted */
    found_scalar = 0;
    ASSERT(yaml_parser_initialize(&parser), "init double-quoted");
    yaml_parser_set_input_string(&parser, (const unsigned char *)"\"hello\\nworld\"", 14);
    while (1) {
        ASSERT(yaml_parser_scan(&parser, &token), "scan double-quoted token");
        if (token.type == YAML_SCALAR_TOKEN) {
            ASSERT_EQ_STR((const char *)token.data.scalar.value, "hello\nworld",
                         "double-quoted value with escape");
            ASSERT_EQ_INT(token.data.scalar.style, YAML_DOUBLE_QUOTED_SCALAR_STYLE,
                         "double-quoted style");
            found_scalar = 1;
        }
        if (token.type == YAML_STREAM_END_TOKEN) {
            yaml_token_delete(&token);
            break;
        }
        yaml_token_delete(&token);
    }
    ASSERT(found_scalar, "found double-quoted scalar");
    yaml_parser_delete(&parser);
}

/* Test scanning anchors and aliases */
static void test_anchors_aliases(void) {
    yaml_token_type_t tokens[32];
    int count;

    ASSERT(scan_string_tokens("&anchor value", tokens, 32, &count), "scan anchor");
    int found_anchor = 0;
    for (int i = 0; i < count; i++) {
        if (tokens[i] == YAML_ANCHOR_TOKEN) found_anchor = 1;
    }
    ASSERT(found_anchor, "anchor: found anchor token");

    ASSERT(scan_string_tokens("*alias", tokens, 32, &count), "scan alias");
    int found_alias = 0;
    for (int i = 0; i < count; i++) {
        if (tokens[i] == YAML_ALIAS_TOKEN) found_alias = 1;
    }
    ASSERT(found_alias, "alias: found alias token");
}

/* Test scanning tags */
static void test_tags(void) {
    yaml_token_type_t tokens[32];
    int count;

    ASSERT(scan_string_tokens("!!str hello", tokens, 32, &count), "scan tag");
    int found_tag = 0;
    for (int i = 0; i < count; i++) {
        if (tokens[i] == YAML_TAG_TOKEN) found_tag = 1;
    }
    ASSERT(found_tag, "tag: found tag token");
}

/* Test scanning document markers */
static void test_document_markers(void) {
    yaml_token_type_t tokens[32];
    int count;

    ASSERT(scan_string_tokens("---\nhello\n...\n", tokens, 32, &count), "scan doc markers");
    int found_doc_start = 0, found_doc_end = 0;
    for (int i = 0; i < count; i++) {
        if (tokens[i] == YAML_DOCUMENT_START_TOKEN) found_doc_start = 1;
        if (tokens[i] == YAML_DOCUMENT_END_TOKEN) found_doc_end = 1;
    }
    ASSERT(found_doc_start, "doc markers: found doc start");
    ASSERT(found_doc_end, "doc markers: found doc end");
}

/* Test scanning version directive */
static void test_version_directive(void) {
    yaml_token_type_t tokens[32];
    int count;

    ASSERT(scan_string_tokens("%YAML 1.1\n---\nhello\n", tokens, 32, &count),
           "scan version directive");
    int found_directive = 0;
    for (int i = 0; i < count; i++) {
        if (tokens[i] == YAML_VERSION_DIRECTIVE_TOKEN) found_directive = 1;
    }
    ASSERT(found_directive, "version directive: found");
}

/* Test scanning tag directive */
static void test_tag_directive(void) {
    yaml_token_type_t tokens[32];
    int count;

    ASSERT(scan_string_tokens("%TAG !t! tag:example.com,2000:\n---\nhello\n",
           tokens, 32, &count), "scan tag directive");
    int found_directive = 0;
    for (int i = 0; i < count; i++) {
        if (tokens[i] == YAML_TAG_DIRECTIVE_TOKEN) found_directive = 1;
    }
    ASSERT(found_directive, "tag directive: found");
}

/* Test scanning comments (should be ignored) */
static void test_comments(void) {
    yaml_token_type_t tokens1[32], tokens2[32];
    int count1, count2;

    ASSERT(scan_string_tokens("key: value", tokens1, 32, &count1), "scan no comment");
    ASSERT(scan_string_tokens("key: value # comment", tokens2, 32, &count2), "scan with comment");
    ASSERT_EQ_INT(count1, count2, "comments: same token count");
}

/* Test scanning block scalars (literal and folded) */
static void test_block_scalars(void) {
    yaml_parser_t parser;
    yaml_token_t token;

    /* Literal scalar */
    const char *literal = "data: |\n  line1\n  line2\n";
    ASSERT(yaml_parser_initialize(&parser), "init literal");
    yaml_parser_set_input_string(&parser, (const unsigned char *)literal, strlen(literal));
    int found_literal = 0;
    while (1) {
        ASSERT(yaml_parser_scan(&parser, &token), "scan literal token");
        if (token.type == YAML_SCALAR_TOKEN &&
            token.data.scalar.style == YAML_LITERAL_SCALAR_STYLE) {
            found_literal = 1;
        }
        if (token.type == YAML_STREAM_END_TOKEN) {
            yaml_token_delete(&token);
            break;
        }
        yaml_token_delete(&token);
    }
    ASSERT(found_literal, "literal: found literal scalar");
    yaml_parser_delete(&parser);

    /* Folded scalar */
    const char *folded = "data: >\n  line1\n  line2\n";
    ASSERT(yaml_parser_initialize(&parser), "init folded");
    yaml_parser_set_input_string(&parser, (const unsigned char *)folded, strlen(folded));
    int found_folded = 0;
    while (1) {
        ASSERT(yaml_parser_scan(&parser, &token), "scan folded token");
        if (token.type == YAML_SCALAR_TOKEN &&
            token.data.scalar.style == YAML_FOLDED_SCALAR_STYLE) {
            found_folded = 1;
        }
        if (token.type == YAML_STREAM_END_TOKEN) {
            yaml_token_delete(&token);
            break;
        }
        yaml_token_delete(&token);
    }
    ASSERT(found_folded, "folded: found folded scalar");
    yaml_parser_delete(&parser);
}

/* Test scanning nested structures */
static void test_nested(void) {
    const char *yaml = "outer:\n  inner:\n    - item1\n    - item2\n";
    yaml_token_type_t tokens[64];
    int count;

    ASSERT(scan_string_tokens(yaml, tokens, 64, &count), "scan nested");

    int block_seq_start = 0, block_map_start = 0;
    for (int i = 0; i < count; i++) {
        if (tokens[i] == YAML_BLOCK_SEQUENCE_START_TOKEN) block_seq_start++;
        if (tokens[i] == YAML_BLOCK_MAPPING_START_TOKEN) block_map_start++;
    }
    ASSERT(block_seq_start >= 1, "nested: has block sequence start");
    ASSERT(block_map_start >= 2, "nested: has 2+ block mapping starts");
}

/* Test scanning multiline plain scalars */
static void test_multiline_plain(void) {
    yaml_parser_t parser;
    yaml_token_t token;
    const char *input = "key: multi\n  line\n  scalar\n";

    ASSERT(yaml_parser_initialize(&parser), "init multiline");
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));

    int found = 0;
    while (1) {
        ASSERT(yaml_parser_scan(&parser, &token), "scan multiline token");
        if (token.type == YAML_SCALAR_TOKEN &&
            strstr((const char *)token.data.scalar.value, "multi") != NULL) {
            found = 1;
        }
        if (token.type == YAML_STREAM_END_TOKEN) {
            yaml_token_delete(&token);
            break;
        }
        yaml_token_delete(&token);
    }
    ASSERT(found, "multiline: found multiline scalar");
    yaml_parser_delete(&parser);
}

/* Test token marks (positions) */
static void test_token_marks(void) {
    yaml_parser_t parser;
    yaml_token_t token;
    const char *input = "key: value\n";

    ASSERT(yaml_parser_initialize(&parser), "init marks");
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));

    /* stream start should be at position 0 */
    ASSERT(yaml_parser_scan(&parser, &token), "scan marks stream start");
    ASSERT_EQ_INT((int)token.start_mark.index, 0, "marks: stream start index=0");
    ASSERT_EQ_INT((int)token.start_mark.line, 0, "marks: stream start line=0");
    ASSERT_EQ_INT((int)token.start_mark.column, 0, "marks: stream start column=0");
    yaml_token_delete(&token);

    yaml_parser_delete(&parser);
}

int main(void) {
    TEST_SUITE_BEGIN("Scanner");

    test_empty_stream();
    test_simple_scalar();
    test_block_mapping();
    test_block_sequence();
    test_flow_sequence();
    test_flow_mapping();
    test_quoted_scalars();
    test_anchors_aliases();
    test_tags();
    test_document_markers();
    test_version_directive();
    test_tag_directive();
    test_comments();
    test_block_scalars();
    test_nested();
    test_multiline_plain();
    test_token_marks();

    TEST_SUITE_END();
}
