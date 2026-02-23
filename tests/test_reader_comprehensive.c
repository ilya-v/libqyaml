#include "test_helper.h"

/*
 * Comprehensive reader tests covering encoding detection, BOM handling,
 * UTF-8 validation, control character detection, and reader edge cases.
 */

/* Helper: parse input and check if stream start succeeds */
__attribute__((unused))
static int parse_starts_ok(const unsigned char *input, size_t len) {
    yaml_parser_t parser;
    yaml_event_t event;
    int ok;

    if (!yaml_parser_initialize(&parser)) return 0;
    yaml_parser_set_input_string(&parser, input, len);
    ok = yaml_parser_parse(&parser, &event);
    if (ok) yaml_event_delete(&event);
    yaml_parser_delete(&parser);
    return ok;
}

/* Helper: parse and check encoding */
static yaml_encoding_t detect_encoding(const unsigned char *input, size_t len) {
    yaml_parser_t parser;
    yaml_event_t event;
    yaml_encoding_t enc = YAML_ANY_ENCODING;

    if (!yaml_parser_initialize(&parser)) return enc;
    yaml_parser_set_input_string(&parser, input, len);
    if (yaml_parser_parse(&parser, &event)) {
        enc = event.data.stream_start.encoding;
        yaml_event_delete(&event);
    }
    yaml_parser_delete(&parser);
    return enc;
}

/* Helper: check if full parse succeeds (reaches stream end) */
static int parse_completes(const unsigned char *input, size_t len) {
    yaml_parser_t parser;
    yaml_event_t event;
    int completed = 0;

    if (!yaml_parser_initialize(&parser)) return 0;
    yaml_parser_set_input_string(&parser, input, len);
    while (1) {
        if (!yaml_parser_parse(&parser, &event)) break;
        if (event.type == YAML_STREAM_END_EVENT) {
            yaml_event_delete(&event);
            completed = 1;
            break;
        }
        yaml_event_delete(&event);
    }
    yaml_parser_delete(&parser);
    return completed;
}

/* ========== Encoding detection ========== */

static void test_r_detect_utf8_no_bom(void) {
    yaml_encoding_t enc = detect_encoding((const unsigned char *)"hello", 5);
    ASSERT_EQ_INT(enc, YAML_UTF8_ENCODING, "UTF-8 no BOM");
}

static void test_r_detect_utf8_bom(void) {
    const unsigned char input[] = {0xEF, 0xBB, 0xBF, 'h', 'i'};
    yaml_encoding_t enc = detect_encoding(input, sizeof(input));
    ASSERT_EQ_INT(enc, YAML_UTF8_ENCODING, "UTF-8 with BOM");
}

static void test_r_detect_utf16le_bom(void) {
    const unsigned char input[] = {0xFF, 0xFE, 'h', 0, 'i', 0};
    yaml_encoding_t enc = detect_encoding(input, sizeof(input));
    ASSERT_EQ_INT(enc, YAML_UTF16LE_ENCODING, "UTF-16LE BOM");
}

static void test_r_detect_utf16be_bom(void) {
    const unsigned char input[] = {0xFE, 0xFF, 0, 'h', 0, 'i'};
    yaml_encoding_t enc = detect_encoding(input, sizeof(input));
    ASSERT_EQ_INT(enc, YAML_UTF16BE_ENCODING, "UTF-16BE BOM");
}

static void test_r_explicit_utf8(void) {
    yaml_parser_t parser;
    yaml_event_t event;
    yaml_parser_initialize(&parser);
    yaml_parser_set_encoding(&parser, YAML_UTF8_ENCODING);
    yaml_parser_set_input_string(&parser, (const unsigned char *)"a", 1);
    yaml_parser_parse(&parser, &event);
    ASSERT_EQ_INT(event.data.stream_start.encoding, YAML_UTF8_ENCODING, "explicit UTF-8");
    yaml_event_delete(&event);
    yaml_parser_delete(&parser);
}

/* ========== UTF-8 validation ========== */

static void test_r_ascii_printable(void) {
    ASSERT(parse_completes((const unsigned char *)"hello world", 11), "ASCII printable");
}

static void test_r_ascii_digits(void) {
    ASSERT(parse_completes((const unsigned char *)"0123456789", 10), "ASCII digits");
}

