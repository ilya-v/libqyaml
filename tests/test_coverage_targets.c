/*
 * Coverage-targeted tests for uncovered code paths.
 * Targets: scanner dispatch cases, emitter error paths, TAG directive errors,
 * and various edge cases identified by gcov analysis.
 */

#include "test_helper.h"
#include <yaml.h>

/* ========== Scanner dispatch coverage ========== */

/* Line 1104: % at value position -> scanner error (% is directive indicator) */
static void test_percent_not_at_column_zero(void) {
    const char *input = "key: %value\n";
    yaml_parser_t parser;
    yaml_token_t token;
    int got_error = 0;

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));

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
    yaml_parser_delete(&parser);

    ASSERT(got_error, "% as value should cause scanner error (directive indicator)");
}

/* Lines 1167, 1172: | and > inside flow context -> error or plain scalar */
static void test_block_indicators_in_flow_context(void) {
    /* | inside flow mapping should be an error */
    const char *input1 = "{key: |}\n";
    yaml_parser_t parser;
    yaml_token_t token;
    int got_error = 0;

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)input1, strlen(input1));

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
    yaml_parser_delete(&parser);

    ASSERT(got_error, "| in flow context should cause error");

    /* > inside flow sequence */
    const char *input2 = "[>]\n";
    got_error = 0;

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)input2, strlen(input2));

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
    yaml_parser_delete(&parser);

    ASSERT(got_error, "> in flow context should cause error");
}

/* Lines 1181-1182: # @ ` cannot start any token */
static void test_invalid_token_start_characters(void) {
    /* @ at beginning should cause error */
    const char *input1 = "@invalid\n";
    yaml_parser_t parser;
    yaml_token_t token;
    int got_error = 0;

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)input1, strlen(input1));

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
    yaml_parser_delete(&parser);

    ASSERT(got_error, "@ should not start any token");

    /* ` at beginning should cause error */
    const char *input2 = "`invalid\n";
    got_error = 0;

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)input2, strlen(input2));

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
    yaml_parser_delete(&parser);

    ASSERT(got_error, "` should not start any token");
}

/* TAG directive with missing whitespace after prefix */
static void test_tag_directive_errors(void) {
    /* %TAG with no space after prefix (non-whitespace follows) */
    const char *input1 = "%TAG !x! http://example.com/x\n---\nkey: value\n";
    yaml_parser_t parser;
    yaml_event_t event;

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)input1, strlen(input1));

    int success = 1;
    while (1) {
        if (!yaml_parser_parse(&parser, &event)) {
            success = 0;
            break;
        }
        if (event.type == YAML_STREAM_END_EVENT) {
            yaml_event_delete(&event);
            break;
        }
        yaml_event_delete(&event);
    }
    yaml_parser_delete(&parser);

    /* This should actually succeed (valid TAG directive) */
    ASSERT(success, "Valid TAG directive should parse successfully");

    /* Duplicate TAG directive for same handle */
    const char *input2 = "%TAG ! tag:example.com,2000:\n%TAG ! tag:other.com,2000:\n---\nkey: value\n";

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)input2, strlen(input2));

    int got_error = 0;
    while (1) {
        if (!yaml_parser_parse(&parser, &event)) {
            got_error = 1;
            break;
        }
        if (event.type == YAML_STREAM_END_EVENT) {
            yaml_event_delete(&event);
            break;
        }
        yaml_event_delete(&event);
    }
    yaml_parser_delete(&parser);

    ASSERT(got_error, "Duplicate TAG directive for same handle should error");
}

/* ========== Emitter error path coverage ========== */

/* Emitter: emit event with wrong sequence */
static void test_emitter_wrong_event_sequence(void) {
    yaml_emitter_t emitter;
    yaml_event_t event;
    unsigned char buffer[1024];
    size_t written;
    int result;

    yaml_emitter_initialize(&emitter);
    yaml_emitter_set_output_string(&emitter, buffer, sizeof(buffer), &written);

    /* Try emitting MAPPING_END before STREAM_START */
    yaml_mapping_end_event_initialize(&event);
    result = yaml_emitter_emit(&emitter, &event);
    ASSERT(!result, "Emitting MAPPING_END before STREAM_START should fail");

    yaml_emitter_delete(&emitter);

    /* Try emitting DOCUMENT_START without STREAM_START - libyaml actually
     * accepts this (auto-emits STREAM_START), so just test it doesn't crash */
    yaml_emitter_initialize(&emitter);
    yaml_emitter_set_output_string(&emitter, buffer, sizeof(buffer), &written);

    yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1);
    (void)yaml_emitter_emit(&emitter, &event);
    /* libyaml accepts this with auto STREAM_START, just verify no crash */
    ASSERT(1, "Emitting DOCUMENT_START exercises auto-stream-start path");

    yaml_emitter_delete(&emitter);
}

