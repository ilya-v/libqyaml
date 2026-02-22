#include "test_helper.h"

/*
 * Helper: emit a simple YAML document to a string buffer with the given encoding.
 * Returns 1 on success, 0 on failure. Sets *written to bytes written.
 */
static int emit_simple_doc(unsigned char *output, size_t output_size,
                           size_t *written, yaml_encoding_t encoding)
{
    yaml_emitter_t emitter;
    yaml_event_t event;

    if (!yaml_emitter_initialize(&emitter)) return 0;
    yaml_emitter_set_output_string(&emitter, output, output_size, written);
    yaml_emitter_set_encoding(&emitter, encoding);

    yaml_stream_start_event_initialize(&event, encoding);
    if (!yaml_emitter_emit(&emitter, &event)) goto error;

    yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1);
    if (!yaml_emitter_emit(&emitter, &event)) goto error;

    yaml_scalar_event_initialize(&event, NULL, (const yaml_char_t *)YAML_STR_TAG,
        (const yaml_char_t *)"hello", 5, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    if (!yaml_emitter_emit(&emitter, &event)) goto error;

    yaml_document_end_event_initialize(&event, 1);
    if (!yaml_emitter_emit(&emitter, &event)) goto error;

    yaml_stream_end_event_initialize(&event);
    if (!yaml_emitter_emit(&emitter, &event)) goto error;

    yaml_emitter_delete(&emitter);
    return 1;

error:
    yaml_emitter_delete(&emitter);
    return 0;
}

/* Test UTF-8 output (baseline) */
static void test_writer_utf8(void) {
    unsigned char output[4096];
    size_t written = 0;

    ASSERT(emit_simple_doc(output, sizeof(output), &written, YAML_UTF8_ENCODING),
           "UTF-8 emit should succeed");
    ASSERT(written > 0, "UTF-8 output should not be empty");

    output[written] = '\0';
    ASSERT(strstr((char *)output, "hello") != NULL,
           "UTF-8 output should contain 'hello'");
}

/* Test UTF-16 LE output */
static void test_writer_utf16le(void) {
    unsigned char output[4096];
    size_t written = 0;

    ASSERT(emit_simple_doc(output, sizeof(output), &written, YAML_UTF16LE_ENCODING),
           "UTF-16LE emit should succeed");
    ASSERT(written > 0, "UTF-16LE output should not be empty");

    /* UTF-16 LE BOM: 0xFF 0xFE */
    ASSERT(output[0] == 0xFF && output[1] == 0xFE,
           "UTF-16LE output should start with LE BOM");

    /* 'h' in UTF-16 LE is 0x68 0x00 */
    {
        int found = 0;
        size_t i;
        for (i = 2; i + 1 < written; i += 2) {
            if (output[i] == 'h' && output[i+1] == 0x00) {
                found = 1;
                break;
            }
        }
        ASSERT(found, "UTF-16LE output should contain 'h' encoded as 0x68 0x00");
    }
}

/* Test UTF-16 BE output */
static void test_writer_utf16be(void) {
    unsigned char output[4096];
    size_t written = 0;

    ASSERT(emit_simple_doc(output, sizeof(output), &written, YAML_UTF16BE_ENCODING),
           "UTF-16BE emit should succeed");
    ASSERT(written > 0, "UTF-16BE output should not be empty");

    /* UTF-16 BE BOM: 0xFE 0xFF */
    ASSERT(output[0] == 0xFE && output[1] == 0xFF,
           "UTF-16BE output should start with BE BOM");

    /* 'h' in UTF-16 BE is 0x00 0x68 */
    {
        int found = 0;
        size_t i;
        for (i = 2; i + 1 < written; i += 2) {
            if (output[i] == 0x00 && output[i+1] == 'h') {
                found = 1;
                break;
            }
        }
        ASSERT(found, "UTF-16BE output should contain 'h' encoded as 0x00 0x68");
    }
}

/* Test UTF-16 output with multi-byte UTF-8 input (BMP characters).
 * Uses plain style with unicode=1 so the emitter passes through non-ASCII
 * BMP characters to the writer, which converts them to UTF-16. */
