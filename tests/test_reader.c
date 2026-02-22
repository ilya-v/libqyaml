#include "test_helper.h"
#include <assert.h>

/* Test various UTF-8 inputs */
static void test_utf8_input(void) {
    /* Simple ASCII */
    {
        yaml_parser_t parser;
        yaml_event_t event;
        const char *input = "hello: world";

        ASSERT(yaml_parser_initialize(&parser), "parser init");
        yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));
        ASSERT(yaml_parser_parse(&parser, &event), "parse stream start");
        ASSERT_EQ_INT(event.type, YAML_STREAM_START_EVENT, "stream start event");
        yaml_event_delete(&event);
        yaml_parser_delete(&parser);
    }

    /* UTF-8 BOM */
    {
        yaml_parser_t parser;
        yaml_event_t event;
        const unsigned char input[] = {0xEF, 0xBB, 0xBF, 'h', 'i', 0};

        ASSERT(yaml_parser_initialize(&parser), "parser init with BOM");
        yaml_parser_set_input_string(&parser, input, sizeof(input) - 1);
        ASSERT(yaml_parser_parse(&parser, &event), "parse BOM stream");
        ASSERT_EQ_INT(event.type, YAML_STREAM_START_EVENT, "BOM stream start");
        ASSERT_EQ_INT(event.data.stream_start.encoding, YAML_UTF8_ENCODING, "BOM encoding UTF8");
        yaml_event_delete(&event);
        yaml_parser_delete(&parser);
    }

    /* Empty input */
    {
        yaml_parser_t parser;
        yaml_event_t event;

        ASSERT(yaml_parser_initialize(&parser), "parser init empty");
        yaml_parser_set_input_string(&parser, (const unsigned char *)"", 0);
        ASSERT(yaml_parser_parse(&parser, &event), "parse empty stream");
        ASSERT_EQ_INT(event.type, YAML_STREAM_START_EVENT, "empty stream start");
        yaml_event_delete(&event);

        ASSERT(yaml_parser_parse(&parser, &event), "parse empty doc");
        ASSERT_EQ_INT(event.type, YAML_STREAM_END_EVENT, "empty stream end");
        yaml_event_delete(&event);
        yaml_parser_delete(&parser);
    }

    /* Multi-byte UTF-8 characters */
    {
        yaml_parser_t parser;
        yaml_event_t event;
        /* "key: value" with Unicode characters */
        const char *input = "key: \xc3\xa9\xc3\xa8\xc3\xaa";

        ASSERT(yaml_parser_initialize(&parser), "parser init utf8 multibyte");
        yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));
        ASSERT(yaml_parser_parse(&parser, &event), "parse utf8 multibyte");
        ASSERT_EQ_INT(event.type, YAML_STREAM_START_EVENT, "utf8 multibyte stream start");
        yaml_event_delete(&event);
        yaml_parser_delete(&parser);
    }
}

/* Test encoding detection */
static void test_encoding_detection(void) {
    /* UTF-16LE BOM */
    {
        yaml_parser_t parser;
        yaml_event_t event;
        const unsigned char input[] = {0xFF, 0xFE, 'h', 0, 'i', 0};

        ASSERT(yaml_parser_initialize(&parser), "parser init UTF16LE");
        yaml_parser_set_input_string(&parser, input, sizeof(input));
        ASSERT(yaml_parser_parse(&parser, &event), "parse UTF16LE");
        ASSERT_EQ_INT(event.data.stream_start.encoding, YAML_UTF16LE_ENCODING, "UTF16LE detected");
        yaml_event_delete(&event);
        yaml_parser_delete(&parser);
    }

    /* UTF-16BE BOM */
    {
        yaml_parser_t parser;
        yaml_event_t event;
        const unsigned char input[] = {0xFE, 0xFF, 0, 'h', 0, 'i'};

        ASSERT(yaml_parser_initialize(&parser), "parser init UTF16BE");
        yaml_parser_set_input_string(&parser, input, sizeof(input));
        ASSERT(yaml_parser_parse(&parser, &event), "parse UTF16BE");
        ASSERT_EQ_INT(event.data.stream_start.encoding, YAML_UTF16BE_ENCODING, "UTF16BE detected");
        yaml_event_delete(&event);
        yaml_parser_delete(&parser);
    }

    /* Explicit encoding setting */
    {
        yaml_parser_t parser;
        yaml_event_t event;

        ASSERT(yaml_parser_initialize(&parser), "parser init explicit enc");
        yaml_parser_set_input_string(&parser, (const unsigned char *)"hi", 2);
        yaml_parser_set_encoding(&parser, YAML_UTF8_ENCODING);
        ASSERT(yaml_parser_parse(&parser, &event), "parse explicit enc");
        ASSERT_EQ_INT(event.data.stream_start.encoding, YAML_UTF8_ENCODING, "explicit UTF8");
        yaml_event_delete(&event);
        yaml_parser_delete(&parser);
    }
}