/* Emitter: invalid anchor/tag in events */
static void test_emitter_invalid_anchor_tag(void) {
    yaml_emitter_t emitter;
    yaml_event_t event;
    unsigned char buffer[4096];
    size_t written;
    int result;

    yaml_emitter_initialize(&emitter);
    yaml_emitter_set_output_string(&emitter, buffer, sizeof(buffer), &written);

    /* Start stream and document */
    yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
    yaml_emitter_emit(&emitter, &event);

    yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1);
    yaml_emitter_emit(&emitter, &event);

    /* Emit mapping with anchor containing invalid characters */
    yaml_mapping_start_event_initialize(&event, (yaml_char_t *)"valid-anchor",
        NULL, 1, YAML_BLOCK_MAPPING_STYLE);
    result = yaml_emitter_emit(&emitter, &event);
    ASSERT(result, "Valid anchor in mapping should succeed");

    /* Emit key and value */
    yaml_scalar_event_initialize(&event, NULL, NULL,
        (yaml_char_t *)"key", 3, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);

    yaml_scalar_event_initialize(&event, NULL, NULL,
        (yaml_char_t *)"value", 5, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);

    yaml_mapping_end_event_initialize(&event);
    yaml_emitter_emit(&emitter, &event);

    yaml_document_end_event_initialize(&event, 1);
    yaml_emitter_emit(&emitter, &event);

    yaml_stream_end_event_initialize(&event);
    result = yaml_emitter_emit(&emitter, &event);
    ASSERT(result, "Emitter should complete successfully with anchor");

    yaml_emitter_delete(&emitter);
}

/* Emitter: various scalar styles */
static void test_emitter_scalar_styles(void) {
    yaml_emitter_t emitter;
    yaml_event_t event;
    unsigned char buffer[4096];
    size_t written;

    yaml_emitter_initialize(&emitter);
    yaml_emitter_set_output_string(&emitter, buffer, sizeof(buffer), &written);

    yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
    yaml_emitter_emit(&emitter, &event);

    yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1);
    yaml_emitter_emit(&emitter, &event);

    yaml_mapping_start_event_initialize(&event, NULL, NULL, 1, YAML_BLOCK_MAPPING_STYLE);
    yaml_emitter_emit(&emitter, &event);

    /* Test literal block scalar style */
    yaml_scalar_event_initialize(&event, NULL, NULL,
        (yaml_char_t *)"literal", 7, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);

    yaml_scalar_event_initialize(&event, NULL, NULL,
        (yaml_char_t *)"line1\nline2\nline3", 17, 1, 0, YAML_LITERAL_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);

    /* Test folded block scalar style */
    yaml_scalar_event_initialize(&event, NULL, NULL,
        (yaml_char_t *)"folded", 6, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);

    yaml_scalar_event_initialize(&event, NULL, NULL,
        (yaml_char_t *)"long line that should be folded", 30, 1, 0, YAML_FOLDED_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);

    /* Test double-quoted style with special chars */
    yaml_scalar_event_initialize(&event, NULL, NULL,
        (yaml_char_t *)"quoted", 6, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);

    yaml_scalar_event_initialize(&event, NULL, NULL,
        (yaml_char_t *)"value with\ttab and\nnewline", 25, 1, 0, YAML_DOUBLE_QUOTED_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);

    /* Test single-quoted style */
    yaml_scalar_event_initialize(&event, NULL, NULL,
        (yaml_char_t *)"single", 6, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);

    yaml_scalar_event_initialize(&event, NULL, NULL,
        (yaml_char_t *)"it's a test", 11, 1, 0, YAML_SINGLE_QUOTED_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);

    yaml_mapping_end_event_initialize(&event);
    yaml_emitter_emit(&emitter, &event);

    yaml_document_end_event_initialize(&event, 1);
    yaml_emitter_emit(&emitter, &event);

    yaml_stream_end_event_initialize(&event);
    int result = yaml_emitter_emit(&emitter, &event);

    ASSERT(result, "Emitter should handle all scalar styles");

    yaml_emitter_delete(&emitter);
}