static void test_writer_utf16_multibyte(void) {
    yaml_emitter_t emitter;
    yaml_event_t event;
    unsigned char output[8192];
    size_t written = 0;

    /* U+00E9 (e-acute) = UTF-8: 0xC3 0xA9, UTF-16 LE: 0xE9 0x00 */
    const yaml_char_t *text = (const yaml_char_t *)"caf\xC3\xA9";

    ASSERT(yaml_emitter_initialize(&emitter), "utf16 multibyte init");
    yaml_emitter_set_output_string(&emitter, output, sizeof(output), &written);
    yaml_emitter_set_encoding(&emitter, YAML_UTF16LE_ENCODING);
    yaml_emitter_set_unicode(&emitter, 1);

    yaml_stream_start_event_initialize(&event, YAML_UTF16LE_ENCODING);
    ASSERT(yaml_emitter_emit(&emitter, &event), "emit stream start");

    yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1);
    ASSERT(yaml_emitter_emit(&emitter, &event), "emit doc start");

    yaml_scalar_event_initialize(&event, NULL, (const yaml_char_t *)YAML_STR_TAG,
        text, 5, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    ASSERT(yaml_emitter_emit(&emitter, &event), "emit scalar with e-acute");

    yaml_document_end_event_initialize(&event, 1);
    ASSERT(yaml_emitter_emit(&emitter, &event), "emit doc end");

    yaml_stream_end_event_initialize(&event);
    ASSERT(yaml_emitter_emit(&emitter, &event), "emit stream end");

    yaml_emitter_delete(&emitter);

    ASSERT(written > 0, "UTF-16LE multibyte output should not be empty");

    /* Find U+00E9 in the UTF-16 LE output: 0xE9 0x00 */
    {
        int found = 0;
        size_t i;
        for (i = 2; i + 1 < written; i += 2) {
            if (output[i] == 0xE9 && output[i+1] == 0x00) {
                found = 1;
                break;
            }
        }
        ASSERT(found, "UTF-16LE output should contain U+00E9 as 0xE9 0x00");
    }
}

/* Test UTF-16 BE with multi-byte UTF-8 (3-byte sequence, U+2603 snowman) */
static void test_writer_utf16be_multibyte(void) {
    yaml_emitter_t emitter;
    yaml_event_t event;
    unsigned char output[8192];
    size_t written = 0;

    /* U+2603 (snowman) = UTF-8: E2 98 83, UTF-16 BE: 26 03 */
    const yaml_char_t *text = (const yaml_char_t *)"\xE2\x98\x83";

    ASSERT(yaml_emitter_initialize(&emitter), "utf16be multibyte init");
    yaml_emitter_set_output_string(&emitter, output, sizeof(output), &written);
    yaml_emitter_set_encoding(&emitter, YAML_UTF16BE_ENCODING);
    yaml_emitter_set_unicode(&emitter, 1);

    yaml_stream_start_event_initialize(&event, YAML_UTF16BE_ENCODING);
    ASSERT(yaml_emitter_emit(&emitter, &event), "emit stream start");

    yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1);
    ASSERT(yaml_emitter_emit(&emitter, &event), "emit doc start");

    yaml_scalar_event_initialize(&event, NULL, (const yaml_char_t *)YAML_STR_TAG,
        text, 3, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    ASSERT(yaml_emitter_emit(&emitter, &event), "emit snowman");

    yaml_document_end_event_initialize(&event, 1);
    ASSERT(yaml_emitter_emit(&emitter, &event), "emit doc end");

    yaml_stream_end_event_initialize(&event);
    ASSERT(yaml_emitter_emit(&emitter, &event), "emit stream end");

    yaml_emitter_delete(&emitter);

    ASSERT(written > 0, "UTF-16BE multibyte output should not be empty");

    /* Find U+2603 in UTF-16 BE: 0x26 0x03 */
    {
        int found = 0;
        size_t i;
        for (i = 2; i + 1 < written; i += 2) {
            if (output[i] == 0x26 && output[i+1] == 0x03) {
                found = 1;
                break;
            }
        }
        ASSERT(found, "UTF-16BE output should contain U+2603 as 0x26 0x03");
    }
}

/* Custom write handler that always fails */
static int failing_write_handler(void *data, unsigned char *buffer, size_t size) {
    (void)data;
    (void)buffer;
    (void)size;
    return 0;
}

/* Test write error handling -- the write handler is called on flush, which
 * happens when the emitter's internal buffer fills up or at stream end.
 * We use a tiny buffer-size-limited approach: emit enough data to trigger
 * a flush, then verify the error. */
static void test_writer_error_utf8(void) {
    yaml_emitter_t emitter;
    yaml_event_t event;
    int failed = 0;

    ASSERT(yaml_emitter_initialize(&emitter), "writer error init");
    yaml_emitter_set_output(&emitter, failing_write_handler, NULL);
    yaml_emitter_set_encoding(&emitter, YAML_UTF8_ENCODING);

    yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
    if (!yaml_emitter_emit(&emitter, &event)) { failed = 1; goto done; }

    yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 0);
    if (!yaml_emitter_emit(&emitter, &event)) { failed = 1; goto done; }

    /* Emit a large scalar to force a buffer flush */
    {
        char big[16384];
        memset(big, 'x', sizeof(big));
        yaml_scalar_event_initialize(&event, NULL, (const yaml_char_t *)YAML_STR_TAG,
            (const yaml_char_t *)big, sizeof(big), 1, 1, YAML_PLAIN_SCALAR_STYLE);
        if (!yaml_emitter_emit(&emitter, &event)) { failed = 1; goto done; }
    }

done:
    ASSERT(failed, "emit with failing writer should eventually fail");
    ASSERT_EQ_INT(emitter.error, YAML_WRITER_ERROR,
                  "error type should be YAML_WRITER_ERROR");

    yaml_emitter_delete(&emitter);
}