/* Test file input */
static void test_file_input(void) {
    FILE *tmpf = tmpfile();
    ASSERT_NOT_NULL(tmpf, "create temp file");
    if (!tmpf) return;

    const char *yaml = "key: value\n";
    fwrite(yaml, 1, strlen(yaml), tmpf);
    rewind(tmpf);

    yaml_parser_t parser;
    yaml_event_t event;
    ASSERT(yaml_parser_initialize(&parser), "parser init file");
    yaml_parser_set_input_file(&parser, tmpf);

    int found_scalar = 0;
    while (1) {
        ASSERT(yaml_parser_parse(&parser, &event), "parse file event");
        if (event.type == YAML_SCALAR_EVENT) {
            if (strcmp((const char *)event.data.scalar.value, "key") == 0)
                found_scalar = 1;
        }
        if (event.type == YAML_STREAM_END_EVENT) {
            yaml_event_delete(&event);
            break;
        }
        yaml_event_delete(&event);
    }
    ASSERT(found_scalar, "file: found key scalar");

    yaml_parser_delete(&parser);
    fclose(tmpf);
}

/* Test custom read handler */
struct string_reader_data {
    const unsigned char *str;
    size_t len;
    size_t pos;
};

static int string_read_handler(void *data, unsigned char *buffer, size_t size,
                                size_t *size_read) {
    struct string_reader_data *d = (struct string_reader_data *)data;
    size_t remaining = d->len - d->pos;
    size_t to_read = remaining < size ? remaining : size;
    if (to_read > 0) {
        memcpy(buffer, d->str + d->pos, to_read);
        d->pos += to_read;
    }
    *size_read = to_read;
    return 1;
}

static void test_custom_handler(void) {
    const char *yaml = "hello: world\n";
    struct string_reader_data data = {
        .str = (const unsigned char *)yaml,
        .len = strlen(yaml),
        .pos = 0
    };

    yaml_parser_t parser;
    yaml_event_t event;

    ASSERT(yaml_parser_initialize(&parser), "parser init custom handler");
    yaml_parser_set_input(&parser, string_read_handler, &data);

    int found_scalar = 0;
    while (1) {
        ASSERT(yaml_parser_parse(&parser, &event), "parse custom handler event");
        if (event.type == YAML_SCALAR_EVENT) {
            if (strcmp((const char *)event.data.scalar.value, "world") == 0)
                found_scalar = 1;
        }
        if (event.type == YAML_STREAM_END_EVENT) {
            yaml_event_delete(&event);
            break;
        }
        yaml_event_delete(&event);
    }
    ASSERT(found_scalar, "custom handler: found world scalar");

    yaml_parser_delete(&parser);
}