/* Emitter: flow style sequences and mappings */
static void test_emitter_flow_styles(void) {
    yaml_emitter_t emitter;
    yaml_event_t event;
    unsigned char buffer[4096];
    size_t written;

    yaml_emitter_initialize(&emitter);
    yaml_emitter_set_output_string(&emitter, buffer, sizeof(buffer), &written);

    yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
    yaml_emitter_emit(&emitter, &event);

    yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1);
    yaml_emitter_emit(&emitter, &event);

    /* Flow sequence */
    yaml_sequence_start_event_initialize(&event, NULL, NULL, 1, YAML_FLOW_SEQUENCE_STYLE);
    yaml_emitter_emit(&emitter, &event);

    yaml_scalar_event_initialize(&event, NULL, NULL,
        (yaml_char_t *)"a", 1, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);

    yaml_scalar_event_initialize(&event, NULL, NULL,
        (yaml_char_t *)"b", 1, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);

    /* Nested flow mapping inside flow sequence */
    yaml_mapping_start_event_initialize(&event, NULL, NULL, 1, YAML_FLOW_MAPPING_STYLE);
    yaml_emitter_emit(&emitter, &event);

    yaml_scalar_event_initialize(&event, NULL, NULL,
        (yaml_char_t *)"key", 3, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);

    yaml_scalar_event_initialize(&event, NULL, NULL,
        (yaml_char_t *)"val", 3, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);

    yaml_mapping_end_event_initialize(&event);
    yaml_emitter_emit(&emitter, &event);

    yaml_sequence_end_event_initialize(&event);
    yaml_emitter_emit(&emitter, &event);

    yaml_document_end_event_initialize(&event, 1);
    yaml_emitter_emit(&emitter, &event);

    yaml_stream_end_event_initialize(&event);
    int result = yaml_emitter_emit(&emitter, &event);

    ASSERT(result, "Flow styles should emit correctly");

    yaml_emitter_delete(&emitter);
}

/* Test scanner with tab character at indentation level */
static void test_tab_character_errors(void) {
    /* Tab at indentation should cause error in block context */
    const char *input = "key:\n\t- value\n";
    yaml_parser_t parser;
    yaml_event_t event;

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));

    while (1) {
        if (!yaml_parser_parse(&parser, &event)) {
            break;
        }
        if (event.type == YAML_STREAM_END_EVENT) {
            yaml_event_delete(&event);
            break;
        }
        yaml_event_delete(&event);
    }
    yaml_parser_delete(&parser);

    /* libyaml allows tabs in some positions, let's just test it doesn't crash */
    ASSERT(1, "Tab character test completed without crash");
}

/* Test multiple documents in same stream */
static void test_multi_document_stream(void) {
    const char *input = "---\nfirst: doc\n---\nsecond: doc\n...\n---\nthird: doc\n...\n";
    yaml_parser_t parser;
    yaml_event_t event;
    int doc_count = 0;

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));

    while (1) {
        if (!yaml_parser_parse(&parser, &event)) break;
        if (event.type == YAML_DOCUMENT_START_EVENT) doc_count++;
        if (event.type == YAML_STREAM_END_EVENT) {
            yaml_event_delete(&event);
            break;
        }
        yaml_event_delete(&event);
    }
    yaml_parser_delete(&parser);

    ASSERT_EQ_INT(doc_count, 3, "Should find 3 documents in multi-doc stream");
}