/* Test write error with UTF-16 encoding */
static void test_writer_error_utf16(void) {
    yaml_emitter_t emitter;
    yaml_event_t event;
    int failed = 0;

    ASSERT(yaml_emitter_initialize(&emitter), "writer error utf16 init");
    yaml_emitter_set_output(&emitter, failing_write_handler, NULL);
    yaml_emitter_set_encoding(&emitter, YAML_UTF16LE_ENCODING);

    yaml_stream_start_event_initialize(&event, YAML_UTF16LE_ENCODING);
    if (!yaml_emitter_emit(&emitter, &event)) { failed = 1; goto done; }

    yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 0);
    if (!yaml_emitter_emit(&emitter, &event)) { failed = 1; goto done; }

    {
        char big[16384];
        memset(big, 'y', sizeof(big));
        yaml_scalar_event_initialize(&event, NULL, (const yaml_char_t *)YAML_STR_TAG,
            (const yaml_char_t *)big, sizeof(big), 1, 1, YAML_PLAIN_SCALAR_STYLE);
        if (!yaml_emitter_emit(&emitter, &event)) { failed = 1; goto done; }
    }

done:
    ASSERT(failed, "emit with failing writer (utf16) should eventually fail");
    ASSERT_EQ_INT(emitter.error, YAML_WRITER_ERROR,
                  "error type should be YAML_WRITER_ERROR (utf16)");

    yaml_emitter_delete(&emitter);
}

/* Test that UTF-16 output can be round-tripped through the parser */
static void test_writer_utf16_roundtrip(void) {
    unsigned char output[8192];
    size_t written = 0;
    yaml_parser_t parser;
    yaml_event_t event;
    int found_scalar = 0;

    ASSERT(emit_simple_doc(output, sizeof(output), &written, YAML_UTF16LE_ENCODING),
           "UTF-16LE emit for roundtrip");

    /* Parse the UTF-16 LE output back */
    ASSERT(yaml_parser_initialize(&parser), "parser init for roundtrip");
    yaml_parser_set_input_string(&parser, output, written);

    while (1) {
        ASSERT(yaml_parser_parse(&parser, &event), "parse utf16 output");
        if (event.type == YAML_SCALAR_EVENT) {
            ASSERT_EQ_STR((const char *)event.data.scalar.value, "hello",
                          "roundtrip scalar value should be 'hello'");
            found_scalar = 1;
        }
        if (event.type == YAML_STREAM_END_EVENT) {
            yaml_event_delete(&event);
            break;
        }
        yaml_event_delete(&event);
    }
    yaml_parser_delete(&parser);

    ASSERT(found_scalar, "should have found scalar in roundtrip");
}

/* Test UTF-16 BE roundtrip */
static void test_writer_utf16be_roundtrip(void) {
    unsigned char output[8192];
    size_t written = 0;
    yaml_parser_t parser;
    yaml_event_t event;
    int found_scalar = 0;

    ASSERT(emit_simple_doc(output, sizeof(output), &written, YAML_UTF16BE_ENCODING),
           "UTF-16BE emit for roundtrip");

    ASSERT(yaml_parser_initialize(&parser), "parser init for BE roundtrip");
    yaml_parser_set_input_string(&parser, output, written);

    while (1) {
        ASSERT(yaml_parser_parse(&parser, &event), "parse utf16be output");
        if (event.type == YAML_SCALAR_EVENT) {
            ASSERT_EQ_STR((const char *)event.data.scalar.value, "hello",
                          "BE roundtrip scalar value should be 'hello'");
            found_scalar = 1;
        }
        if (event.type == YAML_STREAM_END_EVENT) {
            yaml_event_delete(&event);
            break;
        }
        yaml_event_delete(&event);
    }
    yaml_parser_delete(&parser);

    ASSERT(found_scalar, "should have found scalar in BE roundtrip");
}

/* ==================================================================
 * Additional writer tests for coverage
 * ================================================================== */

/* Test UTF-16 LE with 4-byte UTF-8 (U+1F600) - emitter escapes as \U, roundtrip works */
static void test_writer_utf16le_4byte(void) {
    yaml_emitter_t emitter;
    yaml_event_t event;
    unsigned char output[8192];
    size_t written = 0;

    /* U+1F600 = UTF-8: F0 9F 98 80 */
    const yaml_char_t *text = (const yaml_char_t *)"\xF0\x9F\x98\x80";

    ASSERT(yaml_emitter_initialize(&emitter), "utf16le 4byte init");
    yaml_emitter_set_output_string(&emitter, output, sizeof(output), &written);
    yaml_emitter_set_encoding(&emitter, YAML_UTF16LE_ENCODING);
    yaml_emitter_set_unicode(&emitter, 1);

    yaml_stream_start_event_initialize(&event, YAML_UTF16LE_ENCODING);
    ASSERT(yaml_emitter_emit(&emitter, &event), "emit stream start");
    yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1);
    ASSERT(yaml_emitter_emit(&emitter, &event), "emit doc start");
    yaml_scalar_event_initialize(&event, NULL, (const yaml_char_t *)YAML_STR_TAG,
        text, 4, 1, 1, YAML_DOUBLE_QUOTED_SCALAR_STYLE);
    ASSERT(yaml_emitter_emit(&emitter, &event), "emit 4byte scalar");
    yaml_document_end_event_initialize(&event, 1);
    ASSERT(yaml_emitter_emit(&emitter, &event), "emit doc end");
    yaml_stream_end_event_initialize(&event);
    ASSERT(yaml_emitter_emit(&emitter, &event), "emit stream end");

    yaml_emitter_delete(&emitter);
    ASSERT(written > 0, "UTF-16LE 4byte output should not be empty");
}