/* Test invalid input detection */
static void test_invalid_input(void) {
    /* Invalid UTF-8 sequence */
    {
        yaml_parser_t parser;
        yaml_event_t event;
        const unsigned char input[] = {0xFF, 0xFE, 0x00}; /* incomplete */

        ASSERT(yaml_parser_initialize(&parser), "parser init invalid");
        yaml_parser_set_input_string(&parser, input, 3);
        /* First event (stream start) should succeed */
        yaml_parser_parse(&parser, &event);
        yaml_event_delete(&event);
        yaml_parser_delete(&parser);
    }

    /* NUL bytes in string -- libyaml rejects these as invalid */
    {
        yaml_parser_t parser;
        yaml_event_t event;
        const unsigned char input[] = {'a', ':', ' ', 'b', 0, 'c'};

        ASSERT(yaml_parser_initialize(&parser), "parser init nul bytes");
        yaml_parser_set_input_string(&parser, input, 6);

        /* Stream start succeeds but scanning the content will fail */
        int ok = yaml_parser_parse(&parser, &event);
        if (ok) {
            yaml_event_delete(&event);
            /* Try to advance -- the NUL byte should cause a reader error */
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
            ASSERT(got_error, "nul bytes: should cause error");
        } else {
            /* Immediate error is also acceptable */
            ASSERT(1, "nul bytes: immediate error");
        }
        yaml_parser_delete(&parser);
    }
}

/* ==================================================================
 * Extended reader tests
 * ================================================================== */

/* Test failing read handler */
static int failing_read_handler(void *data, unsigned char *buffer, size_t size,
                                 size_t *size_read) {
    (void)data;
    (void)buffer;
    (void)size;
    *size_read = 0;
    return 0;
}

static void test_failing_handler(void) {
    yaml_parser_t parser;
    yaml_event_t event;

    ASSERT(yaml_parser_initialize(&parser), "failing handler init");
    yaml_parser_set_input(&parser, failing_read_handler, NULL);

    /* Stream start should succeed (no data needed yet) */
    int ok = yaml_parser_parse(&parser, &event);
    if (ok) {
        yaml_event_delete(&event);
        /* Next parse should fail */
        ok = yaml_parser_parse(&parser, &event);
        if (!ok) {
            ASSERT_EQ_INT(parser.error, YAML_READER_ERROR, "failing: reader error");
        }
    }
    yaml_parser_delete(&parser);
}

/* Test UTF-16LE input without BOM but with explicit encoding */
static void test_utf16le_no_bom(void) {
    yaml_parser_t parser;
    yaml_event_t event;
    /* "hi" in UTF-16LE without BOM: 'h' 0x00 'i' 0x00 '\n' 0x00 */
    const unsigned char input[] = {'h', 0, 'i', 0, '\n', 0};

    ASSERT(yaml_parser_initialize(&parser), "utf16le no bom init");
    yaml_parser_set_input_string(&parser, input, sizeof(input));
    yaml_parser_set_encoding(&parser, YAML_UTF16LE_ENCODING);

    ASSERT(yaml_parser_parse(&parser, &event), "utf16le no bom parse");
    ASSERT_EQ_INT(event.type, YAML_STREAM_START_EVENT, "stream start");
    yaml_event_delete(&event);

    /* Should be able to parse further */
    int found_scalar = 0;
    while (1) {
        ASSERT(yaml_parser_parse(&parser, &event), "utf16le no bom next");
        if (event.type == YAML_SCALAR_EVENT) found_scalar = 1;
        if (event.type == YAML_STREAM_END_EVENT) {
            yaml_event_delete(&event);
            break;
        }
        yaml_event_delete(&event);
    }
    ASSERT(found_scalar, "utf16le no bom found scalar");
    yaml_parser_delete(&parser);
}

/* Test UTF-16BE input without BOM */
static void test_utf16be_no_bom(void) {
    yaml_parser_t parser;
    yaml_event_t event;
    const unsigned char input[] = {0, 'h', 0, 'i', 0, '\n'};

    ASSERT(yaml_parser_initialize(&parser), "utf16be no bom init");
    yaml_parser_set_input_string(&parser, input, sizeof(input));
    yaml_parser_set_encoding(&parser, YAML_UTF16BE_ENCODING);

    ASSERT(yaml_parser_parse(&parser, &event), "utf16be no bom parse");
    ASSERT_EQ_INT(event.type, YAML_STREAM_START_EVENT, "stream start");
    yaml_event_delete(&event);

    int found_scalar = 0;
    while (1) {
        ASSERT(yaml_parser_parse(&parser, &event), "utf16be no bom next");
        if (event.type == YAML_SCALAR_EVENT) found_scalar = 1;
        if (event.type == YAML_STREAM_END_EVENT) {
            yaml_event_delete(&event);
            break;
        }
        yaml_event_delete(&event);
    }
    ASSERT(found_scalar, "utf16be no bom found scalar");
    yaml_parser_delete(&parser);
}