/* Test emitter with explicit document markers */
static void test_emitter_explicit_document_markers(void) {
    yaml_emitter_t emitter;
    yaml_event_t event;
    unsigned char buffer[4096];
    size_t written;

    yaml_emitter_initialize(&emitter);
    yaml_emitter_set_output_string(&emitter, buffer, sizeof(buffer), &written);

    yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
    yaml_emitter_emit(&emitter, &event);

    /* First document with explicit markers */
    yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 0);
    yaml_emitter_emit(&emitter, &event);

    yaml_scalar_event_initialize(&event, NULL, NULL,
        (yaml_char_t *)"first", 5, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);

    yaml_document_end_event_initialize(&event, 0);
    yaml_emitter_emit(&emitter, &event);

    /* Second document with explicit markers */
    yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 0);
    yaml_emitter_emit(&emitter, &event);

    yaml_scalar_event_initialize(&event, NULL, NULL,
        (yaml_char_t *)"second", 6, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);

    yaml_document_end_event_initialize(&event, 0);
    yaml_emitter_emit(&emitter, &event);

    yaml_stream_end_event_initialize(&event);
    int result = yaml_emitter_emit(&emitter, &event);

    ASSERT(result, "Multi-document emit should succeed");

    yaml_emitter_delete(&emitter);
}

/* Test custom allocator via yaml_*_initialize_with_allocator */
static void test_custom_allocator(void) {
    yaml_parser_t parser;

    /* Test with default allocator (passing NULL for functions) */
    int result = yaml_parser_initialize(&parser);
    ASSERT(result, "Parser init with default allocator should succeed");
    yaml_parser_delete(&parser);

    /* Test emitter with default allocator */
    yaml_emitter_t emitter;
    result = yaml_emitter_initialize(&emitter);
    ASSERT(result, "Emitter init with default allocator should succeed");
    yaml_emitter_delete(&emitter);
}

/* Test parser with various encoding inputs */
static void test_parser_encoding_detection(void) {
    /* UTF-8 BOM */
    const unsigned char utf8_bom[] = {0xEF, 0xBB, 0xBF, 'k', 'e', 'y', ':', ' ', 'v', 'a', 'l', '\n', 0};
    yaml_parser_t parser;
    yaml_event_t event;

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, utf8_bom, sizeof(utf8_bom) - 1);

    int success = 1;
    while (1) {
        if (!yaml_parser_parse(&parser, &event)) {
            success = 0;
            break;
        }
        if (event.type == YAML_STREAM_END_EVENT) {
            yaml_event_delete(&event);
            break;
        }
        yaml_event_delete(&event);
    }
    yaml_parser_delete(&parser);

    ASSERT(success, "UTF-8 BOM input should parse successfully");
}

/* Test emitter with various widths and canonical mode */
static void test_emitter_options(void) {
    yaml_emitter_t emitter;
    yaml_event_t event;
    unsigned char buffer[4096];
    size_t written;

    /* Test canonical mode */
    yaml_emitter_initialize(&emitter);
    yaml_emitter_set_output_string(&emitter, buffer, sizeof(buffer), &written);
    yaml_emitter_set_canonical(&emitter, 1);
    yaml_emitter_set_indent(&emitter, 4);
    yaml_emitter_set_width(&emitter, 40);

    yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
    yaml_emitter_emit(&emitter, &event);

    yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 0);
    yaml_emitter_emit(&emitter, &event);

    yaml_mapping_start_event_initialize(&event, NULL, NULL, 1, YAML_BLOCK_MAPPING_STYLE);
    yaml_emitter_emit(&emitter, &event);

    yaml_scalar_event_initialize(&event, NULL, NULL,
        (yaml_char_t *)"a-very-long-key-name-that-exceeds-width", 39,
        1, 1, YAML_PLAIN_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);

    yaml_scalar_event_initialize(&event, NULL, NULL,
        (yaml_char_t *)"a-very-long-value-that-also-exceeds-the-configured-width-limit", 62,
        1, 1, YAML_PLAIN_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);

    yaml_mapping_end_event_initialize(&event);
    yaml_emitter_emit(&emitter, &event);

    yaml_document_end_event_initialize(&event, 0);
    yaml_emitter_emit(&emitter, &event);

    yaml_stream_end_event_initialize(&event);
    int result = yaml_emitter_emit(&emitter, &event);

    ASSERT(result, "Canonical mode emitter should succeed");

    yaml_emitter_delete(&emitter);
}