static void test_r_ascii_symbols(void) {
    ASSERT(parse_completes((const unsigned char *)"\"!@#$%^&*()\"", 12), "ASCII symbols");
}

static void test_r_utf8_2byte(void) {
    /* Latin small e with acute: U+00E9 = 0xC3 0xA9 */
    ASSERT(parse_completes((const unsigned char *)"\xc3\xa9", 2), "UTF-8 2-byte");
}

static void test_r_utf8_3byte(void) {
    /* Euro sign: U+20AC = 0xE2 0x82 0xAC */
    ASSERT(parse_completes((const unsigned char *)"\xe2\x82\xac", 3), "UTF-8 3-byte");
}

static void test_r_utf8_4byte(void) {
    /* U+1F600 = 0xF0 0x9F 0x98 0x80 */
    ASSERT(parse_completes((const unsigned char *)"\xf0\x9f\x98\x80", 4), "UTF-8 4-byte");
}

static void test_r_utf8_mixed(void) {
    const char *input = "key: \xc3\xa9l\xc3\xa8ve";
    ASSERT(parse_completes((const unsigned char *)input, strlen(input)), "UTF-8 mixed");
}

static void test_r_utf8_japanese(void) {
    /* Hiragana "ka": U+304B = 0xE3 0x81 0x8B */
    const char *input = "\xe3\x81\x8b";
    ASSERT(parse_completes((const unsigned char *)input, strlen(input)), "UTF-8 Japanese");
}

static void test_r_utf8_chinese(void) {
    /* CJK Unified "yi": U+4E00 = 0xE4 0xB8 0x80 */
    const char *input = "\xe4\xb8\x80";
    ASSERT(parse_completes((const unsigned char *)input, strlen(input)), "UTF-8 Chinese");
}

static void test_r_utf8_emoji(void) {
    const char *input = "\xf0\x9f\x98\x80";
    ASSERT(parse_completes((const unsigned char *)input, strlen(input)), "UTF-8 emoji");
}

/* ========== Invalid UTF-8 ========== */

static void test_r_invalid_continuation(void) {
    /* 0x80 alone is an invalid continuation byte */
    const unsigned char input[] = {0x80};
    ASSERT(!parse_completes(input, 1), "invalid continuation rejected");
}

static void test_r_invalid_overlong_2byte(void) {
    /* Overlong encoding of '/' (U+002F): 0xC0 0xAF */
    const unsigned char input[] = {0xC0, 0xAF};
    ASSERT(!parse_completes(input, 2), "overlong 2-byte rejected");
}

static void test_r_invalid_truncated_2byte(void) {
    /* Start of 2-byte sequence but missing continuation */
    const unsigned char input[] = {0xC3};
    ASSERT(!parse_completes(input, 1), "truncated 2-byte rejected");
}

static void test_r_invalid_truncated_3byte(void) {
    /* Start of 3-byte sequence but only 1 continuation */
    const unsigned char input[] = {0xE2, 0x82};
    ASSERT(!parse_completes(input, 2), "truncated 3-byte rejected");
}

static void test_r_invalid_fe(void) {
    /* 0xFE is never valid in UTF-8 */
    const unsigned char input[] = {0xFE};
    /* May be interpreted as BOM start - just verify no crash */
    (void)parse_completes(input, 1);
    ASSERT(1, "0xFE handled without crash");
}

/* ========== Control characters ========== */

static void test_r_tab_in_content(void) {
    /* Tab is allowed in some contexts in YAML */
    ASSERT(parse_completes((const unsigned char *)"\"a\tb\"", 5), "tab in double quote");
}

static void test_r_newline_lf(void) {
    ASSERT(parse_completes((const unsigned char *)"a\nb", 3), "LF");
}

static void test_r_newline_cr(void) {
    ASSERT(parse_completes((const unsigned char *)"a\rb", 3), "CR");
}

static void test_r_newline_crlf(void) {
    ASSERT(parse_completes((const unsigned char *)"a\r\nb", 4), "CRLF");
}

static void test_r_nul_rejected(void) {
    const unsigned char input[] = {'a', 0, 'b'};
    ASSERT(!parse_completes(input, 3), "NUL rejected in content");
}