/* Test reading 4-byte UTF-8 (U+1F600) */
static void test_utf8_4byte(void) {
    yaml_parser_t parser;
    yaml_event_t event;
    /* U+1F600 = F0 9F 98 80 */
    const char *input = "key: \"\xF0\x9F\x98\x80\"\n";

    ASSERT(yaml_parser_initialize(&parser), "utf8 4byte init");
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));

    int found = 0;
    while (1) {
        ASSERT(yaml_parser_parse(&parser, &event), "utf8 4byte parse");
        if (event.type == YAML_SCALAR_EVENT) {
            if (event.data.scalar.length == 4 &&
                memcmp(event.data.scalar.value, "\xF0\x9F\x98\x80", 4) == 0)
                found = 1;
        }
        if (event.type == YAML_STREAM_END_EVENT) {
            yaml_event_delete(&event);
            break;
        }
        yaml_event_delete(&event);
    }
    ASSERT(found, "utf8 4byte found emoji scalar");
    yaml_parser_delete(&parser);
}

/* Test reading UTF-16LE with 4-byte UTF-8 (surrogate pair) */
static void test_utf16le_surrogate_input(void) {
    yaml_emitter_t emitter;
    yaml_event_t event;
    unsigned char output[8192];
    size_t written = 0;

    /* First emit a document with U+1F600 in UTF-16LE */
    const yaml_char_t *text = (const yaml_char_t *)"\xF0\x9F\x98\x80";

    ASSERT(yaml_emitter_initialize(&emitter), "surr input emit init");
    yaml_emitter_set_output_string(&emitter, output, sizeof(output), &written);
    yaml_emitter_set_encoding(&emitter, YAML_UTF16LE_ENCODING);
    yaml_emitter_set_unicode(&emitter, 1);

    yaml_stream_start_event_initialize(&event, YAML_UTF16LE_ENCODING);
    ASSERT(yaml_emitter_emit(&emitter, &event), "surr emit ss");
    yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1);
    ASSERT(yaml_emitter_emit(&emitter, &event), "surr emit ds");
    yaml_scalar_event_initialize(&event, NULL, (const yaml_char_t *)YAML_STR_TAG,
        text, 4, 1, 1, YAML_DOUBLE_QUOTED_SCALAR_STYLE);
    ASSERT(yaml_emitter_emit(&emitter, &event), "surr emit scalar");
    yaml_document_end_event_initialize(&event, 1);
    ASSERT(yaml_emitter_emit(&emitter, &event), "surr emit de");
    yaml_stream_end_event_initialize(&event);
    ASSERT(yaml_emitter_emit(&emitter, &event), "surr emit se");
    yaml_emitter_delete(&emitter);

    /* Now parse it back */
    yaml_parser_t parser;
    int found = 0;
    ASSERT(yaml_parser_initialize(&parser), "surr input parse init");
    yaml_parser_set_input_string(&parser, output, written);

    while (1) {
        ASSERT(yaml_parser_parse(&parser, &event), "surr input parse");
        if (event.type == YAML_SCALAR_EVENT) {
            if (event.data.scalar.length == 4 &&
                memcmp(event.data.scalar.value, "\xF0\x9F\x98\x80", 4) == 0)
                found = 1;
        }
        if (event.type == YAML_STREAM_END_EVENT) {
            yaml_event_delete(&event);
            break;
        }
        yaml_event_delete(&event);
    }
    yaml_parser_delete(&parser);
    ASSERT(found, "utf16le surrogate roundtrip found emoji");
}