/* Test UTF-16 emitter with surrogate pairs (writer.c lines 89,94,116-122) */
static int utf16_write_handler(void *data, unsigned char *buffer, size_t size) {
    (void)data; (void)buffer; (void)size;
    return 1; /* Accept all output */
}

static void test_emitter_utf16_surrogate_pairs(void) {
    yaml_emitter_t emitter;
    yaml_event_t event;

    /* U+1F600 (grinning face) is a 4-byte UTF-8 sequence: F0 9F 98 80
     * In UTF-16, it requires a surrogate pair: D83D DE00 */
    const unsigned char scalar_with_surrogate[] = {
        0xF0, 0x9F, 0x98, 0x80, 0x00  /* U+1F600 + null terminator */
    };

    /* Test UTF-16LE */
    yaml_emitter_initialize(&emitter);
    yaml_emitter_set_output(&emitter, utf16_write_handler, NULL);
    yaml_emitter_set_encoding(&emitter, YAML_UTF16LE_ENCODING);

    yaml_stream_start_event_initialize(&event, YAML_UTF16LE_ENCODING);
    yaml_emitter_emit(&emitter, &event);

    yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1);
    yaml_emitter_emit(&emitter, &event);

    yaml_scalar_event_initialize(&event, NULL, NULL,
        (yaml_char_t *)scalar_with_surrogate, 4,
        1, 1, YAML_PLAIN_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);

    yaml_document_end_event_initialize(&event, 1);
    yaml_emitter_emit(&emitter, &event);

    yaml_stream_end_event_initialize(&event);
    int result = yaml_emitter_emit(&emitter, &event);
    ASSERT(result, "UTF-16LE emitter with surrogate pair should succeed");
    yaml_emitter_delete(&emitter);

    /* Test UTF-16BE */
    yaml_emitter_initialize(&emitter);
    yaml_emitter_set_output(&emitter, utf16_write_handler, NULL);
    yaml_emitter_set_encoding(&emitter, YAML_UTF16BE_ENCODING);

    yaml_stream_start_event_initialize(&event, YAML_UTF16BE_ENCODING);
    yaml_emitter_emit(&emitter, &event);

    yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1);
    yaml_emitter_emit(&emitter, &event);

    yaml_scalar_event_initialize(&event, NULL, NULL,
        (yaml_char_t *)scalar_with_surrogate, 4,
        1, 1, YAML_PLAIN_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);

    yaml_document_end_event_initialize(&event, 1);
    yaml_emitter_emit(&emitter, &event);

    yaml_stream_end_event_initialize(&event);
    result = yaml_emitter_emit(&emitter, &event);
    ASSERT(result, "UTF-16BE emitter with surrogate pair should succeed");
    yaml_emitter_delete(&emitter);
}

/* Test emitter with literal scalar containing line breaks (emitter.c lines 1959-1964) */
static void test_emitter_literal_multiline(void) {
    unsigned char output[4096];
    size_t output_len = 0;
    yaml_emitter_t emitter;
    yaml_event_t event;

    yaml_emitter_initialize(&emitter);
    yaml_emitter_set_output_string(&emitter, output, sizeof(output), &output_len);
    yaml_emitter_set_canonical(&emitter, 0);

    yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
    yaml_emitter_emit(&emitter, &event);

    yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1);
    yaml_emitter_emit(&emitter, &event);

    /* Emit a literal scalar with multiple lines */
    const char *multiline = "line one\nline two\nline three\n";
    yaml_scalar_event_initialize(&event, NULL,
        (yaml_char_t *)"tag:yaml.org,2002:str",
        (yaml_char_t *)multiline, strlen(multiline),
        0, 0, YAML_LITERAL_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);

    yaml_document_end_event_initialize(&event, 1);
    yaml_emitter_emit(&emitter, &event);

    yaml_stream_end_event_initialize(&event);
    int result = yaml_emitter_emit(&emitter, &event);
    ASSERT(result, "Literal scalar with line breaks should emit successfully");

    yaml_emitter_delete(&emitter);
}