static void test_r_control_char_rejected(void) {
    /* ASCII control chars (except TAB, LF, CR) should be rejected */
    const unsigned char input[] = {'\"', 0x01, '\"'};
    ASSERT(!parse_completes(input, 3), "control char 0x01 rejected");
}

static void test_r_control_char_bel(void) {
    const unsigned char input[] = {'\"', 0x07, '\"'};
    ASSERT(!parse_completes(input, 3), "BEL rejected");
}

static void test_r_control_char_bs(void) {
    const unsigned char input[] = {'\"', 0x08, '\"'};
    ASSERT(!parse_completes(input, 3), "BS rejected");
}

static void test_r_control_char_vt(void) {
    const unsigned char input[] = {'\"', 0x0B, '\"'};
    ASSERT(!parse_completes(input, 3), "VT rejected");
}

static void test_r_control_char_ff(void) {
    const unsigned char input[] = {'\"', 0x0C, '\"'};
    ASSERT(!parse_completes(input, 3), "FF rejected");
}

static void test_r_control_char_esc(void) {
    const unsigned char input[] = {'\"', 0x1B, '\"'};
    ASSERT(!parse_completes(input, 3), "ESC rejected");
}

static void test_r_del_char(void) {
    const unsigned char input[] = {'\"', 0x7F, '\"'};
    ASSERT(!parse_completes(input, 3), "DEL rejected");
}

/* ========== BOM handling ========== */

static void test_r_utf8_bom_stripped(void) {
    const unsigned char input[] = {0xEF, 0xBB, 0xBF, 'h', 'i'};
    yaml_parser_t parser;
    yaml_event_t event;
    int found = 0;

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, input, sizeof(input));
    while (1) {
        if (!yaml_parser_parse(&parser, &event)) break;
        if (event.type == YAML_SCALAR_EVENT) {
            ASSERT_EQ_STR(event.data.scalar.value, "hi", "BOM stripped from content");
            found = 1;
        }
        int done = (event.type == YAML_STREAM_END_EVENT);
        yaml_event_delete(&event);
        if (done) break;
    }
    yaml_parser_delete(&parser);
    ASSERT(found, "found scalar after BOM");
}

static void test_r_bom_only(void) {
    const unsigned char input[] = {0xEF, 0xBB, 0xBF, 0x00};
    yaml_event_type_t ev[8];
    int n;
    ASSERT(parse_string_events((const char *)input, ev, 8, &n), "BOM only parse");
    ASSERT_EQ_INT(n, 2, "BOM only: 2 events");
}

/* ========== Empty and edge-case inputs ========== */

static void test_r_empty_input(void) {
    ASSERT(parse_completes((const unsigned char *)"", 0), "empty");
}

static void test_r_single_newline(void) {
    ASSERT(parse_completes((const unsigned char *)"\n", 1), "single LF");
}

static void test_r_single_space(void) {
    ASSERT(parse_completes((const unsigned char *)" ", 1), "single space");
}

static void test_r_many_newlines(void) {
    ASSERT(parse_completes((const unsigned char *)"\n\n\n\n\n", 5), "many LF");
}

static void test_r_many_spaces(void) {
    ASSERT(parse_completes((const unsigned char *)"     ", 5), "many spaces");
}

static void test_r_large_input(void) {
    /* 64KB of spaces + scalar */
    size_t len = 65536 + 6;
    unsigned char *buf = malloc(len);
    if (!buf) { ASSERT(0, "malloc failed"); return; }
    memset(buf, ' ', 65536);
    memcpy(buf + 65536, "hello\n", 6);
    ASSERT(parse_completes(buf, len), "64KB input");
    free(buf);
}

/* ========== File input ========== */

static void test_r_file_small(void) {
    FILE *f = tmpfile();
    if (!f) { ASSERT(0, "tmpfile"); return; }
    fprintf(f, "a: b\n");
    rewind(f);

    yaml_parser_t parser;
    yaml_event_t event;
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_file(&parser, f);

    int count = 0;
    while (1) {
        if (!yaml_parser_parse(&parser, &event)) break;
        count++;
        int done = (event.type == YAML_STREAM_END_EVENT);
        yaml_event_delete(&event);
        if (done) break;
    }
    yaml_parser_delete(&parser);
    fclose(f);
    ASSERT(count > 2, "file small: parsed events");
}