/* Test large file input */
static void test_large_file_input(void) {
    FILE *tmpf = tmpfile();
    ASSERT_NOT_NULL(tmpf, "large file create");

    /* Write a sequence with many items */
    for (int i = 0; i < 100; i++) {
        fprintf(tmpf, "- item%d\n", i);
    }
    rewind(tmpf);

    yaml_parser_t parser;
    yaml_event_t event;
    ASSERT(yaml_parser_initialize(&parser), "large file init");
    yaml_parser_set_input_file(&parser, tmpf);

    int scalar_count = 0;
    while (1) {
        ASSERT(yaml_parser_parse(&parser, &event), "large file parse");
        if (event.type == YAML_SCALAR_EVENT) scalar_count++;
        if (event.type == YAML_STREAM_END_EVENT) {
            yaml_event_delete(&event);
            break;
        }
        yaml_event_delete(&event);
    }
    yaml_parser_delete(&parser);
    fclose(tmpf);

    ASSERT_EQ_INT(scalar_count, 100, "large file: 100 scalars");
}

/* Test chunked read handler (small chunks) */
static int chunked_read_handler(void *data, unsigned char *buffer, size_t size,
                                 size_t *size_read) {
    struct string_reader_data *d = (struct string_reader_data *)data;
    size_t remaining = d->len - d->pos;
    /* Return at most 3 bytes at a time */
    size_t to_read = remaining < 3 ? remaining : 3;
    if (to_read > size) to_read = size;
    if (to_read > 0) {
        memcpy(buffer, d->str + d->pos, to_read);
        d->pos += to_read;
    }
    *size_read = to_read;
    return 1;
}

static void test_chunked_handler(void) {
    const char *yaml = "key: value\nlist:\n- one\n- two\n";
    struct string_reader_data data = {
        .str = (const unsigned char *)yaml,
        .len = strlen(yaml),
        .pos = 0
    };

    yaml_parser_t parser;
    yaml_event_t event;

    ASSERT(yaml_parser_initialize(&parser), "chunked init");
    yaml_parser_set_input(&parser, chunked_read_handler, &data);

    int scalar_count = 0;
    while (1) {
        ASSERT(yaml_parser_parse(&parser, &event), "chunked parse");
        if (event.type == YAML_SCALAR_EVENT) scalar_count++;
        if (event.type == YAML_STREAM_END_EVENT) {
            yaml_event_delete(&event);
            break;
        }
        yaml_event_delete(&event);
    }
    yaml_parser_delete(&parser);

    ASSERT_EQ_INT(scalar_count, 5, "chunked: 5 scalars (key,value,list,one,two)");
}

/* Test multiple documents via file */
static void test_multi_doc_file(void) {
    FILE *tmpf = tmpfile();
    ASSERT_NOT_NULL(tmpf, "multi doc file create");

    const char *yaml = "---\nfirst\n...\n---\nsecond\n...\n";
    fwrite(yaml, 1, strlen(yaml), tmpf);
    rewind(tmpf);

    yaml_parser_t parser;
    yaml_event_t event;
    ASSERT(yaml_parser_initialize(&parser), "multi doc init");
    yaml_parser_set_input_file(&parser, tmpf);

    int doc_count = 0;
    while (1) {
        ASSERT(yaml_parser_parse(&parser, &event), "multi doc parse");
        if (event.type == YAML_DOCUMENT_START_EVENT) doc_count++;
        if (event.type == YAML_STREAM_END_EVENT) {
            yaml_event_delete(&event);
            break;
        }
        yaml_event_delete(&event);
    }
    yaml_parser_delete(&parser);
    fclose(tmpf);

    ASSERT_EQ_INT(doc_count, 2, "multi doc: 2 documents");
}

int main(void) {
    TEST_SUITE_BEGIN("Reader");

    test_utf8_input();
    test_encoding_detection();
    test_file_input();
    test_custom_handler();
    test_invalid_input();

    /* Extended */
    test_failing_handler();
    test_utf16le_no_bom();
    test_utf16be_no_bom();
    test_utf8_4byte();
    test_utf16le_surrogate_input();
    test_large_file_input();
    test_chunked_handler();
    test_multi_doc_file();

    TEST_SUITE_END();
}