/* Test UTF-16 BE with 4-byte UTF-8 */
static void test_writer_utf16be_4byte(void) {
    yaml_emitter_t emitter;
    yaml_event_t event;
    unsigned char output[8192];
    size_t written = 0;

    const yaml_char_t *text = (const yaml_char_t *)"\xF0\x9F\x98\x80";

    ASSERT(yaml_emitter_initialize(&emitter), "utf16be 4byte init");
    yaml_emitter_set_output_string(&emitter, output, sizeof(output), &written);
    yaml_emitter_set_encoding(&emitter, YAML_UTF16BE_ENCODING);
    yaml_emitter_set_unicode(&emitter, 1);

    yaml_stream_start_event_initialize(&event, YAML_UTF16BE_ENCODING);
    ASSERT(yaml_emitter_emit(&emitter, &event), "emit stream start");
    yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1);
    ASSERT(yaml_emitter_emit(&emitter, &event), "emit doc start");
    yaml_scalar_event_initialize(&event, NULL, (const yaml_char_t *)YAML_STR_TAG,
        text, 4, 1, 1, YAML_DOUBLE_QUOTED_SCALAR_STYLE);
    ASSERT(yaml_emitter_emit(&emitter, &event), "emit 4byte scalar");
    yaml_document_end_event_initialize(&event, 1);
    ASSERT(yaml_emitter_emit(&emitter, &event), "emit doc end");
    yaml_stream_end_event_initialize(&event);
    ASSERT(yaml_emitter_emit(&emitter, &event), "emit stream end");

    yaml_emitter_delete(&emitter);
    ASSERT(written > 0, "UTF-16BE 4byte output should not be empty");
}

/* Test UTF-16 4-byte roundtrip via escape sequence */
static void test_writer_utf16_4byte_roundtrip(void) {
    yaml_emitter_t emitter;
    yaml_event_t event;
    unsigned char output[8192];
    size_t written = 0;
    yaml_parser_t parser;
    int found_scalar = 0;

    const yaml_char_t *text = (const yaml_char_t *)"\xF0\x9F\x98\x80";

    ASSERT(yaml_emitter_initialize(&emitter), "4byte rt init");
    yaml_emitter_set_output_string(&emitter, output, sizeof(output), &written);
    yaml_emitter_set_encoding(&emitter, YAML_UTF16LE_ENCODING);
    yaml_emitter_set_unicode(&emitter, 1);

    yaml_stream_start_event_initialize(&event, YAML_UTF16LE_ENCODING);
    ASSERT(yaml_emitter_emit(&emitter, &event), "rt emit stream start");
    yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1);
    ASSERT(yaml_emitter_emit(&emitter, &event), "rt emit doc start");
    yaml_scalar_event_initialize(&event, NULL, (const yaml_char_t *)YAML_STR_TAG,
        text, 4, 1, 1, YAML_DOUBLE_QUOTED_SCALAR_STYLE);
    ASSERT(yaml_emitter_emit(&emitter, &event), "rt emit scalar");
    yaml_document_end_event_initialize(&event, 1);
    ASSERT(yaml_emitter_emit(&emitter, &event), "rt emit doc end");
    yaml_stream_end_event_initialize(&event);
    ASSERT(yaml_emitter_emit(&emitter, &event), "rt emit stream end");

    yaml_emitter_delete(&emitter);

    /* Parse back -- parser decodes \U escape to original UTF-8 bytes */
    ASSERT(yaml_parser_initialize(&parser), "rt parser init");
    yaml_parser_set_input_string(&parser, output, written);

    while (1) {
        ASSERT(yaml_parser_parse(&parser, &event), "rt parse event");
        if (event.type == YAML_SCALAR_EVENT) {
            ASSERT_EQ_INT((int)event.data.scalar.length, 4, "rt scalar len=4");
            ASSERT(memcmp(event.data.scalar.value, "\xF0\x9F\x98\x80", 4) == 0,
                   "rt scalar value matches U+1F600 UTF-8");
            found_scalar = 1;
        }
        if (event.type == YAML_STREAM_END_EVENT) {
            yaml_event_delete(&event);
            break;
        }
        yaml_event_delete(&event);
    }
    yaml_parser_delete(&parser);

    ASSERT(found_scalar, "4byte roundtrip: found scalar");
}