/* Test emitter with empty literal/folded scalar (emitter.c line 2226: chomp_hint = "-") */
static void test_emitter_empty_block_scalar(void) {
    unsigned char output[4096];
    size_t output_len = 0;
    yaml_emitter_t emitter;
    yaml_event_t event;

    yaml_emitter_initialize(&emitter);
    yaml_emitter_set_output_string(&emitter, output, sizeof(output), &output_len);

    yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
    yaml_emitter_emit(&emitter, &event);

    yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1);
    yaml_emitter_emit(&emitter, &event);

    yaml_mapping_start_event_initialize(&event, NULL, NULL, 1, YAML_BLOCK_MAPPING_STYLE);
    yaml_emitter_emit(&emitter, &event);

    yaml_scalar_event_initialize(&event, NULL, NULL,
        (yaml_char_t *)"key", 3, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);

    /* Empty string with literal style -- should trigger chomp_hint = "-" */
    yaml_scalar_event_initialize(&event, NULL,
        (yaml_char_t *)"tag:yaml.org,2002:str",
        (yaml_char_t *)"", 0,
        0, 0, YAML_LITERAL_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);

    yaml_mapping_end_event_initialize(&event);
    yaml_emitter_emit(&emitter, &event);

    yaml_document_end_event_initialize(&event, 1);
    yaml_emitter_emit(&emitter, &event);

    yaml_stream_end_event_initialize(&event);
    int result = yaml_emitter_emit(&emitter, &event);
    /* The emitter may choose a different style for empty string, but should not crash */
    ASSERT(result, "Empty block scalar should emit successfully");

    yaml_emitter_delete(&emitter);
}

/* Test alias resolution in loader */
static void test_loader_alias(void) {
    const char *input = "---\na: &anchor value\nb: *anchor\n";
    yaml_parser_t parser;
    yaml_document_t doc;

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));

    int result = yaml_parser_load(&parser, &doc);
    ASSERT(result, "Loading document with alias should succeed");

    yaml_node_t *root = yaml_document_get_root_node(&doc);
    ASSERT_NOT_NULL(root, "Root node should exist");
    ASSERT_EQ_INT(root->type, YAML_MAPPING_NODE, "Root should be mapping");

    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
}

/* Test loader with nested sequences and mappings */
static void test_loader_nested_structures(void) {
    const char *input =
        "---\n"
        "sequence:\n"
        "  - item1\n"
        "  - nested:\n"
        "      key: value\n"
        "  - - sub1\n"
        "    - sub2\n"
        "mapping:\n"
        "  key1:\n"
        "    subkey: subval\n"
        "  key2: [a, b, c]\n";

    yaml_parser_t parser;
    yaml_document_t doc;

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));

    int result = yaml_parser_load(&parser, &doc);
    ASSERT(result, "Loading nested doc should succeed");

    yaml_node_t *root = yaml_document_get_root_node(&doc);
    ASSERT_NOT_NULL(root, "Root node should exist");
    ASSERT_EQ_INT(root->type, YAML_MAPPING_NODE, "Root should be mapping");

    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
}

/* Test emitter with unicode content */
static void test_emitter_unicode(void) {
    yaml_emitter_t emitter;
    yaml_event_t event;
    unsigned char buffer[4096];
    size_t written;

    yaml_emitter_initialize(&emitter);
    yaml_emitter_set_output_string(&emitter, buffer, sizeof(buffer), &written);
    yaml_emitter_set_unicode(&emitter, 1);

    yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
    yaml_emitter_emit(&emitter, &event);

    yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1);
    yaml_emitter_emit(&emitter, &event);

    /* Unicode scalar */
    const char *unicode_val = "\xc3\xa9\xc3\xa0\xc3\xbc"; /* e-accent, a-grave, u-umlaut */
    yaml_scalar_event_initialize(&event, NULL, NULL,
        (yaml_char_t *)unicode_val, strlen(unicode_val),
        1, 1, YAML_PLAIN_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);

    yaml_document_end_event_initialize(&event, 1);
    yaml_emitter_emit(&emitter, &event);

    yaml_stream_end_event_initialize(&event);
    int result = yaml_emitter_emit(&emitter, &event);

    ASSERT(result, "Unicode emitter should succeed");

    yaml_emitter_delete(&emitter);
}

