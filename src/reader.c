
#include "yaml_private.h"

/*
 * SSE2-accelerated ASCII scanning for the reader's fast path.
 * Scans for printable ASCII bytes (0x20-0x7E) which are always valid in YAML.
 * Stops at whitespace (tab/LF/CR), control chars, and high bytes, letting the
 * scalar fallback handle those (they're valid but uncommon in the middle of
 * content).
 */

#if defined(__SSE2__) && defined(__x86_64__)
#include <immintrin.h>

static inline size_t
yaml_reader_scan_ascii_sse2(const unsigned char *src, size_t len)
{
    const unsigned char *p = src;
    const __m128i v_lo = _mm_set1_epi8(0x1F);  /* below printable */
    const __m128i v_del = _mm_set1_epi8(0x7F);  /* DEL character */

    while (len >= 16) {
        __m128i data = _mm_loadu_si128((const __m128i *)p);
        /* ch <= 0x1F: unsigned compare via max(ch, 0x1F) == 0x1F */
        __m128i le_ctrl = _mm_cmpeq_epi8(_mm_max_epu8(data, v_lo), v_lo);
        /* ch == 0x7F */
        __m128i eq_del = _mm_cmpeq_epi8(data, v_del);
        /* ch >= 0x80: sign bit set */
        int hi_mask = _mm_movemask_epi8(data);
        int stop_mask = _mm_movemask_epi8(_mm_or_si128(le_ctrl, eq_del));
        stop_mask |= hi_mask;
        if (stop_mask) {
            return (size_t)(p - src) + (size_t)__builtin_ctz((unsigned)stop_mask);
        }
        p += 16;
        len -= 16;
    }
    /* Scalar tail: same printable ASCII check */
    while (len > 0) {
        unsigned char ch = *p;
        if (ch < 0x20 || ch >= 0x7F) break;
        p++;
        len--;
    }
    return (size_t)(p - src);
}

#define HAVE_READER_SIMD 1
#endif /* __SSE2__ && __x86_64__ */

/*
 * Check if a byte buffer is entirely composed of YAML-safe ASCII:
 * printable (0x20-0x7E), tab (0x09), LF (0x0A), CR (0x0D).
 * Returns 1 if all bytes are safe, 0 otherwise.
 */
int
yaml_input_is_ascii(const unsigned char *data, size_t size)
{
    const unsigned char *p = data;
    size_t remaining = size;

#if defined(__SSE2__) && defined(__x86_64__)
    /* SIMD: check 16 bytes at a time.
     * A byte is valid if: (0x20 <= b <= 0x7E) || b == 0x09 || b == 0x0A || b == 0x0D.
     * Equivalently, invalid if: b < 0x09, b == 0x0B, b == 0x0C, b in [0x0E..0x1F], b == 0x7F, b >= 0x80. */
    {
        const __m128i v_tab = _mm_set1_epi8(0x09);
        const __m128i v_lf  = _mm_set1_epi8(0x0A);
        const __m128i v_cr  = _mm_set1_epi8(0x0D);
        const __m128i v_lo  = _mm_set1_epi8(0x1F);
        const __m128i v_del = _mm_set1_epi8(0x7F);

        while (remaining >= 16) {
            __m128i data16 = _mm_loadu_si128((const __m128i *)p);
            /* is_printable: 0x20..0x7E (not control, not DEL, not high) */
            __m128i le_ctrl = _mm_cmpeq_epi8(_mm_max_epu8(data16, v_lo), v_lo);
            __m128i eq_del = _mm_cmpeq_epi8(data16, v_del);
            int hi_mask = _mm_movemask_epi8(data16);
            int bad_mask = _mm_movemask_epi8(_mm_or_si128(le_ctrl, eq_del)) | hi_mask;

            if (bad_mask) {
                /* Some bytes are not printable. Check if they're tab/LF/CR. */
                __m128i is_tab = _mm_cmpeq_epi8(data16, v_tab);
                __m128i is_lf  = _mm_cmpeq_epi8(data16, v_lf);
                __m128i is_cr  = _mm_cmpeq_epi8(data16, v_cr);
                int ok_mask = _mm_movemask_epi8(_mm_or_si128(is_tab,
                        _mm_or_si128(is_lf, is_cr)));
                /* Any bad byte that's not tab/LF/CR is truly invalid */
                if (bad_mask & ~ok_mask)
                    return 0;
            }
            p += 16;
            remaining -= 16;
        }
    }
#endif

    while (remaining > 0) {
        unsigned char c = *p;
        if (c >= 0x20 && c <= 0x7E) { p++; remaining--; continue; }
        if (c == 0x09 || c == 0x0A || c == 0x0D) { p++; remaining--; continue; }
        return 0;
    }
    return 1;
}