/* Test emitter output buffer overflow */
static void test_writer_output_buffer_overflow(void) {
    yaml_emitter_t emitter;
    yaml_event_t event;
    unsigned char output[32]; /* tiny buffer */
    size_t written = 0;

    ASSERT(yaml_emitter_initialize(&emitter), "overflow init");
    yaml_emitter_set_output_string(&emitter, output, sizeof(output), &written);
    yaml_emitter_set_encoding(&emitter, YAML_UTF8_ENCODING);

    yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
    if (!yaml_emitter_emit(&emitter, &event)) goto done;

    yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 0);
    if (!yaml_emitter_emit(&emitter, &event)) goto done;

    /* Large scalar will overflow 32-byte output */
    {
        char big[256];
        memset(big, 'z', sizeof(big));
        yaml_scalar_event_initialize(&event, NULL, (const yaml_char_t *)YAML_STR_TAG,
            (const yaml_char_t *)big, sizeof(big), 1, 1, YAML_PLAIN_SCALAR_STYLE);
        if (!yaml_emitter_emit(&emitter, &event)) goto done;
    }

done:
    /* written should be capped at sizeof(output) */
    ASSERT(written <= sizeof(output), "written <= buffer size");
    yaml_emitter_delete(&emitter);
}

/* Test emitter set_width with 0 (should use INT_MAX) */
static void test_writer_width_zero(void) {
    yaml_emitter_t emitter;
    yaml_event_t event;
    unsigned char output[8192];
    size_t written = 0;

    ASSERT(yaml_emitter_initialize(&emitter), "width zero init");
    yaml_emitter_set_output_string(&emitter, output, sizeof(output), &written);
    yaml_emitter_set_encoding(&emitter, YAML_UTF8_ENCODING);
    yaml_emitter_set_width(&emitter, -1); /* triggers INT_MAX path */

    yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
    ASSERT(yaml_emitter_emit(&emitter, &event), "width zero stream start");

    yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1);
    ASSERT(yaml_emitter_emit(&emitter, &event), "width zero doc start");

    /* Emit a long mapping to test width behavior */
    yaml_mapping_start_event_initialize(&event, NULL, NULL, 1,
        YAML_BLOCK_MAPPING_STYLE);
    ASSERT(yaml_emitter_emit(&emitter, &event), "width zero map start");

    yaml_scalar_event_initialize(&event, NULL, (const yaml_char_t *)YAML_STR_TAG,
        (const yaml_char_t *)"key", 3, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    ASSERT(yaml_emitter_emit(&emitter, &event), "width zero key");

    yaml_scalar_event_initialize(&event, NULL, (const yaml_char_t *)YAML_STR_TAG,
        (const yaml_char_t *)"value", 5, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    ASSERT(yaml_emitter_emit(&emitter, &event), "width zero value");

    yaml_mapping_end_event_initialize(&event);
    ASSERT(yaml_emitter_emit(&emitter, &event), "width zero map end");

    yaml_document_end_event_initialize(&event, 1);
    ASSERT(yaml_emitter_emit(&emitter, &event), "width zero doc end");

    yaml_stream_end_event_initialize(&event);
    ASSERT(yaml_emitter_emit(&emitter, &event), "width zero stream end");

    yaml_emitter_delete(&emitter);
    ASSERT(written > 0, "width zero produced output");
}

/* Test emitter with verbatim tag */
static void test_writer_verbatim_tag(void) {
    yaml_emitter_t emitter;
    yaml_event_t event;
    unsigned char output[8192];
    size_t written = 0;

    ASSERT(yaml_emitter_initialize(&emitter), "verbatim tag init");
    yaml_emitter_set_output_string(&emitter, output, sizeof(output), &written);

    yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
    ASSERT(yaml_emitter_emit(&emitter, &event), "vt stream start");

    yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1);
    ASSERT(yaml_emitter_emit(&emitter, &event), "vt doc start");

    /* Use a verbatim tag (no known handle) */
    yaml_scalar_event_initialize(&event, NULL,
        (const yaml_char_t *)"tag:example.com,2024:custom",
        (const yaml_char_t *)"value", 5, 0, 0, YAML_DOUBLE_QUOTED_SCALAR_STYLE);
    ASSERT(yaml_emitter_emit(&emitter, &event), "vt scalar");

    yaml_document_end_event_initialize(&event, 1);
    ASSERT(yaml_emitter_emit(&emitter, &event), "vt doc end");

    yaml_stream_end_event_initialize(&event);
    ASSERT(yaml_emitter_emit(&emitter, &event), "vt stream end");

    yaml_emitter_delete(&emitter);

    output[written] = '\0';
    /* Should contain verbatim tag !<...> */
    ASSERT(strstr((char *)output, "!<tag:example.com,2024:custom>") != NULL,
           "output contains verbatim tag");
}

