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

    TEST_SUITE_END();
}