static void test_r_file_empty(void) {
    FILE *f = tmpfile();
    if (!f) { ASSERT(0, "tmpfile"); return; }
    rewind(f);

    yaml_parser_t parser;
    yaml_event_t event;
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_file(&parser, f);

    yaml_parser_parse(&parser, &event);
    ASSERT_EQ_INT(event.type, YAML_STREAM_START_EVENT, "empty file: stream start");
    yaml_event_delete(&event);

    yaml_parser_parse(&parser, &event);
    ASSERT_EQ_INT(event.type, YAML_STREAM_END_EVENT, "empty file: stream end");
    yaml_event_delete(&event);

    yaml_parser_delete(&parser);
    fclose(f);
}

/* ========== Custom reader edge cases ========== */

struct byte_reader_data {
    const unsigned char *buf;
    size_t len;
    size_t pos;
    size_t chunk_size;
};

static int byte_reader(void *data, unsigned char *buffer, size_t size, size_t *size_read) {
    struct byte_reader_data *d = data;
    size_t avail = d->len - d->pos;
    size_t n = avail < d->chunk_size ? avail : d->chunk_size;
    if (n > size) n = size;
    memcpy(buffer, d->buf + d->pos, n);
    d->pos += n;
    *size_read = n;
    return 1;
}

static void test_r_reader_byte_at_a_time(void) {
    const char *yaml = "key: value\n";
    struct byte_reader_data data = {
        (const unsigned char *)yaml, strlen(yaml), 0, 1
    };
    yaml_parser_t parser;
    yaml_event_t event;
    yaml_parser_initialize(&parser);
    yaml_parser_set_input(&parser, byte_reader, &data);

    int found = 0;
    while (1) {
        if (!yaml_parser_parse(&parser, &event)) break;
        if (event.type == YAML_SCALAR_EVENT &&
            strcmp((const char *)event.data.scalar.value, "value") == 0)
            found = 1;
        int done = (event.type == YAML_STREAM_END_EVENT);
        yaml_event_delete(&event);
        if (done) break;
    }
    yaml_parser_delete(&parser);
    ASSERT(found, "byte reader: found value");
}

static void test_r_reader_small_chunks(void) {
    const char *yaml = "- a\n- b\n- c\n";
    struct byte_reader_data data = {
        (const unsigned char *)yaml, strlen(yaml), 0, 3
    };
    yaml_parser_t parser;
    yaml_event_t event;
    yaml_parser_initialize(&parser);
    yaml_parser_set_input(&parser, byte_reader, &data);

    int scalar_count = 0;
    while (1) {
        if (!yaml_parser_parse(&parser, &event)) break;
        if (event.type == YAML_SCALAR_EVENT) scalar_count++;
        int done = (event.type == YAML_STREAM_END_EVENT);
        yaml_event_delete(&event);
        if (done) break;
    }
    yaml_parser_delete(&parser);
    ASSERT_EQ_INT(scalar_count, 3, "small chunks: 3 scalars");
}

/* ========== Line ending normalization ========== */

static void test_r_crlf_normalized(void) {
    yaml_parser_t parser;
    yaml_event_t event;
    int found = 0;

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser,
        (const unsigned char *)"a: b\r\nc: d\r\n", 12);
    while (1) {
        if (!yaml_parser_parse(&parser, &event)) break;
        if (event.type == YAML_SCALAR_EVENT &&
            strcmp((const char *)event.data.scalar.value, "d") == 0)
            found = 1;
        int done = (event.type == YAML_STREAM_END_EVENT);
        yaml_event_delete(&event);
        if (done) break;
    }
    yaml_parser_delete(&parser);
    ASSERT(found, "CRLF normalized");
}

static void test_r_cr_only_normalized(void) {
    yaml_parser_t parser;
    yaml_event_t event;
    int found = 0;

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser,
        (const unsigned char *)"a: b\rc: d\r", 10);
    while (1) {
        if (!yaml_parser_parse(&parser, &event)) break;
        if (event.type == YAML_SCALAR_EVENT &&
            strcmp((const char *)event.data.scalar.value, "d") == 0)
            found = 1;
        int done = (event.type == YAML_STREAM_END_EVENT);
        yaml_event_delete(&event);
        if (done) break;
    }
    yaml_parser_delete(&parser);
    ASSERT(found, "CR normalized");
}