/* Test emitter with flow alias */
static void test_writer_flow_alias(void) {
    yaml_emitter_t emitter;
    yaml_event_t event;
    unsigned char output[8192];
    size_t written = 0;

    ASSERT(yaml_emitter_initialize(&emitter), "flow alias init");
    yaml_emitter_set_output_string(&emitter, output, sizeof(output), &written);

    yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
    ASSERT(yaml_emitter_emit(&emitter, &event), "fa stream start");

    yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1);
    ASSERT(yaml_emitter_emit(&emitter, &event), "fa doc start");

    /* Flow sequence with anchor and alias */
    yaml_sequence_start_event_initialize(&event, NULL, NULL, 1,
        YAML_FLOW_SEQUENCE_STYLE);
    ASSERT(yaml_emitter_emit(&emitter, &event), "fa seq start");

    yaml_scalar_event_initialize(&event,
        (const yaml_char_t *)"anchor1", (const yaml_char_t *)YAML_STR_TAG,
        (const yaml_char_t *)"data", 4, 0, 0, YAML_PLAIN_SCALAR_STYLE);
    ASSERT(yaml_emitter_emit(&emitter, &event), "fa anchored scalar");

    yaml_alias_event_initialize(&event, (const yaml_char_t *)"anchor1");
    ASSERT(yaml_emitter_emit(&emitter, &event), "fa alias");

    yaml_sequence_end_event_initialize(&event);
    ASSERT(yaml_emitter_emit(&emitter, &event), "fa seq end");

    yaml_document_end_event_initialize(&event, 1);
    ASSERT(yaml_emitter_emit(&emitter, &event), "fa doc end");

    yaml_stream_end_event_initialize(&event);
    ASSERT(yaml_emitter_emit(&emitter, &event), "fa stream end");

    yaml_emitter_delete(&emitter);

    output[written] = '\0';
    ASSERT(strstr((char *)output, "*anchor1") != NULL,
           "output contains alias reference");
}