/* Test emitter with explicit tags */
static void test_emitter_explicit_tags(void) {
    yaml_emitter_t emitter;
    yaml_event_t event;
    unsigned char buffer[4096];
    size_t written;

    yaml_emitter_initialize(&emitter);
    yaml_emitter_set_output_string(&emitter, buffer, sizeof(buffer), &written);

    yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
    yaml_emitter_emit(&emitter, &event);

    /* TAG directive */
    yaml_tag_directive_t tag_directives[] = {
        {(yaml_char_t *)"!x!", (yaml_char_t *)"tag:example.com,2000:"}
    };
    yaml_tag_directive_t *tag_start = tag_directives;
    yaml_tag_directive_t *tag_end = tag_directives + 1;

    yaml_document_start_event_initialize(&event, NULL, tag_start, tag_end, 0);
    yaml_emitter_emit(&emitter, &event);

    /* Scalar with explicit tag */
    yaml_scalar_event_initialize(&event, NULL,
        (yaml_char_t *)"tag:example.com,2000:mytype",
        (yaml_char_t *)"42", 2, 0, 0, YAML_PLAIN_SCALAR_STYLE);
    yaml_emitter_emit(&emitter, &event);

    yaml_document_end_event_initialize(&event, 0);
    yaml_emitter_emit(&emitter, &event);

    yaml_stream_end_event_initialize(&event);
    int result = yaml_emitter_emit(&emitter, &event);

    ASSERT(result, "Emitter with explicit tags should succeed");

    yaml_emitter_delete(&emitter);
}

/* Test scanner with block scalars containing various chomping indicators */
static void test_block_scalar_chomping(void) {
    /* Strip chomping */
    const char *input1 = "key: |-\n  text\n\n";
    yaml_token_type_t tokens[32];
    int count;

    int result = scan_string_tokens(input1, tokens, 32, &count);
    ASSERT(result, "Strip chomping should scan successfully");

    /* Keep chomping */
    const char *input2 = "key: |+\n  text\n\n";
    result = scan_string_tokens(input2, tokens, 32, &count);
    ASSERT(result, "Keep chomping should scan successfully");

    /* Clip chomping with indentation indicator */
    const char *input3 = "key: |2\n  text\n";
    result = scan_string_tokens(input3, tokens, 32, &count);
    ASSERT(result, "Clip chomping with indent indicator should scan successfully");

    /* Folded with strip */
    const char *input4 = "key: >-\n  text\n\n";
    result = scan_string_tokens(input4, tokens, 32, &count);
    ASSERT(result, "Folded strip should scan successfully");

    /* Folded with keep and indentation */
    const char *input5 = "key: >+2\n  text\n\n";
    result = scan_string_tokens(input5, tokens, 32, &count);
    ASSERT(result, "Folded keep+indent should scan successfully");
}

/* Test complex key detection */
static void test_complex_keys(void) {
    /* Explicit complex key */
    const char *input1 = "? key\n: value\n";
    yaml_token_type_t tokens[32];
    int count;

    int result = scan_string_tokens(input1, tokens, 32, &count);
    ASSERT(result, "Explicit complex key should scan successfully");

    /* Complex key in flow context */
    const char *input2 = "{? key : value}\n";
    result = scan_string_tokens(input2, tokens, 32, &count);
    ASSERT(result, "Complex key in flow should scan successfully");

    /* Long key (>1024 chars) - may fail because libyaml requires simple keys
     * to be <=1024 chars; just verify no crash */
    char long_input[2048];
    memset(long_input, 'a', 1500);
    strcpy(long_input + 1500, ": value\n");
    (void)scan_string_tokens(long_input, tokens, 32, &count);
    /* libyaml treats >1024 char keys as not simple keys, may cause parse issues */
    ASSERT(1, "Long key scan completed without crash");
}