/*
 * Declarations.
 */

static int
yaml_parser_set_reader_error(yaml_parser_t *parser, const char *problem,
        size_t offset, int value);

static int
yaml_parser_update_raw_buffer(yaml_parser_t *parser);

static int
yaml_parser_determine_encoding(yaml_parser_t *parser);

YAML_DECLARE(int)
yaml_parser_update_buffer(yaml_parser_t *parser, size_t length);

/*
 * Set the reader error and return 0.
 */

static int
yaml_parser_set_reader_error(yaml_parser_t *parser, const char *problem,
        size_t offset, int value)
{
    parser->error = YAML_READER_ERROR;
    parser->problem = problem;
    parser->problem_offset = offset;
    parser->problem_value = value;

    return 0;
}

/*
 * Byte order marks.
 */

#define BOM_UTF8    "\xef\xbb\xbf"
#define BOM_UTF16LE "\xff\xfe"
#define BOM_UTF16BE "\xfe\xff"

/*
 * Determine the input stream encoding by checking the BOM symbol. If no BOM is
 * found, the UTF-8 encoding is assumed. Return 1 on success, 0 on failure.
 */

static int
yaml_parser_determine_encoding(yaml_parser_t *parser)
{
    /* Ensure that we had enough bytes in the raw buffer. */

    while (!parser->eof
            && parser->raw_buffer.last - parser->raw_buffer.pointer < 3) {
        if (!yaml_parser_update_raw_buffer(parser)) {
            return 0;
        }
    }

    /* Determine the encoding. */

    if (parser->raw_buffer.last - parser->raw_buffer.pointer >= 2
            && !memcmp(parser->raw_buffer.pointer, BOM_UTF16LE, 2)) {
        parser->encoding = YAML_UTF16LE_ENCODING;
        parser->raw_buffer.pointer += 2;
        parser->offset += 2;
    }
    else if (parser->raw_buffer.last - parser->raw_buffer.pointer >= 2
            && !memcmp(parser->raw_buffer.pointer, BOM_UTF16BE, 2)) {
        parser->encoding = YAML_UTF16BE_ENCODING;
        parser->raw_buffer.pointer += 2;
        parser->offset += 2;
    }
    else if (parser->raw_buffer.last - parser->raw_buffer.pointer >= 3
            && !memcmp(parser->raw_buffer.pointer, BOM_UTF8, 3)) {
        parser->encoding = YAML_UTF8_ENCODING;
        parser->raw_buffer.pointer += 3;
        parser->offset += 3;
    }
    else {
        parser->encoding = YAML_UTF8_ENCODING;
    }

    return 1;
}

/*
 * Update the raw buffer.
 */

static int
yaml_parser_update_raw_buffer(yaml_parser_t *parser)
{
    size_t size_read = 0;

    /* Return if the raw buffer is full. */

    if (parser->raw_buffer.start == parser->raw_buffer.pointer
            && parser->raw_buffer.last == parser->raw_buffer.end)
        return 1;

    /* Return on EOF. */

    if (parser->eof) return 1;

    /* Move the remaining bytes in the raw buffer to the beginning. */

    if (parser->raw_buffer.start < parser->raw_buffer.pointer
            && parser->raw_buffer.pointer < parser->raw_buffer.last) {
        memmove(parser->raw_buffer.start, parser->raw_buffer.pointer,
                parser->raw_buffer.last - parser->raw_buffer.pointer);
    }
    parser->raw_buffer.last -=
        parser->raw_buffer.pointer - parser->raw_buffer.start;
    parser->raw_buffer.pointer = parser->raw_buffer.start;

    /* Call the read handler to fill the buffer. */

    if (!parser->read_handler(parser->read_handler_data, parser->raw_buffer.last,
                parser->raw_buffer.end - parser->raw_buffer.last, &size_read)) {
        return yaml_parser_set_reader_error(parser, "input error",
                parser->offset, -1);
    }
    parser->raw_buffer.last += size_read;
    if (!size_read) {
        parser->eof = 1;
    }

    return 1;
}

/*
 * Ensure that the buffer contains at least `length` characters.
 * Return 1 on success, 0 on failure.
 *
 * The length is supposed to be significantly less that the buffer size.
 */