/* Test emitter with flow mapping containing anchor */
static void test_writer_flow_mapping_anchor(void) {
    yaml_emitter_t emitter;
    yaml_event_t event;
    unsigned char output[8192];
    size_t written = 0;

    ASSERT(yaml_emitter_initialize(&emitter), "flow map anchor init");
    yaml_emitter_set_output_string(&emitter, output, sizeof(output), &written);

    yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
    ASSERT(yaml_emitter_emit(&emitter, &event), "fma stream start");

    yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1);
    ASSERT(yaml_emitter_emit(&emitter, &event), "fma doc start");

    /* Flow sequence containing anchored mapping */
    yaml_sequence_start_event_initialize(&event, NULL, NULL, 1,
        YAML_FLOW_SEQUENCE_STYLE);
    ASSERT(yaml_emitter_emit(&emitter, &event), "fma seq start");

    yaml_mapping_start_event_initialize(&event,
        (const yaml_char_t *)"mapanchor",
        (const yaml_char_t *)"tag:yaml.org,2002:map", 0,
        YAML_FLOW_MAPPING_STYLE);
    ASSERT(yaml_emitter_emit(&emitter, &event), "fma map start");

    yaml_scalar_event_initialize(&event, NULL, (const yaml_char_t *)YAML_STR_TAG,
        (const yaml_char_t *)"k", 1, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    ASSERT(yaml_emitter_emit(&emitter, &event), "fma key");

    yaml_scalar_event_initialize(&event, NULL, (const yaml_char_t *)YAML_STR_TAG,
        (const yaml_char_t *)"v", 1, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    ASSERT(yaml_emitter_emit(&emitter, &event), "fma value");

    yaml_mapping_end_event_initialize(&event);
    ASSERT(yaml_emitter_emit(&emitter, &event), "fma map end");

    yaml_sequence_end_event_initialize(&event);
    ASSERT(yaml_emitter_emit(&emitter, &event), "fma seq end");

    yaml_document_end_event_initialize(&event, 1);
    ASSERT(yaml_emitter_emit(&emitter, &event), "fma doc end");

    yaml_stream_end_event_initialize(&event);
    ASSERT(yaml_emitter_emit(&emitter, &event), "fma stream end");

    yaml_emitter_delete(&emitter);

    output[written] = '\0';
    ASSERT(strstr((char *)output, "&mapanchor") != NULL,
           "output contains mapping anchor");
}

/* Test emitter with flow sequence containing anchor and tag */
static void test_writer_flow_sequence_anchor(void) {
    yaml_emitter_t emitter;
    yaml_event_t event;
    unsigned char output[8192];
    size_t written = 0;

    ASSERT(yaml_emitter_initialize(&emitter), "flow seq anchor init");
    yaml_emitter_set_output_string(&emitter, output, sizeof(output), &written);

    yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
    ASSERT(yaml_emitter_emit(&emitter, &event), "fsa stream start");

    yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1);
    ASSERT(yaml_emitter_emit(&emitter, &event), "fsa doc start");

    /* Anchored flow sequence nested in another */
    yaml_sequence_start_event_initialize(&event, NULL, NULL, 1,
        YAML_FLOW_SEQUENCE_STYLE);
    ASSERT(yaml_emitter_emit(&emitter, &event), "fsa outer seq start");

    yaml_sequence_start_event_initialize(&event,
        (const yaml_char_t *)"seqanc",
        (const yaml_char_t *)"tag:yaml.org,2002:seq", 0,
        YAML_FLOW_SEQUENCE_STYLE);
    ASSERT(yaml_emitter_emit(&emitter, &event), "fsa inner seq start");

    yaml_scalar_event_initialize(&event, NULL, (const yaml_char_t *)YAML_STR_TAG,
        (const yaml_char_t *)"item", 4, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    ASSERT(yaml_emitter_emit(&emitter, &event), "fsa item");

    yaml_sequence_end_event_initialize(&event);
    ASSERT(yaml_emitter_emit(&emitter, &event), "fsa inner seq end");

    yaml_sequence_end_event_initialize(&event);
    ASSERT(yaml_emitter_emit(&emitter, &event), "fsa outer seq end");

    yaml_document_end_event_initialize(&event, 1);
    ASSERT(yaml_emitter_emit(&emitter, &event), "fsa doc end");

    yaml_stream_end_event_initialize(&event);
    ASSERT(yaml_emitter_emit(&emitter, &event), "fsa stream end");

    yaml_emitter_delete(&emitter);

    output[written] = '\0';
    ASSERT(strstr((char *)output, "&seqanc") != NULL,
           "output contains sequence anchor");
}

/* Test emitter file output */
static void test_writer_file_output(void) {
    yaml_emitter_t emitter;
    yaml_event_t event;

    FILE *tmpf = tmpfile();
    ASSERT_NOT_NULL(tmpf, "tmpfile for writer");

    ASSERT(yaml_emitter_initialize(&emitter), "file output init");
    yaml_emitter_set_output_file(&emitter, tmpf);

    yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
    ASSERT(yaml_emitter_emit(&emitter, &event), "file stream start");

    yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1);
    ASSERT(yaml_emitter_emit(&emitter, &event), "file doc start");

    yaml_scalar_event_initialize(&event, NULL, (const yaml_char_t *)YAML_STR_TAG,
        (const yaml_char_t *)"filetest", 8, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    ASSERT(yaml_emitter_emit(&emitter, &event), "file scalar");

    yaml_document_end_event_initialize(&event, 1);
    ASSERT(yaml_emitter_emit(&emitter, &event), "file doc end");

    yaml_stream_end_event_initialize(&event);
    ASSERT(yaml_emitter_emit(&emitter, &event), "file stream end");

    yaml_emitter_delete(&emitter);

    /* Verify content by parsing the file back */
    rewind(tmpf);
    yaml_parser_t parser;
    int found = 0;
    ASSERT(yaml_parser_initialize(&parser), "file parse init");
    yaml_parser_set_input_file(&parser, tmpf);
    while (1) {
        ASSERT(yaml_parser_parse(&parser, &event), "file parse event");
        if (event.type == YAML_SCALAR_EVENT &&
            strcmp((const char *)event.data.scalar.value, "filetest") == 0)
            found = 1;
        if (event.type == YAML_STREAM_END_EVENT) {
            yaml_event_delete(&event);
            break;
        }
        yaml_event_delete(&event);
    }
    yaml_parser_delete(&parser);
    fclose(tmpf);

    ASSERT(found, "file roundtrip found scalar");
}

/* Test emitter with canonical mode */
static void test_writer_canonical(void) {
    yaml_emitter_t emitter;
    yaml_event_t event;
    unsigned char output[8192];
    size_t written = 0;

    ASSERT(yaml_emitter_initialize(&emitter), "canonical init");
    yaml_emitter_set_output_string(&emitter, output, sizeof(output), &written);
    yaml_emitter_set_canonical(&emitter, 1);

    yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
    ASSERT(yaml_emitter_emit(&emitter, &event), "canon stream start");

    yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1);
    ASSERT(yaml_emitter_emit(&emitter, &event), "canon doc start");

    yaml_scalar_event_initialize(&event, NULL, (const yaml_char_t *)YAML_STR_TAG,
        (const yaml_char_t *)"test", 4, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    ASSERT(yaml_emitter_emit(&emitter, &event), "canon scalar");

    yaml_document_end_event_initialize(&event, 1);
    ASSERT(yaml_emitter_emit(&emitter, &event), "canon doc end");

    yaml_stream_end_event_initialize(&event);
    ASSERT(yaml_emitter_emit(&emitter, &event), "canon stream end");

    yaml_emitter_delete(&emitter);

    output[written] = '\0';
    /* Canonical mode: explicit doc start marker and tags */
    ASSERT(strstr((char *)output, "---") != NULL, "canonical has ---");
    ASSERT(written > 0, "canonical produced output");
}

/* Test emitter with custom write handler (success) */
struct buffer_data {
    unsigned char *buf;
    size_t size;
    size_t used;
};

static int buffer_write_handler(void *data, unsigned char *buffer, size_t size) {
    struct buffer_data *bd = (struct buffer_data *)data;
    if (bd->used + size > bd->size) return 0;
    memcpy(bd->buf + bd->used, buffer, size);
    bd->used += size;
    return 1;
}

static void test_writer_custom_handler(void) {
    yaml_emitter_t emitter;
    yaml_event_t event;
    unsigned char buf[4096];
    struct buffer_data data = { buf, sizeof(buf), 0 };

    ASSERT(yaml_emitter_initialize(&emitter), "custom handler init");
    yaml_emitter_set_output(&emitter, buffer_write_handler, &data);

    yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
    ASSERT(yaml_emitter_emit(&emitter, &event), "ch stream start");

    yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1);
    ASSERT(yaml_emitter_emit(&emitter, &event), "ch doc start");

    yaml_scalar_event_initialize(&event, NULL, (const yaml_char_t *)YAML_STR_TAG,
        (const yaml_char_t *)"custom", 6, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    ASSERT(yaml_emitter_emit(&emitter, &event), "ch scalar");

    yaml_document_end_event_initialize(&event, 1);
    ASSERT(yaml_emitter_emit(&emitter, &event), "ch doc end");

    yaml_stream_end_event_initialize(&event);
    ASSERT(yaml_emitter_emit(&emitter, &event), "ch stream end");

    yaml_emitter_delete(&emitter);

    buf[data.used] = '\0';
    ASSERT(strstr((char *)buf, "custom") != NULL, "custom handler output has scalar");
}

/* Test emitter with folded scalar that has line breaks */
static void test_writer_folded_scalar_breaks(void) {
    yaml_emitter_t emitter;
    yaml_event_t event;
    unsigned char output[8192];
    size_t written = 0;

    ASSERT(yaml_emitter_initialize(&emitter), "folded breaks init");
    yaml_emitter_set_output_string(&emitter, output, sizeof(output), &written);

    yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
    ASSERT(yaml_emitter_emit(&emitter, &event), "fb stream start");

    yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1);
    ASSERT(yaml_emitter_emit(&emitter, &event), "fb doc start");

    /* Folded scalar with multiple paragraphs */
    const char *text = "first line\n\nsecond paragraph\nthird line\n";
    yaml_scalar_event_initialize(&event, NULL, (const yaml_char_t *)YAML_STR_TAG,
        (const yaml_char_t *)text, strlen(text), 0, 0,
        YAML_FOLDED_SCALAR_STYLE);
    ASSERT(yaml_emitter_emit(&emitter, &event), "fb scalar");

    yaml_document_end_event_initialize(&event, 1);
    ASSERT(yaml_emitter_emit(&emitter, &event), "fb doc end");

    yaml_stream_end_event_initialize(&event);
    ASSERT(yaml_emitter_emit(&emitter, &event), "fb stream end");

    yaml_emitter_delete(&emitter);

    ASSERT(written > 0, "folded breaks produced output");
}

