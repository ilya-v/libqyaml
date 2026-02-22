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

    TEST_SUITE_END();
}