YAML_DECLARE(int)
yaml_parser_update_buffer(yaml_parser_t *parser, size_t length)
{
    int first = 1;

    assert(parser->read_handler);   /* Read handler must be set. */

    /* Zero-copy fast path for ASCII string input.  On the very first call
     * to update_buffer, check if the entire input is YAML-safe ASCII.
     * If so, point the scanner's buffer directly into the input string,
     * eliminating all buffer copy and memmove overhead. */

    if (parser->buffer_alloc) {
        /* Already in zero-copy mode.  The only reason we're called is
         * EOF (unread < length).  Transition back to the allocated buffer. */
        if (parser->unread >= length)
            return 1;
        {
            yaml_char_t *alloc = parser->buffer_alloc;
            size_t remaining = parser->unread;
            if (remaining > 0)
                memcpy(alloc, parser->buffer.pointer, remaining);
            alloc[remaining] = '\0';
            parser->buffer.start = alloc;
            parser->buffer.pointer = alloc;
            parser->buffer.last = alloc + remaining + 1;
            parser->buffer.end = alloc + INPUT_BUFFER_SIZE;
            parser->unread = remaining + 1;
            parser->buffer_alloc = NULL;
            parser->eof = 1;
            return 1;
        }
    }

    if (!parser->encoding
            && parser->read_handler == yaml_string_read_handler
            && parser->raw_buffer.pointer == parser->raw_buffer.last)
    {
        /* First call: try to activate zero-copy mode. */
        const unsigned char *input = parser->input.string.start;
        size_t size = (size_t)(parser->input.string.end - parser->input.string.start);
        if (size > 0 && yaml_input_is_ascii(input, size)) {
            parser->buffer_alloc = parser->buffer.start;
            parser->buffer.start = (yaml_char_t *)input;
            parser->buffer.pointer = (yaml_char_t *)input;
            parser->buffer.last = (yaml_char_t *)(input + size);
            parser->buffer.end = (yaml_char_t *)(input + size);
            parser->unread = size;
            parser->offset = size;
            parser->encoding = YAML_UTF8_ENCODING;
            parser->input.string.current = parser->input.string.end;
            return 1;
        }
    }

    /* If the EOF flag is set and the raw buffer is empty, do nothing. */

    if (parser->eof && parser->raw_buffer.pointer == parser->raw_buffer.last)
        return 1;

    /* Return if the buffer contains enough characters. */

    if (parser->unread >= length)
        return 1;

    /* Determine the input encoding if it is not known yet. */

    if (!parser->encoding) {
        if (!yaml_parser_determine_encoding(parser))
            return 0;
    }

    /* Move the unread characters to the beginning of the buffer. */

    if (parser->buffer.start < parser->buffer.pointer
            && parser->buffer.pointer < parser->buffer.last) {
        size_t size = parser->buffer.last - parser->buffer.pointer;
        memmove(parser->buffer.start, parser->buffer.pointer, size);
        parser->buffer.pointer = parser->buffer.start;
        parser->buffer.last = parser->buffer.start + size;
    }
    else if (parser->buffer.pointer == parser->buffer.last) {
        parser->buffer.pointer = parser->buffer.start;
        parser->buffer.last = parser->buffer.start;
    }

    /*
     * Fast path for string input with UTF-8 encoding: copy ASCII content
     * directly from the input string to the decoded buffer, bypassing
     * the raw buffer entirely.  This eliminates one full memcpy and all
     * raw buffer management overhead for the common case.
     */

    if (parser->encoding == YAML_UTF8_ENCODING
            && parser->read_handler == yaml_string_read_handler
            && parser->raw_buffer.pointer == parser->raw_buffer.last)
    {
        while (parser->unread < length)
        {
            const unsigned char *src = parser->input.string.current;
            const unsigned char *src_end = parser->input.string.end;

            if (src >= src_end) {
                /* EOF: put NUL sentinel and return */
                if (!parser->eof) {
                    parser->eof = 1;
                }
                *(parser->buffer.last++) = '\0';
                parser->unread ++;
                return 1;
            }

            /* Scan for valid ASCII run directly in the input string.
             * Uses SIMD when available for printable ASCII (0x20-0x7E),
             * then scalar fallback picks up whitespace chars (tab/LF/CR). */
            {
                const unsigned char *run_start = src;
                size_t buf_avail = (size_t)(parser->buffer.end
                                    - parser->buffer.last);
                size_t src_avail = (size_t)(src_end - src);
                size_t avail = src_avail < buf_avail ? src_avail : buf_avail;

#ifdef HAVE_READER_SIMD
                /* SIMD: scan printable ASCII in bulk */
                {
                    size_t n = yaml_reader_scan_ascii_sse2(src, avail);
                    src += n;
                }
                /* Scalar tail: pick up whitespace chars that SIMD skipped */
#endif
                {
                    const unsigned char *scan_end = run_start + avail;
                    while (src < scan_end) {
                        unsigned char c = *src;
                        if (c >= 0x20) {
                            if (c <= 0x7E) { src++; continue; }
                            break; /* c == 0x7F or c >= 0x80 */
                        }
                        if (c == 0x0A || c == 0x0D || c == 0x09) {
                            src++; continue;
                        }
                        break; /* control character or non-ASCII */
                    }
                }

                if (src > run_start) {
                    size_t n = (size_t)(src - run_start);
                    memcpy(parser->buffer.last, run_start, n);
                    parser->buffer.last += n;
                    parser->offset += n;
                    parser->unread += n;
                    parser->input.string.current = src;
                    continue;
                }
            }

            /* Non-ASCII or control character: fall back to normal path */
            break;
        }

        /* If we got enough characters, return success */
        if (parser->unread >= length)
            return 1;

        /* Otherwise fall through to the normal raw-buffer path for
         * non-ASCII content. First, ensure the raw buffer has data
         * by loading from the input string via the normal handler. */
    }

    /* Fill the buffer until it has enough characters. */

    while (parser->unread < length)
    {
        /* Fill the raw buffer if necessary. */

        if (!first || parser->raw_buffer.pointer == parser->raw_buffer.last) {
            if (!yaml_parser_update_raw_buffer(parser)) return 0;
        }
        first = 0;

        /* Decode the raw buffer. */

        while (parser->raw_buffer.pointer != parser->raw_buffer.last)
        {
            unsigned int value = 0, value2 = 0;
            int incomplete = 0;
            unsigned char octet;
            unsigned int width = 0;
            int low, high;
            size_t k;
            size_t raw_unread = parser->raw_buffer.last - parser->raw_buffer.pointer;

            /* Decode the next character. */

            switch (parser->encoding)
            {
                case YAML_UTF8_ENCODING:

                    /*
                     * Decode a UTF-8 character.  Check RFC 3629
                     * (http://www.ietf.org/rfc/rfc3629.txt) for more details.
                     *
                     * The following table (taken from the RFC) is used for
                     * decoding.
                     *
                     *    Char. number range |        UTF-8 octet sequence
                     *      (hexadecimal)    |              (binary)
                     *   --------------------+------------------------------------
                     *   0000 0000-0000 007F | 0xxxxxxx
                     *   0000 0080-0000 07FF | 110xxxxx 10xxxxxx
                     *   0000 0800-0000 FFFF | 1110xxxx 10xxxxxx 10xxxxxx
                     *   0001 0000-0010 FFFF | 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
                     *
                     * Additionally, the characters in the range 0xD800-0xDFFF
                     * are prohibited as they are reserved for use with UTF-16
                     * surrogate pairs.
                     */

                    /* Determine the length of the UTF-8 sequence. */

                    octet = parser->raw_buffer.pointer[0];

                    /*
                     * Fast path: batch-process ASCII bytes (0x09, 0x0A,
                     * 0x0D, 0x20-0x7E) directly without full decode.
                     * For UTF-8, ASCII bytes are single-byte and pass
                     * through unchanged.
                     *
                     * First scan to find the end of the valid ASCII run,
                     * then memcpy the whole run at once.
                     */
                    if ((octet & 0x80) == 0x00) {
                        unsigned char *rp = parser->raw_buffer.pointer;
                        unsigned char *rl = parser->raw_buffer.last;
                        unsigned char *run_start = rp;
                        /* Limit scan to available decoded buffer space. */
                        size_t buf_avail = (size_t)(parser->buffer.end
                                            - parser->buffer.last);
                        if ((size_t)(rl - rp) > buf_avail) {
                            rl = rp + buf_avail;
                        }

#ifdef HAVE_READER_SIMD
                        /* SIMD: bulk scan printable ASCII (0x20-0x7E) */
                        {
                            size_t avail = (size_t)(rl - rp);
                            size_t n = yaml_reader_scan_ascii_sse2(rp, avail);
                            rp += n;
                        }
                        /* Scalar tail: pick up whitespace (tab/LF/CR) */
#endif
                        while (rp < rl) {
                            unsigned char c = *rp;
                            if (c >= 0x20) {
                                if (c <= 0x7E) {
                                    rp++;
                                    continue;
                                }
                                break; /* c == 0x7F or c >= 0x80 */
                            }
                            if (c == 0x0A || c == 0x0D || c == 0x09) {
                                rp++;
                                continue;
                            }
                            break; /* invalid control character */
                        }

                        if (rp > run_start) {
                            size_t n = (size_t)(rp - run_start);
                            memcpy(parser->buffer.last, run_start, n);
                            parser->buffer.last += n;
                            parser->offset += n;
                            parser->unread += n;
                            parser->raw_buffer.pointer = rp;
                            continue;
                        }

                        /* Single ASCII byte that's a control char */
                        value = octet;
                        width = 1;
                        break;
                    }

                    width = (octet & 0xE0) == 0xC0 ? 2 :
                            (octet & 0xF0) == 0xE0 ? 3 :
                            (octet & 0xF8) == 0xF0 ? 4 : 0;

                    /* Check if the leading octet is valid. */

                    if (!width)
                        return yaml_parser_set_reader_error(parser,
                                "invalid leading UTF-8 octet",
                                parser->offset, octet);

                    /* Check if the raw buffer contains an incomplete character. */

                    if (width > raw_unread) {
                        if (parser->eof) {
                            return yaml_parser_set_reader_error(parser,
                                    "incomplete UTF-8 octet sequence",
                                    parser->offset, -1);
                        }
                        incomplete = 1;
                        break;
                    }

                    /* Decode the leading octet. */

                    value = (octet & 0xE0) == 0xC0 ? octet & 0x1F :
                            (octet & 0xF0) == 0xE0 ? octet & 0x0F :
                            (octet & 0xF8) == 0xF0 ? octet & 0x07 : 0;

                    /* Check and decode the trailing octets. */

                    for (k = 1; k < width; k ++)
                    {
                        octet = parser->raw_buffer.pointer[k];

                        /* Check if the octet is valid. */

                        if ((octet & 0xC0) != 0x80)
                            return yaml_parser_set_reader_error(parser,
                                    "invalid trailing UTF-8 octet",
                                    parser->offset+k, octet);

                        /* Decode the octet. */

                        value = (value << 6) + (octet & 0x3F);
                    }

                    /* Check the length of the sequence against the value. */

                    if (!((width == 1) ||
                            (width == 2 && value >= 0x80) ||
                            (width == 3 && value >= 0x800) ||
                            (width == 4 && value >= 0x10000)))
                        return yaml_parser_set_reader_error(parser,
                                "invalid length of a UTF-8 sequence",
                                parser->offset, -1);

                    /* Check the range of the value. */

                    if ((value >= 0xD800 && value <= 0xDFFF) || value > 0x10FFFF)
                        return yaml_parser_set_reader_error(parser,
                                "invalid Unicode character",
                                parser->offset, value);

                    break;

                case YAML_UTF16LE_ENCODING:
                case YAML_UTF16BE_ENCODING:

                    low = (parser->encoding == YAML_UTF16LE_ENCODING ? 0 : 1);
                    high = (parser->encoding == YAML_UTF16LE_ENCODING ? 1 : 0);

                    /*
                     * The UTF-16 encoding is not as simple as one might
                     * naively think.  Check RFC 2781
                     * (http://www.ietf.org/rfc/rfc2781.txt).
                     *
                     * Normally, two subsequent bytes describe a Unicode
                     * character.  However a special technique (called a
                     * surrogate pair) is used for specifying character
                     * values larger than 0xFFFF.
                     *
                     * A surrogate pair consists of two pseudo-characters:
                     *      high surrogate area (0xD800-0xDBFF)
                     *      low surrogate area (0xDC00-0xDFFF)
                     *
                     * The following formulas are used for decoding
                     * and encoding characters using surrogate pairs:
                     *
                     *  U  = U' + 0x10000   (0x01 00 00 <= U <= 0x10 FF FF)
                     *  U' = yyyyyyyyyyxxxxxxxxxx   (0 <= U' <= 0x0F FF FF)
                     *  W1 = 110110yyyyyyyyyy
                     *  W2 = 110111xxxxxxxxxx
                     *
                     * where U is the character value, W1 is the high surrogate
                     * area, W2 is the low surrogate area.
                     */

                    /* Check for incomplete UTF-16 character. */

                    if (raw_unread < 2) {
                        if (parser->eof) {
                            return yaml_parser_set_reader_error(parser,
                                    "incomplete UTF-16 character",
                                    parser->offset, -1);
                        }
                        incomplete = 1;
                        break;
                    }

                    /* Get the character. */

                    value = parser->raw_buffer.pointer[low]
                        + (parser->raw_buffer.pointer[high] << 8);

                    /* Check for unexpected low surrogate area. */

                    if ((value & 0xFC00) == 0xDC00)
                        return yaml_parser_set_reader_error(parser,
                                "unexpected low surrogate area",
                                parser->offset, value);

                    /* Check for a high surrogate area. */

                    if ((value & 0xFC00) == 0xD800) {

                        width = 4;

                        /* Check for incomplete surrogate pair. */

                        if (raw_unread < 4) {
                            if (parser->eof) {
                                return yaml_parser_set_reader_error(parser,
                                        "incomplete UTF-16 surrogate pair",
                                        parser->offset, -1);
                            }
                            incomplete = 1;
                            break;
                        }

                        /* Get the next character. */

                        value2 = parser->raw_buffer.pointer[low+2]
                            + (parser->raw_buffer.pointer[high+2] << 8);

                        /* Check for a low surrogate area. */

                        if ((value2 & 0xFC00) != 0xDC00)
                            return yaml_parser_set_reader_error(parser,
                                    "expected low surrogate area",
                                    parser->offset+2, value2);

                        /* Generate the value of the surrogate pair. */

                        value = 0x10000 + ((value & 0x3FF) << 10) + (value2 & 0x3FF);
                    }

                    else {
                        width = 2;
                    }

                    break;

                default:
                    assert(1);      /* Impossible. */
            }

            /* Check if the raw buffer contains enough bytes to form a character. */

            if (incomplete) break;

            /*
             * Check if the character is in the allowed range:
             *      #x9 | #xA | #xD | [#x20-#x7E]               (8 bit)
             *      | #x85 | [#xA0-#xD7FF] | [#xE000-#xFFFD]    (16 bit)
             *      | [#x10000-#x10FFFF]                        (32 bit)
             */

            if (! (value == 0x09 || value == 0x0A || value == 0x0D
                        || (value >= 0x20 && value <= 0x7E)
                        || (value == 0x85) || (value >= 0xA0 && value <= 0xD7FF)
                        || (value >= 0xE000 && value <= 0xFFFD)
                        || (value >= 0x10000 && value <= 0x10FFFF)))
                return yaml_parser_set_reader_error(parser,
                        "control characters are not allowed",
                        parser->offset, value);

            /* Move the raw pointers. */

            parser->raw_buffer.pointer += width;
            parser->offset += width;

            /* Finally put the character into the buffer. */

            /* 0000 0000-0000 007F -> 0xxxxxxx */
            if (value <= 0x7F) {
                *(parser->buffer.last++) = value;
            }
            /* 0000 0080-0000 07FF -> 110xxxxx 10xxxxxx */
            else if (value <= 0x7FF) {
                *(parser->buffer.last++) = 0xC0 + (value >> 6);
                *(parser->buffer.last++) = 0x80 + (value & 0x3F);
            }
            /* 0000 0800-0000 FFFF -> 1110xxxx 10xxxxxx 10xxxxxx */
            else if (value <= 0xFFFF) {
                *(parser->buffer.last++) = 0xE0 + (value >> 12);
                *(parser->buffer.last++) = 0x80 + ((value >> 6) & 0x3F);
                *(parser->buffer.last++) = 0x80 + (value & 0x3F);
            }
            /* 0001 0000-0010 FFFF -> 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx */
            else {
                *(parser->buffer.last++) = 0xF0 + (value >> 18);
                *(parser->buffer.last++) = 0x80 + ((value >> 12) & 0x3F);
                *(parser->buffer.last++) = 0x80 + ((value >> 6) & 0x3F);
                *(parser->buffer.last++) = 0x80 + (value & 0x3F);
            }

            parser->unread ++;
        }

        /* On EOF, put NUL into the buffer and return. */

        if (parser->eof) {
            *(parser->buffer.last++) = '\0';
            parser->unread ++;
            return 1;
        }

    }

    if (parser->offset >= MAX_FILE_SIZE) {
        return yaml_parser_set_reader_error(parser, "input is too long",
            parser->offset, -1);
    }

    return 1;
}