/* Test emitter with literal scalar that has trailing newlines (strip chomp) */
static void test_writer_literal_strip_chomp(void) {
    yaml_emitter_t emitter;
    yaml_event_t event;
    unsigned char output[8192];
    size_t written = 0;

    ASSERT(yaml_emitter_initialize(&emitter), "literal strip init");
    yaml_emitter_set_output_string(&emitter, output, sizeof(output), &written);

    yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
    ASSERT(yaml_emitter_emit(&emitter, &event), "ls stream start");

    yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1);
    ASSERT(yaml_emitter_emit(&emitter, &event), "ls doc start");

    /* Literal scalar without trailing newline (triggers strip chomp "-") */
    const char *text = "line one\nline two";
    yaml_scalar_event_initialize(&event, NULL, (const yaml_char_t *)YAML_STR_TAG,
        (const yaml_char_t *)text, strlen(text), 0, 0,
        YAML_LITERAL_SCALAR_STYLE);
    ASSERT(yaml_emitter_emit(&emitter, &event), "ls scalar");

    yaml_document_end_event_initialize(&event, 1);
    ASSERT(yaml_emitter_emit(&emitter, &event), "ls doc end");

    yaml_stream_end_event_initialize(&event);
    ASSERT(yaml_emitter_emit(&emitter, &event), "ls stream end");

    yaml_emitter_delete(&emitter);

    output[written] = '\0';
    /* Should contain strip indicator "|-" */
    ASSERT(strstr((char *)output, "|-") != NULL, "literal strip has |- indicator");
}

int main(void) {
    TEST_SUITE_BEGIN("writer");

    test_writer_utf8();
    test_writer_utf16le();
    test_writer_utf16be();
    test_writer_utf16_multibyte();
    test_writer_utf16be_multibyte();
    test_writer_error_utf8();
    test_writer_error_utf16();
    test_writer_utf16_roundtrip();
    test_writer_utf16be_roundtrip();

    /* Extended */
    test_writer_utf16le_4byte();
    test_writer_utf16be_4byte();
    test_writer_utf16_4byte_roundtrip();
    test_writer_output_buffer_overflow();
    test_writer_width_zero();
    test_writer_verbatim_tag();
    test_writer_flow_alias();
    test_writer_flow_mapping_anchor();
    test_writer_flow_sequence_anchor();
    test_writer_file_output();
    test_writer_canonical();
    test_writer_custom_handler();
    test_writer_folded_scalar_breaks();
    test_writer_literal_strip_chomp();

    TEST_SUITE_END();
}
