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

int main(void) {
    TEST_SUITE_BEGIN("Reader");

    test_utf8_input();
    test_encoding_detection();
    test_file_input();
    test_custom_handler();
    test_invalid_input();

    TEST_SUITE_END();
}