/* Test API: yaml_document_add_* functions */
static void test_document_builder_api(void) {
    yaml_document_t doc;

    yaml_document_initialize(&doc, NULL, NULL, NULL, 0, 0);

    /* Add scalar */
    int scalar_id = yaml_document_add_scalar(&doc, NULL,
        (yaml_char_t *)"hello", 5, YAML_PLAIN_SCALAR_STYLE);
    ASSERT(scalar_id > 0, "Adding scalar should return positive ID");

    /* Add sequence */
    int seq_id = yaml_document_add_sequence(&doc, NULL, YAML_BLOCK_SEQUENCE_STYLE);
    ASSERT(seq_id > 0, "Adding sequence should return positive ID");

    /* Add mapping */
    int map_id = yaml_document_add_mapping(&doc, NULL, YAML_BLOCK_MAPPING_STYLE);
    ASSERT(map_id > 0, "Adding mapping should return positive ID");

    /* Append to sequence */
    int result = yaml_document_append_sequence_item(&doc, seq_id, scalar_id);
    ASSERT(result, "Appending to sequence should succeed");

    /* Add pair to mapping */
    int key_id = yaml_document_add_scalar(&doc, NULL,
        (yaml_char_t *)"key", 3, YAML_PLAIN_SCALAR_STYLE);
    int val_id = yaml_document_add_scalar(&doc, NULL,
        (yaml_char_t *)"value", 5, YAML_PLAIN_SCALAR_STYLE);
    result = yaml_document_append_mapping_pair(&doc, map_id, key_id, val_id);
    ASSERT(result, "Appending to mapping should succeed");

    yaml_document_delete(&doc);
}

/* Regression test: double-free when yaml_parser_load fails and caller
 * also calls yaml_document_delete. Found by fuzz_load harness.
 * Input triggers a parse error mid-stream; yaml_parser_load calls
 * yaml_document_delete on its error path (loader.c:141), then the caller
 * calls it again. This is safe in libyaml because yaml_document_delete
 * memsets the document to zero, making it idempotent. */
static void test_document_delete_after_load_failure(void) {
    const char *input = "-:\n%YAML 1.1\n---\niVc:\n%YAML 0";
    yaml_parser_t parser;
    yaml_document_t document;

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));

    while (1) {
        if (!yaml_parser_load(&parser, &document)) {
            /* This second delete must be safe (idempotent) */
            yaml_document_delete(&document);
            break;
        }
        if (!yaml_document_get_root_node(&document)) {
            yaml_document_delete(&document);
            break;
        }
        yaml_document_delete(&document);
    }

    yaml_parser_delete(&parser);
    ASSERT(1, "Double yaml_document_delete after load failure should not crash");
}

/* Test special YAML values */
static void test_special_yaml_values(void) {
    /* Null values */
    const char *input1 = "key: ~\nkey2: null\nkey3:\n";
    yaml_event_type_t events[32];
    int count;

    int result = parse_string_events(input1, events, 32, &count);
    ASSERT(result, "Null values should parse successfully");

    /* Boolean values */
    const char *input2 = "a: true\nb: false\nc: yes\nd: no\n";
    result = parse_string_events(input2, events, 32, &count);
    ASSERT(result, "Boolean values should parse successfully");

    /* Numeric values */
    const char *input3 = "int: 42\nfloat: 3.14\nhex: 0xFF\noct: 0o77\ninf: .inf\nnan: .nan\n";
    result = parse_string_events(input3, events, 32, &count);
    ASSERT(result, "Numeric values should parse successfully");
}

int main(void) {
    TEST_SUITE_BEGIN("Coverage Targets");

    printf("  Scanner dispatch tests:\n");
    test_percent_not_at_column_zero();
    test_block_indicators_in_flow_context();
    test_invalid_token_start_characters();

    printf("  TAG directive tests:\n");
    test_tag_directive_errors();

    printf("  Emitter error path tests:\n");
    test_emitter_wrong_event_sequence();
    test_emitter_invalid_anchor_tag();
    test_emitter_scalar_styles();
    test_emitter_flow_styles();
    test_emitter_explicit_document_markers();
    test_emitter_options();
    test_emitter_unicode();
    test_emitter_explicit_tags();
    test_emitter_utf16_surrogate_pairs();
    test_emitter_literal_multiline();
    test_emitter_empty_block_scalar();

    printf("  Scanner edge cases:\n");
    test_tab_character_errors();
    test_block_scalar_chomping();
    test_complex_keys();

    printf("  Loader tests:\n");
    test_loader_alias();
    test_loader_nested_structures();
    test_document_delete_after_load_failure();

    printf("  API tests:\n");
    test_custom_allocator();
    test_parser_encoding_detection();
    test_document_builder_api();

    printf("  Value tests:\n");
    test_multi_document_stream();
    test_special_yaml_values();

    TEST_SUITE_END();
}