static void test_r_mixed_line_endings(void) {
    yaml_parser_t parser;
    yaml_event_t event;
    int scalar_count = 0;

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser,
        (const unsigned char *)"- a\n- b\r\n- c\r", 13);
    while (1) {
        if (!yaml_parser_parse(&parser, &event)) break;
        if (event.type == YAML_SCALAR_EVENT) scalar_count++;
        int done = (event.type == YAML_STREAM_END_EVENT);
        yaml_event_delete(&event);
        if (done) break;
    }
    yaml_parser_delete(&parser);
    ASSERT_EQ_INT(scalar_count, 3, "mixed endings: 3 scalars");
}

/* ========== NEL and LS/PS ========== */

static void test_r_nel_line_break(void) {
    /* NEL (Next Line): U+0085 = 0xC2 0x85 */
    const unsigned char input[] = {'a', ':', ' ', 'b', 0xC2, 0x85, 'c', ':', ' ', 'd'};
    ASSERT(parse_completes(input, sizeof(input)), "NEL line break");
}

static void test_r_ls_line_break(void) {
    /* LS (Line Separator): U+2028 = 0xE2 0x80 0xA8 */
    const unsigned char input[] = {'a', ':', ' ', 'b', 0xE2, 0x80, 0xA8, 'c', ':', ' ', 'd'};
    ASSERT(parse_completes(input, sizeof(input)), "LS line break");
}

static void test_r_ps_line_break(void) {
    /* PS (Paragraph Separator): U+2029 = 0xE2 0x80 0xA9 */
    const unsigned char input[] = {'a', ':', ' ', 'b', 0xE2, 0x80, 0xA9, 'c', ':', ' ', 'd'};
    ASSERT(parse_completes(input, sizeof(input)), "PS line break");
}

int main(void) {
    TEST_SUITE_BEGIN("Reader Comprehensive");

    /* Encoding detection */
    test_r_detect_utf8_no_bom();
    test_r_detect_utf8_bom();
    test_r_detect_utf16le_bom();
    test_r_detect_utf16be_bom();
    test_r_explicit_utf8();

    /* UTF-8 validation */
    test_r_ascii_printable();
    test_r_ascii_digits();
    test_r_ascii_symbols();
    test_r_utf8_2byte();
    test_r_utf8_3byte();
    test_r_utf8_4byte();
    test_r_utf8_mixed();
    test_r_utf8_japanese();
    test_r_utf8_chinese();
    test_r_utf8_emoji();

    /* Invalid UTF-8 */
    test_r_invalid_continuation();
    test_r_invalid_overlong_2byte();
    test_r_invalid_truncated_2byte();
    test_r_invalid_truncated_3byte();
    test_r_invalid_fe();

    /* Control characters */
    test_r_tab_in_content();
    test_r_newline_lf();
    test_r_newline_cr();
    test_r_newline_crlf();
    test_r_nul_rejected();
    test_r_control_char_rejected();
    test_r_control_char_bel();
    test_r_control_char_bs();
    test_r_control_char_vt();
    test_r_control_char_ff();
    test_r_control_char_esc();
    test_r_del_char();

    /* BOM handling */
    test_r_utf8_bom_stripped();
    test_r_bom_only();

    /* Empty/edge-case inputs */
    test_r_empty_input();
    test_r_single_newline();
    test_r_single_space();
    test_r_many_newlines();
    test_r_many_spaces();
    test_r_large_input();

    /* File input */
    test_r_file_small();
    test_r_file_empty();

    /* Custom reader */
    test_r_reader_byte_at_a_time();
    test_r_reader_small_chunks();

    /* Line ending normalization */
    test_r_crlf_normalized();
    test_r_cr_only_normalized();
    test_r_mixed_line_endings();

    /* NEL and LS/PS */
    test_r_nel_line_break();
    test_r_ls_line_break();
    test_r_ps_line_break();

    TEST_SUITE_END();
}
