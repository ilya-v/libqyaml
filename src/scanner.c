
/*
 * Introduction
 * ************
 *
 * The following notes assume that you are familiar with the YAML specification
 * (http://yaml.org/spec/cvs/current.html).  We mostly follow it, although in
 * some cases we are less restrictive that it requires.
 *
 * The process of transforming a YAML stream into a sequence of events is
 * divided on two steps: Scanning and Parsing.
 *
 * The Scanner transforms the input stream into a sequence of tokens, while the
 * parser transform the sequence of tokens produced by the Scanner into a
 * sequence of parsing events.
 *
 * The Scanner is rather clever and complicated. The Parser, on the contrary,
 * is a straightforward implementation of a recursive-descendant parser (or,
 * LL(1) parser, as it is usually called).
 *
 * Actually there are two issues of Scanning that might be called "clever", the
 * rest is quite straightforward.  The issues are "block collection start" and
 * "simple keys".  Both issues are explained below in details.
 *
 * Here the Scanning step is explained and implemented.  We start with the list
 * of all the tokens produced by the Scanner together with short descriptions.
 *
 * Now, tokens:
 *
 *      STREAM-START(encoding)          # The stream start.
 *      STREAM-END                      # The stream end.
 *      VERSION-DIRECTIVE(major,minor)  # The '%YAML' directive.
 *      TAG-DIRECTIVE(handle,prefix)    # The '%TAG' directive.
 *      DOCUMENT-START                  # '---'
 *      DOCUMENT-END                    # '...'
 *      BLOCK-SEQUENCE-START            # Indentation increase denoting a block
 *      BLOCK-MAPPING-START             # sequence or a block mapping.
 *      BLOCK-END                       # Indentation decrease.
 *      FLOW-SEQUENCE-START             # '['
 *      FLOW-SEQUENCE-END               # ']'
 *      FLOW-MAPPING-START              # '{'
 *      FLOW-MAPPING-END                # '}'
 *      BLOCK-ENTRY                     # '-'
 *      FLOW-ENTRY                      # ','
 *      KEY                             # '?' or nothing (simple keys).
 *      VALUE                           # ':'
 *      ALIAS(anchor)                   # '*anchor'
 *      ANCHOR(anchor)                  # '&anchor'
 *      TAG(handle,suffix)              # '!handle!suffix'
 *      SCALAR(value,style)             # A scalar.
 *
 * The following two tokens are "virtual" tokens denoting the beginning and the
 * end of the stream:
 *
 *      STREAM-START(encoding)
 *      STREAM-END
 *
 * We pass the information about the input stream encoding with the
 * STREAM-START token.
 *
 * The next two tokens are responsible for tags:
 *
 *      VERSION-DIRECTIVE(major,minor)
 *      TAG-DIRECTIVE(handle,prefix)
 *
 * Example:
 *
 *      %YAML   1.1
 *      %TAG    !   !foo
 *      %TAG    !yaml!  tag:yaml.org,2002:
 *      ---
 *
 * The corresponding sequence of tokens:
 *
 *      STREAM-START(utf-8)
 *      VERSION-DIRECTIVE(1,1)
 *      TAG-DIRECTIVE("!","!foo")
 *      TAG-DIRECTIVE("!yaml","tag:yaml.org,2002:")
 *      DOCUMENT-START
 *      STREAM-END
 *
 * Note that the VERSION-DIRECTIVE and TAG-DIRECTIVE tokens occupy a whole
 * line.
 *
 * The document start and end indicators are represented by:
 *
 *      DOCUMENT-START
 *      DOCUMENT-END
 *
 * Note that if a YAML stream contains an implicit document (without '---'
 * and '...' indicators), no DOCUMENT-START and DOCUMENT-END tokens will be
 * produced.
 *
 * In the following examples, we present whole documents together with the
 * produced tokens.
 *
 *      1. An implicit document:
 *
 *          'a scalar'
 *
 *      Tokens:
 *
 *          STREAM-START(utf-8)
 *          SCALAR("a scalar",single-quoted)
 *          STREAM-END
 *
 *      2. An explicit document:
 *
 *          ---
 *          'a scalar'
 *          ...
 *
 *      Tokens:
 *
 *          STREAM-START(utf-8)
 *          DOCUMENT-START
 *          SCALAR("a scalar",single-quoted)
 *          DOCUMENT-END
 *          STREAM-END
 *
 *      3. Several documents in a stream:
 *
 *          'a scalar'
 *          ---
 *          'another scalar'
 *          ---
 *          'yet another scalar'
 *
 *      Tokens:
 *
 *          STREAM-START(utf-8)
 *          SCALAR("a scalar",single-quoted)
 *          DOCUMENT-START
 *          SCALAR("another scalar",single-quoted)
 *          DOCUMENT-START
 *          SCALAR("yet another scalar",single-quoted)
 *          STREAM-END
 *
 * We have already introduced the SCALAR token above.  The following tokens are
 * used to describe aliases, anchors, tag, and scalars:
 *
 *      ALIAS(anchor)
 *      ANCHOR(anchor)
 *      TAG(handle,suffix)
 *      SCALAR(value,style)
 *
 * The following series of examples illustrate the usage of these tokens:
 *
 *      1. A recursive sequence:
 *
 *          &A [ *A ]
 *
 *      Tokens:
 *
 *          STREAM-START(utf-8)
 *          ANCHOR("A")
 *          FLOW-SEQUENCE-START
 *          ALIAS("A")
 *          FLOW-SEQUENCE-END
 *          STREAM-END
 *
 *      2. A tagged scalar:
 *
 *          !!float "3.14"  # A good approximation.
 *
 *      Tokens:
 *
 *          STREAM-START(utf-8)
 *          TAG("!!","float")
 *          SCALAR("3.14",double-quoted)
 *          STREAM-END
 *
 *      3. Various scalar styles:
 *
 *          --- # Implicit empty plain scalars do not produce tokens.
 *          --- a plain scalar
 *          --- 'a single-quoted scalar'
 *          --- "a double-quoted scalar"
 *          --- |-
 *            a literal scalar
 *          --- >-
 *            a folded
 *            scalar
 *
 *      Tokens:
 *
 *          STREAM-START(utf-8)
 *          DOCUMENT-START
 *          DOCUMENT-START
 *          SCALAR("a plain scalar",plain)
 *          DOCUMENT-START
 *          SCALAR("a single-quoted scalar",single-quoted)
 *          DOCUMENT-START
 *          SCALAR("a double-quoted scalar",double-quoted)
 *          DOCUMENT-START
 *          SCALAR("a literal scalar",literal)
 *          DOCUMENT-START
 *          SCALAR("a folded scalar",folded)
 *          STREAM-END
 *
 * Now it's time to review collection-related tokens. We will start with
 * flow collections:
 *
 *      FLOW-SEQUENCE-START
 *      FLOW-SEQUENCE-END
 *      FLOW-MAPPING-START
 *      FLOW-MAPPING-END
 *      FLOW-ENTRY
 *      KEY
 *      VALUE
 *
 * The tokens FLOW-SEQUENCE-START, FLOW-SEQUENCE-END, FLOW-MAPPING-START, and
 * FLOW-MAPPING-END represent the indicators '[', ']', '{', and '}'
 * correspondingly.  FLOW-ENTRY represent the ',' indicator.  Finally the
 * indicators '?' and ':', which are used for denoting mapping keys and values,
 * are represented by the KEY and VALUE tokens.
 *
 * The following examples show flow collections:
 *
 *      1. A flow sequence:
 *
 *          [item 1, item 2, item 3]
 *
 *      Tokens:
 *
 *          STREAM-START(utf-8)
 *          FLOW-SEQUENCE-START
 *          SCALAR("item 1",plain)
 *          FLOW-ENTRY
 *          SCALAR("item 2",plain)
 *          FLOW-ENTRY
 *          SCALAR("item 3",plain)
 *          FLOW-SEQUENCE-END
 *          STREAM-END
 *
 *      2. A flow mapping:
 *
 *          {
 *              a simple key: a value,  # Note that the KEY token is produced.
 *              ? a complex key: another value,
 *          }
 *
 *      Tokens:
 *
 *          STREAM-START(utf-8)
 *          FLOW-MAPPING-START
 *          KEY
 *          SCALAR("a simple key",plain)
 *          VALUE
 *          SCALAR("a value",plain)
 *          FLOW-ENTRY
 *          KEY
 *          SCALAR("a complex key",plain)
 *          VALUE
 *          SCALAR("another value",plain)
 *          FLOW-ENTRY
 *          FLOW-MAPPING-END
 *          STREAM-END
 *
 * A simple key is a key which is not denoted by the '?' indicator.  Note that
 * the Scanner still produce the KEY token whenever it encounters a simple key.
 *
 * For scanning block collections, the following tokens are used (note that we
 * repeat KEY and VALUE here):
 *
 *      BLOCK-SEQUENCE-START
 *      BLOCK-MAPPING-START
 *      BLOCK-END
 *      BLOCK-ENTRY
 *      KEY
 *      VALUE
 *
 * The tokens BLOCK-SEQUENCE-START and BLOCK-MAPPING-START denote indentation
 * increase that precedes a block collection (cf. the INDENT token in Python).
 * The token BLOCK-END denote indentation decrease that ends a block collection
 * (cf. the DEDENT token in Python).  However YAML has some syntax peculiarities
 * that makes detections of these tokens more complex.
 *
 * The tokens BLOCK-ENTRY, KEY, and VALUE are used to represent the indicators
 * '-', '?', and ':' correspondingly.
 *
 * The following examples show how the tokens BLOCK-SEQUENCE-START,
 * BLOCK-MAPPING-START, and BLOCK-END are emitted by the Scanner:
 *
 *      1. Block sequences:
 *
 *          - item 1
 *          - item 2
 *          -
 *            - item 3.1
 *            - item 3.2
 *          -
 *            key 1: value 1
 *            key 2: value 2
 *
 *      Tokens:
 *
 *          STREAM-START(utf-8)
 *          BLOCK-SEQUENCE-START
 *          BLOCK-ENTRY
 *          SCALAR("item 1",plain)
 *          BLOCK-ENTRY
 *          SCALAR("item 2",plain)
 *          BLOCK-ENTRY
 *          BLOCK-SEQUENCE-START
 *          BLOCK-ENTRY
 *          SCALAR("item 3.1",plain)
 *          BLOCK-ENTRY
 *          SCALAR("item 3.2",plain)
 *          BLOCK-END
 *          BLOCK-ENTRY
 *          BLOCK-MAPPING-START
 *          KEY
 *          SCALAR("key 1",plain)
 *          VALUE
 *          SCALAR("value 1",plain)
 *          KEY
 *          SCALAR("key 2",plain)
 *          VALUE
 *          SCALAR("value 2",plain)
 *          BLOCK-END
 *          BLOCK-END
 *          STREAM-END
 *
 *      2. Block mappings:
 *
 *          a simple key: a value   # The KEY token is produced here.
 *          ? a complex key
 *          : another value
 *          a mapping:
 *            key 1: value 1
 *            key 2: value 2
 *          a sequence:
 *            - item 1
 *            - item 2
 *
 *      Tokens:
 *
 *          STREAM-START(utf-8)
 *          BLOCK-MAPPING-START
 *          KEY
 *          SCALAR("a simple key",plain)
 *          VALUE
 *          SCALAR("a value",plain)
 *          KEY
 *          SCALAR("a complex key",plain)
 *          VALUE
 *          SCALAR("another value",plain)
 *          KEY
 *          SCALAR("a mapping",plain)
 *          VALUE
 *          BLOCK-MAPPING-START
 *          KEY
 *          SCALAR("key 1",plain)
 *          VALUE
 *          SCALAR("value 1",plain)
 *          KEY
 *          SCALAR("key 2",plain)
 *          VALUE
 *          SCALAR("value 2",plain)
 *          BLOCK-END
 *          KEY
 *          SCALAR("a sequence",plain)
 *          VALUE
 *          BLOCK-SEQUENCE-START
 *          BLOCK-ENTRY
 *          SCALAR("item 1",plain)
 *          BLOCK-ENTRY
 *          SCALAR("item 2",plain)
 *          BLOCK-END
 *          BLOCK-END
 *          STREAM-END
 *
 * YAML does not always require to start a new block collection from a new
 * line.  If the current line contains only '-', '?', and ':' indicators, a new
 * block collection may start at the current line.  The following examples
 * illustrate this case:
 *
 *      1. Collections in a sequence:
 *
 *          - - item 1
 *            - item 2
 *          - key 1: value 1
 *            key 2: value 2
 *          - ? complex key
 *            : complex value
 *
 *      Tokens:
 *
 *          STREAM-START(utf-8)
 *          BLOCK-SEQUENCE-START
 *          BLOCK-ENTRY
 *          BLOCK-SEQUENCE-START
 *          BLOCK-ENTRY
 *          SCALAR("item 1",plain)
 *          BLOCK-ENTRY
 *          SCALAR("item 2",plain)
 *          BLOCK-END
 *          BLOCK-ENTRY
 *          BLOCK-MAPPING-START
 *          KEY
 *          SCALAR("key 1",plain)
 *          VALUE
 *          SCALAR("value 1",plain)
 *          KEY
 *          SCALAR("key 2",plain)
 *          VALUE
 *          SCALAR("value 2",plain)
 *          BLOCK-END
 *          BLOCK-ENTRY
 *          BLOCK-MAPPING-START
 *          KEY
 *          SCALAR("complex key")
 *          VALUE
 *          SCALAR("complex value")
 *          BLOCK-END
 *          BLOCK-END
 *          STREAM-END
 *
 *      2. Collections in a mapping:
 *
 *          ? a sequence
 *          : - item 1
 *            - item 2
 *          ? a mapping
 *          : key 1: value 1
 *            key 2: value 2
 *
 *      Tokens:
 *
 *          STREAM-START(utf-8)
 *          BLOCK-MAPPING-START
 *          KEY
 *          SCALAR("a sequence",plain)
 *          VALUE
 *          BLOCK-SEQUENCE-START
 *          BLOCK-ENTRY
 *          SCALAR("item 1",plain)
 *          BLOCK-ENTRY
 *          SCALAR("item 2",plain)
 *          BLOCK-END
 *          KEY
 *          SCALAR("a mapping",plain)
 *          VALUE
 *          BLOCK-MAPPING-START
 *          KEY
 *          SCALAR("key 1",plain)
 *          VALUE
 *          SCALAR("value 1",plain)
 *          KEY
 *          SCALAR("key 2",plain)
 *          VALUE
 *          SCALAR("value 2",plain)
 *          BLOCK-END
 *          BLOCK-END
 *          STREAM-END
 *
 * YAML also permits non-indented sequences if they are included into a block
 * mapping.  In this case, the token BLOCK-SEQUENCE-START is not produced:
 *
 *      key:
 *      - item 1    # BLOCK-SEQUENCE-START is NOT produced here.
 *      - item 2
 *
 * Tokens:
 *
 *      STREAM-START(utf-8)
 *      BLOCK-MAPPING-START
 *      KEY
 *      SCALAR("key",plain)
 *      VALUE
 *      BLOCK-ENTRY
 *      SCALAR("item 1",plain)
 *      BLOCK-ENTRY
 *      SCALAR("item 2",plain)
 *      BLOCK-END
 */

#include "yaml_private.h"

/*
 * SIMD scanning support for batch character classification.
 * Uses SSE2 to scan 16 bytes at a time for characters that terminate
 * plain scalars, flow scalars, block scalars, or whitespace runs.
 */

#if defined(__SSE2__) && defined(__x86_64__)
#include <immintrin.h>

/*
 * Scan up to `len` bytes starting at `src` for characters that cannot appear
 * in a block-context plain scalar.  Returns the number of safe bytes
 * (all in the ASCII printable range, excluding space, '#', ':', and high bytes).
 */
static inline size_t
yaml_scan_plain_block_sse2(const yaml_char_t *src, size_t len)
{
    const yaml_char_t *p = src;
    const __m128i v_space = _mm_set1_epi8(0x20);
    const __m128i v_hash  = _mm_set1_epi8('#');
    const __m128i v_colon = _mm_set1_epi8(':');

    while (len >= 16) {
        __m128i data = _mm_loadu_si128((const __m128i *)p);
        /* ch <= 0x20: unsigned compare via max(ch, 0x20) == 0x20 */
        __m128i le_space = _mm_cmpeq_epi8(_mm_max_epu8(data, v_space), v_space);
        __m128i eq_hash  = _mm_cmpeq_epi8(data, v_hash);
        __m128i eq_colon = _mm_cmpeq_epi8(data, v_colon);
        /* ch >= 0x80: sign bit is set for values 128-255 */
        int hi_mask = _mm_movemask_epi8(data);
        int stop_mask = _mm_movemask_epi8(_mm_or_si128(_mm_or_si128(le_space, eq_hash), eq_colon));
        stop_mask |= hi_mask;
        if (stop_mask) {
            return (size_t)(p - src) + (size_t)__builtin_ctz((unsigned)stop_mask);
        }
        p += 16;
        len -= 16;
    }
    /* Tail: scalar fallback */
    while (len > 0) {
        unsigned char ch = *p;
        if (ch <= 0x20 || ch == '#' || ch == ':' || ch >= 0x80)
            break;
        p++;
        len--;
    }
    return (size_t)(p - src);
}

/*
 * Same as above but for flow-context plain scalars (additional terminators).
 */
static inline size_t
yaml_scan_plain_flow_sse2(const yaml_char_t *src, size_t len)
{
    const yaml_char_t *p = src;
    const __m128i v_space = _mm_set1_epi8(0x20);
    const __m128i v_hash  = _mm_set1_epi8('#');
    const __m128i v_colon = _mm_set1_epi8(':');
    const __m128i v_comma = _mm_set1_epi8(',');
    const __m128i v_lbrack = _mm_set1_epi8('[');
    const __m128i v_rbrack = _mm_set1_epi8(']');
    const __m128i v_lbrace = _mm_set1_epi8('{');
    const __m128i v_rbrace = _mm_set1_epi8('}');

    while (len >= 16) {
        __m128i data = _mm_loadu_si128((const __m128i *)p);
        __m128i le_space = _mm_cmpeq_epi8(_mm_max_epu8(data, v_space), v_space);
        __m128i eq_hash  = _mm_cmpeq_epi8(data, v_hash);
        __m128i eq_colon = _mm_cmpeq_epi8(data, v_colon);
        __m128i eq_comma = _mm_cmpeq_epi8(data, v_comma);
        __m128i eq_lb    = _mm_cmpeq_epi8(data, v_lbrack);
        __m128i eq_rb    = _mm_cmpeq_epi8(data, v_rbrack);
        __m128i eq_lc    = _mm_cmpeq_epi8(data, v_lbrace);
        __m128i eq_rc    = _mm_cmpeq_epi8(data, v_rbrace);
        int hi_mask = _mm_movemask_epi8(data);
        int stop_mask = _mm_movemask_epi8(
            _mm_or_si128(
                _mm_or_si128(
                    _mm_or_si128(le_space, eq_hash),
                    _mm_or_si128(eq_colon, eq_comma)),
                _mm_or_si128(
                    _mm_or_si128(eq_lb, eq_rb),
                    _mm_or_si128(eq_lc, eq_rc))));
        stop_mask |= hi_mask;
        if (stop_mask) {
            return (size_t)(p - src) + (size_t)__builtin_ctz((unsigned)stop_mask);
        }
        p += 16;
        len -= 16;
    }
    while (len > 0) {
        unsigned char ch = *p;
        if (ch <= 0x20 || ch == '#' || ch == ':' || ch >= 0x80
            || ch == ',' || ch == '[' || ch == ']'
            || ch == '{' || ch == '}')
            break;
        p++;
        len--;
    }
    return (size_t)(p - src);
}

/*
 * Scan for flow scalar terminators (quote char, backslash, control/high bytes).
 * Used for both single-quoted and double-quoted scalar batch copy.
 */
static inline size_t
yaml_scan_flow_scalar_sse2(const yaml_char_t *src, size_t len,
        unsigned char quote)
{
    const yaml_char_t *p = src;
    const __m128i v_space = _mm_set1_epi8(0x20);
    const __m128i v_quote = _mm_set1_epi8((char)quote);
    const __m128i v_bslash = _mm_set1_epi8('\\');

    while (len >= 16) {
        __m128i data = _mm_loadu_si128((const __m128i *)p);
        __m128i le_space = _mm_cmpeq_epi8(_mm_max_epu8(data, v_space), v_space);
        __m128i eq_quote = _mm_cmpeq_epi8(data, v_quote);
        __m128i eq_bslash = _mm_cmpeq_epi8(data, v_bslash);
        int hi_mask = _mm_movemask_epi8(data);
        int stop_mask = _mm_movemask_epi8(
            _mm_or_si128(_mm_or_si128(le_space, eq_quote), eq_bslash));
        stop_mask |= hi_mask;
        if (stop_mask) {
            return (size_t)(p - src) + (size_t)__builtin_ctz((unsigned)stop_mask);
        }
        p += 16;
        len -= 16;
    }
    while (len > 0) {
        unsigned char ch = *p;
        if (ch <= 0x20 || ch >= 0x80 || ch == quote || ch == '\\')
            break;
        p++;
        len--;
    }
    return (size_t)(p - src);
}

/*
 * Scan for block scalar line terminators (NUL, CR, LF, high bytes).
 */
static inline size_t
yaml_scan_block_scalar_sse2(const yaml_char_t *src, size_t len)
{
    const yaml_char_t *p = src;
    const __m128i v_nul = _mm_setzero_si128();
    const __m128i v_lf  = _mm_set1_epi8('\n');
    const __m128i v_cr  = _mm_set1_epi8('\r');

    while (len >= 16) {
        __m128i data = _mm_loadu_si128((const __m128i *)p);
        __m128i eq_nul = _mm_cmpeq_epi8(data, v_nul);
        __m128i eq_lf  = _mm_cmpeq_epi8(data, v_lf);
        __m128i eq_cr  = _mm_cmpeq_epi8(data, v_cr);
        int hi_mask = _mm_movemask_epi8(data);
        int stop_mask = _mm_movemask_epi8(
            _mm_or_si128(_mm_or_si128(eq_nul, eq_lf), eq_cr));
        stop_mask |= hi_mask;
        if (stop_mask) {
            return (size_t)(p - src) + (size_t)__builtin_ctz((unsigned)stop_mask);
        }
        p += 16;
        len -= 16;
    }
    while (len > 0) {
        unsigned char ch = *p;
        if (ch == '\0' || ch == '\n' || ch == '\r' || ch >= 0x80)
            break;
        p++;
        len--;
    }
    return (size_t)(p - src);
}

#define HAVE_SIMD_SCAN 1
#endif /* __SSE2__ && __x86_64__ */

/*
 * Ensure that the buffer contains the required number of characters.
 * Return 1 on success, 0 on failure (reader error or memory error).
 */

#define CACHE(parser,length)                                                    \
    (__builtin_expect(parser->unread >= (length), 1)                            \
        ? 1                                                                     \
        : yaml_parser_update_buffer(parser, (length)))

/*
 * Advance the buffer pointer.
 */

#define SKIP(parser)                                                            \
     (parser->mark.index ++,                                                    \
      parser->mark.column ++,                                                   \
      parser->unread --,                                                        \
      parser->buffer.pointer +=                                                 \
        __builtin_expect((*(parser->buffer.pointer) & 0x80) == 0, 1)            \
        ? 1 : WIDTH(parser->buffer))

#define SKIP_LINE(parser)                                                       \
     (IS_CRLF(parser->buffer) ?                                                 \
      (parser->mark.index += 2,                                                 \
       parser->mark.column = 0,                                                 \
       parser->mark.line ++,                                                    \
       parser->unread -= 2,                                                     \
       parser->buffer.pointer += 2) :                                           \
      IS_BREAK(parser->buffer) ?                                                \
      (parser->mark.index ++,                                                   \
       parser->mark.column = 0,                                                 \
       parser->mark.line ++,                                                    \
       parser->unread --,                                                       \
       parser->buffer.pointer += WIDTH(parser->buffer)) : 0)

/*
 * Copy a character to a string buffer and advance pointers.
 */

#define READ(parser,string)                                                     \
     (STRING_EXTEND(parser,string) ?                                            \
         (COPY(string,parser->buffer),                                          \
          parser->mark.index ++,                                                \
          parser->mark.column ++,                                               \
          parser->unread --,                                                    \
          1) : 0)

/*
 * Copy a line break character to a string buffer and advance pointers.
 */

#define READ_LINE(parser,string)                                                \
    (STRING_EXTEND(parser,string) ?                                             \
    (((CHECK_AT(parser->buffer,'\r',0)                                          \
       && CHECK_AT(parser->buffer,'\n',1)) ?        /* CR LF -> LF */           \
     (*((string).pointer++) = (yaml_char_t) '\n',                               \
      parser->buffer.pointer += 2,                                              \
      parser->mark.index += 2,                                                  \
      parser->mark.column = 0,                                                  \
      parser->mark.line ++,                                                     \
      parser->unread -= 2) :                                                    \
     (CHECK_AT(parser->buffer,'\r',0)                                           \
      || CHECK_AT(parser->buffer,'\n',0)) ?         /* CR|LF -> LF */           \
     (*((string).pointer++) = (yaml_char_t) '\n',                               \
      parser->buffer.pointer ++,                                                \
      parser->mark.index ++,                                                    \
      parser->mark.column = 0,                                                  \
      parser->mark.line ++,                                                     \
      parser->unread --) :                                                      \
     (CHECK_AT(parser->buffer,'\xC2',0)                                         \
      && CHECK_AT(parser->buffer,'\x85',1)) ?       /* NEL -> LF */             \
     (*((string).pointer++) = (yaml_char_t) '\n',                               \
      parser->buffer.pointer += 2,                                              \
      parser->mark.index ++,                                                    \
      parser->mark.column = 0,                                                  \
      parser->mark.line ++,                                                     \
      parser->unread --) :                                                      \
     (CHECK_AT(parser->buffer,'\xE2',0) &&                                      \
      CHECK_AT(parser->buffer,'\x80',1) &&                                      \
      (CHECK_AT(parser->buffer,'\xA8',2) ||                                     \
       CHECK_AT(parser->buffer,'\xA9',2))) ?        /* LS|PS -> LS|PS */        \
     (*((string).pointer++) = *(parser->buffer.pointer++),                      \
      *((string).pointer++) = *(parser->buffer.pointer++),                      \
      *((string).pointer++) = *(parser->buffer.pointer++),                      \
      parser->mark.index ++,                                                    \
      parser->mark.column = 0,                                                  \
      parser->mark.line ++,                                                     \
      parser->unread --) : 0),                                                  \
    1) : 0)

/*
 * Public API declarations.
 */

YAML_DECLARE(int)
yaml_parser_scan(yaml_parser_t *parser, yaml_token_t *token);

/*
 * Error handling.
 */

static int
yaml_parser_set_scanner_error(yaml_parser_t *parser, const char *context,
        yaml_mark_t context_mark, const char *problem);

/*
 * High-level token API.
 */

YAML_DECLARE(int)
yaml_parser_fetch_more_tokens(yaml_parser_t *parser);

static int
yaml_parser_fetch_next_token(yaml_parser_t *parser);

/*
 * Potential simple keys.
 */

static int
yaml_parser_stale_simple_keys(yaml_parser_t *parser);

static int
yaml_parser_save_simple_key(yaml_parser_t *parser);

static int
yaml_parser_remove_simple_key(yaml_parser_t *parser);

static int
yaml_parser_increase_flow_level(yaml_parser_t *parser);

static int
yaml_parser_decrease_flow_level(yaml_parser_t *parser);

/*
 * Indentation treatment.
 */

static int
yaml_parser_roll_indent(yaml_parser_t *parser, ptrdiff_t column,
        ptrdiff_t number, yaml_token_type_t type, yaml_mark_t mark);

static int
yaml_parser_unroll_indent(yaml_parser_t *parser, ptrdiff_t column);

/*
 * Token fetchers.
 */

static int
yaml_parser_fetch_stream_start(yaml_parser_t *parser);

static int
yaml_parser_fetch_stream_end(yaml_parser_t *parser);

static int
yaml_parser_fetch_directive(yaml_parser_t *parser);

static int
yaml_parser_fetch_document_indicator(yaml_parser_t *parser,
        yaml_token_type_t type);

static int
yaml_parser_fetch_flow_collection_start(yaml_parser_t *parser,
        yaml_token_type_t type);

static int
yaml_parser_fetch_flow_collection_end(yaml_parser_t *parser,
        yaml_token_type_t type);

static int
yaml_parser_fetch_flow_entry(yaml_parser_t *parser);

static int
yaml_parser_fetch_block_entry(yaml_parser_t *parser);

static int
yaml_parser_fetch_key(yaml_parser_t *parser);

static int
yaml_parser_fetch_value(yaml_parser_t *parser);

static int
yaml_parser_fetch_anchor(yaml_parser_t *parser, yaml_token_type_t type);

static int
yaml_parser_fetch_tag(yaml_parser_t *parser);

static int
yaml_parser_fetch_block_scalar(yaml_parser_t *parser, int literal);

static int
yaml_parser_fetch_flow_scalar(yaml_parser_t *parser, int single);

static int
yaml_parser_fetch_plain_scalar(yaml_parser_t *parser);

/*
 * Token scanners.
 */

static int
yaml_parser_scan_to_next_token(yaml_parser_t *parser);

static int
yaml_parser_scan_directive(yaml_parser_t *parser, yaml_token_t *token);

static int
yaml_parser_scan_directive_name(yaml_parser_t *parser,
        yaml_mark_t start_mark, yaml_char_t **name);

static int
yaml_parser_scan_version_directive_value(yaml_parser_t *parser,
        yaml_mark_t start_mark, int *major, int *minor);

static int
yaml_parser_scan_version_directive_number(yaml_parser_t *parser,
        yaml_mark_t start_mark, int *number);

static int
yaml_parser_scan_tag_directive_value(yaml_parser_t *parser,
        yaml_mark_t mark, yaml_char_t **handle, yaml_char_t **prefix);

static int
yaml_parser_scan_anchor(yaml_parser_t *parser, yaml_token_t *token,
        yaml_token_type_t type);

static int
yaml_parser_scan_tag(yaml_parser_t *parser, yaml_token_t *token);

static int
yaml_parser_scan_tag_handle(yaml_parser_t *parser, int directive,
        yaml_mark_t start_mark, yaml_char_t **handle);

static int
yaml_parser_scan_tag_uri(yaml_parser_t *parser, int uri_char, int directive,
        yaml_char_t *head, yaml_mark_t start_mark, yaml_char_t **uri);

static int
yaml_parser_scan_uri_escapes(yaml_parser_t *parser, int directive,
        yaml_mark_t start_mark, yaml_string_t *string);

static int
yaml_parser_scan_block_scalar(yaml_parser_t *parser, yaml_token_t *token,
        int literal);

static int
yaml_parser_scan_block_scalar_breaks(yaml_parser_t *parser,
        int *indent, yaml_string_t *breaks,
        yaml_mark_t start_mark, yaml_mark_t *end_mark);

static int
yaml_parser_scan_flow_scalar(yaml_parser_t *parser, yaml_token_t *token,
        int single);

static int
yaml_parser_scan_plain_scalar(yaml_parser_t *parser, yaml_token_t *token);

/*
 * Get the next token.
 */

YAML_DECLARE(int)
yaml_parser_scan(yaml_parser_t *parser, yaml_token_t *token)
{
    assert(parser); /* Non-NULL parser object is expected. */
    assert(token);  /* Non-NULL token object is expected. */

    /* No tokens after STREAM-END or error. */

    if (parser->stream_end_produced || parser->error) {
        memset(token, 0, sizeof(yaml_token_t));
        return 1;
    }

    /* Ensure that the tokens queue contains enough tokens. */

    if (!parser->token_available) {
        if (!yaml_parser_fetch_more_tokens(parser)) {
            memset(token, 0, sizeof(yaml_token_t));
            return 0;
        }
    }

    /* Fetch the next token from the queue. */

    *token = DEQUEUE(parser, parser->tokens);
    parser->tokens_parsed ++;

    if (__builtin_expect(token->type == YAML_STREAM_END_TOKEN, 0)) {
        parser->stream_end_produced = 1;
        parser->token_available = 0;
    } else {
        parser->token_available =
            (parser->tokens.head != parser->tokens.tail
                && parser->possible_simple_key_count == 0);
    }

    return 1;
}

/*
 * Set the scanner error and return 0.
 */

static int
yaml_parser_set_scanner_error(yaml_parser_t *parser, const char *context,
        yaml_mark_t context_mark, const char *problem)
{
    parser->error = YAML_SCANNER_ERROR;
    parser->context = context;
    parser->context_mark = context_mark;
    parser->problem = problem;
    parser->problem_mark = parser->mark;

    return 0;
}

/*
 * Ensure that the tokens queue contains at least one token which can be
 * returned to the Parser.
 */

YAML_DECLARE(int)
yaml_parser_fetch_more_tokens(yaml_parser_t *parser)
{
    int need_more_tokens;

    /* While we need more tokens to fetch, do it. */

    while (1)
    {
        /*
         * Check if we really need to fetch more tokens.
         */

        need_more_tokens = 0;

        if (__builtin_expect(parser->tokens.head == parser->tokens.tail, 1))
        {
            /* Queue is empty. */

            need_more_tokens = 1;
        }
        else if (__builtin_expect(parser->possible_simple_key_count > 0, 0))
        {
            yaml_simple_key_t *simple_key;
            size_t tokens_parsed = parser->tokens_parsed;
            size_t mark_line = parser->mark.line;
            size_t mark_index = parser->mark.index;

            /* Check stale simple keys + head-position check.
             * Only entered when there are possible simple keys. */

            for (simple_key = parser->simple_keys.start;
                    simple_key != parser->simple_keys.top; simple_key++) {
                if (!simple_key->possible) continue;
                if (__builtin_expect(simple_key->mark.line < mark_line
                        || simple_key->mark.index+1024 < mark_index, 0)) {
                    if (simple_key->required) {
                        return yaml_parser_set_scanner_error(parser,
                                "while scanning a simple key", simple_key->mark,
                                "could not find expected ':'");
                    }
                    simple_key->possible = 0;
                    parser->possible_simple_key_count --;
                    continue;
                }
                if (simple_key->token_number == tokens_parsed) {
                    need_more_tokens = 1;
                    break;
                }
            }
        }

        /* We are finished. */

        if (__builtin_expect(!need_more_tokens, 1))
            break;

        /* Fetch the next token. */

        if (!yaml_parser_fetch_next_token(parser))
            return 0;
    }

    parser->token_available = 1;

    return 1;
}

/*
 * Batch fast path: emit KEY + SCALAR(key) + VALUE + SCALAR(value) tokens
 * for a simple "key: value" block mapping entry.
 * Returns: 1 = success (tokens emitted), 0 = not applicable, -1 = error.
 */
static int
yaml_parser_fetch_kv_pair_batch(yaml_parser_t *parser)
{
    unsigned char ch0 = parser->buffer.pointer[0];

    /* Quick check: is this a plain scalar start in block context?
     * Excludes indicators and special chars. */
    if (ch0 <= 0x20 || ch0 >= 0x80
            || ch0 == '#' || ch0 == ':' || ch0 == '-' || ch0 == '.'
            || ch0 == '?' || ch0 == '*' || ch0 == '&' || ch0 == '!'
            || ch0 == '|' || ch0 == '>' || ch0 == '\'' || ch0 == '"'
            || ch0 == '@' || ch0 == '`' || ch0 == '%'
            || ch0 == '[' || ch0 == ']' || ch0 == '{' || ch0 == '}' || ch0 == ',')
        return 0;

    yaml_char_t *src = parser->buffer.pointer;
    size_t avail = parser->unread - 1; /* reserve 1 for lookahead */
    size_t key_len;

#ifdef HAVE_SIMD_SCAN
    key_len = yaml_scan_plain_block_sse2(src, avail);
#else
    {
        yaml_char_t *p = src;
        yaml_char_t *lim = src + avail;
        while (p < lim) {
            unsigned char c = *p;
            if (c <= 0x20 || c == '#' || c == ':' || c >= 0x80)
                break;
            p++;
        }
        key_len = (size_t)(p - src);
    }
#endif

    /* Need at least: key + ": " + 1 value char + terminator */
    if (key_len == 0 || key_len + 3 >= parser->unread
            || src[key_len] != ':' || src[key_len + 1] != ' ')
        return 0;

    /* Check that the value starts with a plain scalar char. */
    unsigned char vch = src[key_len + 2];
    if (vch <= 0x20 || vch >= 0x80
            || vch == '#' || vch == ':' || vch == '-' || vch == '.'
            || vch == '?' || vch == '*' || vch == '&' || vch == '!'
            || vch == '|' || vch == '>' || vch == '\'' || vch == '"'
            || vch == '@' || vch == '`' || vch == '%'
            || vch == '[' || vch == ']' || vch == '{' || vch == '}' || vch == ',')
        return 0;

    /* Scan value: starts at src[key_len + 2] */
    yaml_char_t *val_src = src + key_len + 2;
    size_t val_avail = parser->unread - key_len - 3;
    size_t val_len;

#ifdef HAVE_SIMD_SCAN
    val_len = yaml_scan_plain_block_sse2(val_src, val_avail);
#else
    {
        yaml_char_t *p = val_src;
        yaml_char_t *lim = val_src + val_avail;
        while (p < lim) {
            unsigned char c = *p;
            if (c <= 0x20 || c == '#' || c == ':' || c >= 0x80)
                break;
            p++;
        }
        val_len = (size_t)(p - val_src);
    }
#endif

    /* Value must be non-empty and followed by a definitive terminator. */
    if (val_len == 0)
        return 0;

    yaml_char_t term = val_src[val_len];
    int is_end = 0;

    if (term == '\0') {
        is_end = 1;
    } else if (term == '\n' || term == '\r') {
        /* Check next line indent for scalar end */
        size_t skip = 1;
        if (term == '\r' && key_len + 2 + val_len + skip < parser->unread
                && val_src[val_len + skip] == '\n')
            skip++;
        size_t next_col = 0;
        size_t remaining = parser->unread - (key_len + 2 + val_len + skip);
        while (next_col < remaining
                && val_src[val_len + skip + next_col] == ' ')
            next_col++;
        if (next_col < remaining
                && val_src[val_len + skip + next_col] != ' '
                && (int)next_col < parser->indent + 1) {
            is_end = 1;
        }
    } else if (term == ':' && key_len + 2 + val_len + 1 < parser->unread
            && (val_src[val_len + 1] == ' '
                || val_src[val_len + 1] == '\t'
                || val_src[val_len + 1] == '\n'
                || val_src[val_len + 1] == '\r'
                || val_src[val_len + 1] == '\0')) {
        is_end = 1;
    }

    if (!is_end)
        return 0;

    /* All conditions met. Emit tokens in batch. */
    yaml_mark_t key_start = parser->mark;
    yaml_mark_t key_end;
    yaml_mark_t val_start;
    yaml_mark_t val_end_mark;
    yaml_token_t token;

    /* 1) BLOCK-MAPPING-START if indent needs rolling */
    if (parser->indent < (ptrdiff_t)parser->mark.column) {
        if (!yaml_parser_roll_indent(parser,
                    parser->mark.column, -1,
                    YAML_BLOCK_MAPPING_START_TOKEN,
                    parser->mark))
            return -1;
    }

    /* 2) KEY token */
    TOKEN_INIT(token, YAML_KEY_TOKEN, key_start, key_start);
    if (!ENQUEUE(parser, parser->tokens, token))
        return -1;

    /* 3) SCALAR(key) -- allocate and copy */
    {
        yaml_char_t *kval = (yaml_char_t *)yaml_malloc_internal(key_len + 1);
        if (!kval) {
            parser->error = YAML_MEMORY_ERROR;
            return -1;
        }
        memcpy(kval, src, key_len);
        kval[key_len] = '\0';

        /* Advance parser past key */
        parser->buffer.pointer += key_len;
        parser->mark.index += key_len;
        parser->mark.column += key_len;
        parser->unread -= key_len;
        key_end = parser->mark;

        if (!ENQUEUE_RESERVE(parser, parser->tokens))
            { yaml_free(kval); return -1; }
        SCALAR_TOKEN_INIT(*parser->tokens.tail, kval,
                key_len, YAML_PLAIN_SCALAR_STYLE,
                key_start, key_end);
        ENQUEUE_COMMIT(parser->tokens);
    }

    /* 4) VALUE token -- skip ": " */
    {
        yaml_mark_t vmark = parser->mark;
        parser->buffer.pointer += 1; /* skip ':' */
        parser->mark.index += 1;
        parser->mark.column += 1;
        parser->unread -= 1;
        yaml_mark_t vmark_end = parser->mark;

        TOKEN_INIT(token, YAML_VALUE_TOKEN, vmark, vmark_end);
        if (!ENQUEUE(parser, parser->tokens, token))
            return -1;

        /* skip ' ' */
        parser->buffer.pointer += 1;
        parser->mark.index += 1;
        parser->mark.column += 1;
        parser->unread -= 1;
    }

    /* 5) SCALAR(value) -- allocate and copy */
    {
        yaml_char_t *vval = (yaml_char_t *)yaml_malloc_internal(val_len + 1);
        if (!vval) {
            parser->error = YAML_MEMORY_ERROR;
            return -1;
        }
        val_start = parser->mark;
        memcpy(vval, parser->buffer.pointer, val_len);
        vval[val_len] = '\0';

        parser->buffer.pointer += val_len;
        parser->mark.index += val_len;
        parser->mark.column += val_len;
        parser->unread -= val_len;
        val_end_mark = parser->mark;

        if (!ENQUEUE_RESERVE(parser, parser->tokens))
            { yaml_free(vval); return -1; }
        SCALAR_TOKEN_INIT(*parser->tokens.tail, vval,
                val_len, YAML_PLAIN_SCALAR_STYLE,
                val_start, val_end_mark);
        ENQUEUE_COMMIT(parser->tokens);
    }

    /* Reset simple key state: after a value, no simple key is active. */
    parser->simple_key_allowed = 0;

    /* Remove any existing simple key for this level */
    {
        yaml_simple_key_t *sk = parser->simple_keys.top - 1;
        if (sk->possible) {
            sk->possible = 0;
            parser->possible_simple_key_count--;
        }
    }

    return 1;
}

/*
 * The dispatcher for token fetchers.
 */

static int
yaml_parser_fetch_next_token(yaml_parser_t *parser)
{
    /* Check if we just started scanning.  Fetch STREAM-START then. */

    if (__builtin_expect(!parser->stream_start_produced, 0)) {
        if (!CACHE(parser, 1))
            return 0;
        return yaml_parser_fetch_stream_start(parser);
    }

    /* Eat whitespaces and comments until we reach the next token. */

    if (!yaml_parser_scan_to_next_token(parser))
        return 0;

    /* Remove obsolete potential simple keys.
     * Skip when no possible simple keys exist (common case). */

    if (__builtin_expect(parser->possible_simple_key_count > 0, 0)) {
        if (!yaml_parser_stale_simple_keys(parser))
            return 0;
    }

    /* Check the indentation level against the current column (block context only).
     * Inline the common case where indent <= column (no unrolling needed). */

    if (!parser->flow_level && parser->indent > (ptrdiff_t)parser->mark.column) {
        if (!yaml_parser_unroll_indent(parser, parser->mark.column))
            return 0;
    }

    /*
     * Ensure that the buffer contains at least 4 characters.  4 is the length
     * of the longest indicators ('--- ' and '... ').
     */

    if (!CACHE(parser, 4))
        return 0;

    /*
     * Batch fast path for block mapping key-value pairs.
     * When in block context with simple keys allowed and the current char
     * starts a plain scalar key followed by ": value\n", emit
     * KEY + SCALAR(key) + VALUE + SCALAR(value) in one call.
     */

    if (__builtin_expect(!parser->flow_level && parser->simple_key_allowed
                && parser->unread >= 6, 1))
    {
        int batch = yaml_parser_fetch_kv_pair_batch(parser);
        if (batch < 0) return 0;  /* error */
        if (batch > 0) return 1;  /* tokens emitted */
        /* batch == 0: conditions not met, fall through to normal dispatch */
    }

    /*
     * Dispatch the next token based on the current character.
     * Using a switch statement enables the compiler to generate a jump table
     * for O(1) dispatch instead of sequential if-else comparisons.
     */

    switch (parser->buffer.pointer[0])
    {
        case '\0':
            return yaml_parser_fetch_stream_end(parser);

        case '%':
            if (parser->mark.column == 0)
                return yaml_parser_fetch_directive(parser);
            return yaml_parser_fetch_plain_scalar(parser);

        case '-':
            if (parser->mark.column == 0
                    && CHECK_AT(parser->buffer, '-', 1)
                    && CHECK_AT(parser->buffer, '-', 2)
                    && IS_BLANKZ_AT(parser->buffer, 3))
                return yaml_parser_fetch_document_indicator(parser,
                        YAML_DOCUMENT_START_TOKEN);
            if (IS_BLANKZ_AT(parser->buffer, 1))
                return yaml_parser_fetch_block_entry(parser);
            return yaml_parser_fetch_plain_scalar(parser);

        case '.':
            if (parser->mark.column == 0
                    && CHECK_AT(parser->buffer, '.', 1)
                    && CHECK_AT(parser->buffer, '.', 2)
                    && IS_BLANKZ_AT(parser->buffer, 3))
                return yaml_parser_fetch_document_indicator(parser,
                        YAML_DOCUMENT_END_TOKEN);
            return yaml_parser_fetch_plain_scalar(parser);

        case '[':
            return yaml_parser_fetch_flow_collection_start(parser,
                    YAML_FLOW_SEQUENCE_START_TOKEN);

        case '{':
            return yaml_parser_fetch_flow_collection_start(parser,
                    YAML_FLOW_MAPPING_START_TOKEN);

        case ']':
            return yaml_parser_fetch_flow_collection_end(parser,
                    YAML_FLOW_SEQUENCE_END_TOKEN);

        case '}':
            return yaml_parser_fetch_flow_collection_end(parser,
                    YAML_FLOW_MAPPING_END_TOKEN);

        case ',':
            return yaml_parser_fetch_flow_entry(parser);

        case '?':
            if (parser->flow_level || IS_BLANKZ_AT(parser->buffer, 1))
                return yaml_parser_fetch_key(parser);
            return yaml_parser_fetch_plain_scalar(parser);

        case ':':
            if (parser->flow_level || IS_BLANKZ_AT(parser->buffer, 1))
                return yaml_parser_fetch_value(parser);
            return yaml_parser_fetch_plain_scalar(parser);

        case '*':
            return yaml_parser_fetch_anchor(parser, YAML_ALIAS_TOKEN);

        case '&':
            return yaml_parser_fetch_anchor(parser, YAML_ANCHOR_TOKEN);

        case '!':
            return yaml_parser_fetch_tag(parser);

        case '|':
            if (!parser->flow_level)
                return yaml_parser_fetch_block_scalar(parser, 1);
            break;

        case '>':
            if (!parser->flow_level)
                return yaml_parser_fetch_block_scalar(parser, 0);
            break;

        case '\'':
            return yaml_parser_fetch_flow_scalar(parser, 1);

        case '"':
            return yaml_parser_fetch_flow_scalar(parser, 0);

        /* Characters that cannot start any token or plain scalar. */
        case '#': case '@': case '`':
            break;

        /* Line break characters never reach here (consumed by scan_to_next_token). */
        case '\r': case '\n':
        case '\t': case ' ':
            break;

        default:
            /* Any other character starts a plain scalar. */
            return yaml_parser_fetch_plain_scalar(parser);
    }

    /*
     * If we don't determine the token type so far, it is an error.
     */

    return yaml_parser_set_scanner_error(parser,
            "while scanning for the next token", parser->mark,
            "found character that cannot start any token");
}

/*
 * Check the list of potential simple keys and remove the positions that
 * cannot contain simple keys anymore.
 */

static int
yaml_parser_stale_simple_keys(yaml_parser_t *parser)
{
    yaml_simple_key_t *simple_key;
    size_t mark_line = parser->mark.line;
    size_t mark_index = parser->mark.index;

    /* Check for a potential simple key for each flow level. */

    for (simple_key = parser->simple_keys.start;
            simple_key != parser->simple_keys.top; simple_key ++)
    {
        /*
         * The specification requires that a simple key
         *
         *  - is limited to a single line,
         *  - is shorter than 1024 characters.
         */

        if (simple_key->possible
                && (simple_key->mark.line < mark_line
                    || simple_key->mark.index+1024 < mark_index)) {

            /* Check if the potential simple key to be removed is required. */

            if (simple_key->required) {
                return yaml_parser_set_scanner_error(parser,
                        "while scanning a simple key", simple_key->mark,
                        "could not find expected ':'");
            }

            simple_key->possible = 0;
            parser->possible_simple_key_count --;
        }
    }

    return 1;
}

/*
 * Check if a simple key may start at the current position and add it if
 * needed.
 */

static int
yaml_parser_save_simple_key(yaml_parser_t *parser)
{
    /*
     * A simple key is required at the current position if the scanner is in
     * the block context and the current column coincides with the indentation
     * level.
     */

    int required = (!parser->flow_level
            && parser->indent == (ptrdiff_t)parser->mark.column);

    /*
     * If the current position may start a simple key, save it.
     */

    if (parser->simple_key_allowed)
    {
        yaml_simple_key_t simple_key;
        simple_key.possible = 1;
        simple_key.required = required;
        simple_key.token_number =
            parser->tokens_parsed + (parser->tokens.tail - parser->tokens.head);
        simple_key.mark = parser->mark;

        if (!yaml_parser_remove_simple_key(parser)) return 0;

        *(parser->simple_keys.top-1) = simple_key;
        parser->possible_simple_key_count ++;
    }

    return 1;
}

/*
 * Remove a potential simple key at the current flow level.
 */

static int
yaml_parser_remove_simple_key(yaml_parser_t *parser)
{
    yaml_simple_key_t *simple_key = parser->simple_keys.top-1;

    if (simple_key->possible)
    {
        /* If the key is required, it is an error. */

        if (simple_key->required) {
            return yaml_parser_set_scanner_error(parser,
                    "while scanning a simple key", simple_key->mark,
                    "could not find expected ':'");
        }

        simple_key->possible = 0;
        parser->possible_simple_key_count --;
    }

    return 1;
}

/*
 * Increase the flow level and resize the simple key list if needed.
 */

static int
yaml_parser_increase_flow_level(yaml_parser_t *parser)
{
    yaml_simple_key_t empty_simple_key = { 0, 0, 0, { 0, 0, 0 } };

    /* Reset the simple key on the next level. */

    if (!PUSH(parser, parser->simple_keys, empty_simple_key))
        return 0;

    /* Increase the flow level. */

    if (parser->flow_level == INT_MAX) {
        parser->error = YAML_MEMORY_ERROR;
        return 0;
    }

    parser->flow_level++;

    return 1;
}

/*
 * Decrease the flow level.
 */

static int
yaml_parser_decrease_flow_level(yaml_parser_t *parser)
{
    if (parser->flow_level) {
        parser->flow_level --;
        if ((parser->simple_keys.top-1)->possible)
            parser->possible_simple_key_count --;
        (void)POP(parser, parser->simple_keys);
    }

    return 1;
}

/*
 * Push the current indentation level to the stack and set the new level
 * the current column is greater than the indentation level.  In this case,
 * append or insert the specified token into the token queue.
 *
 */

static int
yaml_parser_roll_indent(yaml_parser_t *parser, ptrdiff_t column,
        ptrdiff_t number, yaml_token_type_t type, yaml_mark_t mark)
{
    yaml_token_t token;

    /* In the flow context, do nothing. */

    if (parser->flow_level)
        return 1;

    if (parser->indent < column)
    {
        /*
         * Push the current indentation level to the stack and set the new
         * indentation level.
         */

        if (!PUSH(parser, parser->indents, parser->indent))
            return 0;

        if (column > INT_MAX) {
            parser->error = YAML_MEMORY_ERROR;
            return 0;
        }

        parser->indent = column;

        /* Create a token and insert it into the queue. */

        TOKEN_INIT(token, type, mark, mark);

        if (number == -1) {
            if (!ENQUEUE(parser, parser->tokens, token))
                return 0;
        }
        else {
            if (!QUEUE_INSERT(parser,
                        parser->tokens, number - parser->tokens_parsed, token))
                return 0;
        }
    }

    return 1;
}

/*
 * Pop indentation levels from the indents stack until the current level
 * becomes less or equal to the column.  For each indentation level, append
 * the BLOCK-END token.
 */


static int
yaml_parser_unroll_indent(yaml_parser_t *parser, ptrdiff_t column)
{
    yaml_token_t token;

    /* In the flow context, do nothing. */

    if (parser->flow_level)
        return 1;

    /* Loop through the indentation levels in the stack. */

    while (parser->indent > column)
    {
        /* Create a token and append it to the queue. */

        TOKEN_INIT(token, YAML_BLOCK_END_TOKEN, parser->mark, parser->mark);

        if (!ENQUEUE(parser, parser->tokens, token))
            return 0;

        /* Pop the indentation level. */

        parser->indent = POP(parser, parser->indents);
    }

    return 1;
}

/*
 * Initialize the scanner and produce the STREAM-START token.
 */

static int
yaml_parser_fetch_stream_start(yaml_parser_t *parser)
{
    yaml_simple_key_t simple_key = { 0, 0, 0, { 0, 0, 0 } };
    yaml_token_t token;

    /* Set the initial indentation. */

    parser->indent = -1;

    /* Initialize the simple key stack. */

    if (!PUSH(parser, parser->simple_keys, simple_key))
        return 0;

    /* A simple key is allowed at the beginning of the stream. */

    parser->simple_key_allowed = 1;

    /* We have started. */

    parser->stream_start_produced = 1;

    /* Create the STREAM-START token and append it to the queue. */

    STREAM_START_TOKEN_INIT(token, parser->encoding,
            parser->mark, parser->mark);

    if (!ENQUEUE(parser, parser->tokens, token))
        return 0;

    return 1;
}

/*
 * Produce the STREAM-END token and shut down the scanner.
 */

static int
yaml_parser_fetch_stream_end(yaml_parser_t *parser)
{
    yaml_token_t token;

    /* Force new line. */

    if (parser->mark.column != 0) {
        parser->mark.column = 0;
        parser->mark.line ++;
    }

    /* Reset the indentation level. */

    if (!yaml_parser_unroll_indent(parser, -1))
        return 0;

    /* Reset simple keys. */

    if (!yaml_parser_remove_simple_key(parser))
        return 0;

    parser->simple_key_allowed = 0;

    /* Create the STREAM-END token and append it to the queue. */

    STREAM_END_TOKEN_INIT(token, parser->mark, parser->mark);

    if (!ENQUEUE(parser, parser->tokens, token))
        return 0;

    return 1;
}

/*
 * Produce a VERSION-DIRECTIVE or TAG-DIRECTIVE token.
 */

static int
yaml_parser_fetch_directive(yaml_parser_t *parser)
{
    yaml_token_t token;

    /* Reset the indentation level. */

    if (!yaml_parser_unroll_indent(parser, -1))
        return 0;

    /* Reset simple keys. */

    if (!yaml_parser_remove_simple_key(parser))
        return 0;

    parser->simple_key_allowed = 0;

    /* Create the YAML-DIRECTIVE or TAG-DIRECTIVE token. */

    if (!yaml_parser_scan_directive(parser, &token))
        return 0;

    /* Append the token to the queue. */

    if (!ENQUEUE(parser, parser->tokens, token)) {
        yaml_token_delete(&token);
        return 0;
    }

    return 1;
}

/*
 * Produce the DOCUMENT-START or DOCUMENT-END token.
 */

static int
yaml_parser_fetch_document_indicator(yaml_parser_t *parser,
        yaml_token_type_t type)
{
    yaml_mark_t start_mark, end_mark;
    yaml_token_t token;

    /* Reset the indentation level. */

    if (!yaml_parser_unroll_indent(parser, -1))
        return 0;

    /* Reset simple keys. */

    if (!yaml_parser_remove_simple_key(parser))
        return 0;

    parser->simple_key_allowed = 0;

    /* Consume the token. */

    start_mark = parser->mark;

    SKIP(parser);
    SKIP(parser);
    SKIP(parser);

    end_mark = parser->mark;

    /* Create the DOCUMENT-START or DOCUMENT-END token. */

    TOKEN_INIT(token, type, start_mark, end_mark);

    /* Append the token to the queue. */

    if (!ENQUEUE(parser, parser->tokens, token))
        return 0;

    return 1;
}

/*
 * Produce the FLOW-SEQUENCE-START or FLOW-MAPPING-START token.
 */

static int
yaml_parser_fetch_flow_collection_start(yaml_parser_t *parser,
        yaml_token_type_t type)
{
    yaml_mark_t start_mark, end_mark;
    yaml_token_t token;

    /* The indicators '[' and '{' may start a simple key. */

    if (!yaml_parser_save_simple_key(parser))
        return 0;

    /* Increase the flow level. */

    if (!yaml_parser_increase_flow_level(parser))
        return 0;

    /* A simple key may follow the indicators '[' and '{'. */

    parser->simple_key_allowed = 1;

    /* Consume the token. */

    start_mark = parser->mark;
    SKIP(parser);
    end_mark = parser->mark;

    /* Create the FLOW-SEQUENCE-START of FLOW-MAPPING-START token. */

    TOKEN_INIT(token, type, start_mark, end_mark);

    /* Append the token to the queue. */

    if (!ENQUEUE(parser, parser->tokens, token))
        return 0;

    return 1;
}

/*
 * Produce the FLOW-SEQUENCE-END or FLOW-MAPPING-END token.
 */

static int
yaml_parser_fetch_flow_collection_end(yaml_parser_t *parser,
        yaml_token_type_t type)
{
    yaml_mark_t start_mark, end_mark;
    yaml_token_t token;

    /* Reset any potential simple key on the current flow level. */

    if (!yaml_parser_remove_simple_key(parser))
        return 0;

    /* Decrease the flow level. */

    if (!yaml_parser_decrease_flow_level(parser))
        return 0;

    /* No simple keys after the indicators ']' and '}'. */

    parser->simple_key_allowed = 0;

    /* Consume the token. */

    start_mark = parser->mark;
    SKIP(parser);
    end_mark = parser->mark;

    /* Create the FLOW-SEQUENCE-END of FLOW-MAPPING-END token. */

    TOKEN_INIT(token, type, start_mark, end_mark);

    /* Append the token to the queue. */

    if (!ENQUEUE(parser, parser->tokens, token))
        return 0;

    return 1;
}

/*
 * Produce the FLOW-ENTRY token.
 */

static int
yaml_parser_fetch_flow_entry(yaml_parser_t *parser)
{
    yaml_mark_t start_mark, end_mark;
    yaml_token_t token;

    /* Reset any potential simple keys on the current flow level. */

    if (!yaml_parser_remove_simple_key(parser))
        return 0;

    /* Simple keys are allowed after ','. */

    parser->simple_key_allowed = 1;

    /* Consume the token. */

    start_mark = parser->mark;
    SKIP(parser);
    end_mark = parser->mark;

    /* Create the FLOW-ENTRY token and append it to the queue. */

    TOKEN_INIT(token, YAML_FLOW_ENTRY_TOKEN, start_mark, end_mark);

    if (!ENQUEUE(parser, parser->tokens, token))
        return 0;

    return 1;
}

/*
 * Produce the BLOCK-ENTRY token.
 */

static int
yaml_parser_fetch_block_entry(yaml_parser_t *parser)
{
    yaml_mark_t start_mark, end_mark;
    yaml_token_t token;

    /* Check if the scanner is in the block context. */

    if (!parser->flow_level)
    {
        /* Check if we are allowed to start a new entry. */

        if (!parser->simple_key_allowed) {
            return yaml_parser_set_scanner_error(parser, NULL, parser->mark,
                    "block sequence entries are not allowed in this context");
        }

        /* Add the BLOCK-SEQUENCE-START token if needed. */

        if (!yaml_parser_roll_indent(parser, parser->mark.column, -1,
                    YAML_BLOCK_SEQUENCE_START_TOKEN, parser->mark))
            return 0;
    }
    else
    {
        /*
         * It is an error for the '-' indicator to occur in the flow context,
         * but we let the Parser detect and report about it because the Parser
         * is able to point to the context.
         */
    }

    /* Reset any potential simple keys on the current flow level. */

    if (!yaml_parser_remove_simple_key(parser))
        return 0;

    /* Simple keys are allowed after '-'. */

    parser->simple_key_allowed = 1;

    /* Consume the token. */

    start_mark = parser->mark;
    SKIP(parser);
    end_mark = parser->mark;

    /* Create the BLOCK-ENTRY token and append it to the queue. */

    TOKEN_INIT(token, YAML_BLOCK_ENTRY_TOKEN, start_mark, end_mark);

    if (!ENQUEUE(parser, parser->tokens, token))
        return 0;

    return 1;
}

/*
 * Produce the KEY token.
 */

static int
yaml_parser_fetch_key(yaml_parser_t *parser)
{
    yaml_mark_t start_mark, end_mark;
    yaml_token_t token;

    /* In the block context, additional checks are required. */

    if (!parser->flow_level)
    {
        /* Check if we are allowed to start a new key (not necessary simple). */

        if (!parser->simple_key_allowed) {
            return yaml_parser_set_scanner_error(parser, NULL, parser->mark,
                    "mapping keys are not allowed in this context");
        }

        /* Add the BLOCK-MAPPING-START token if needed. */

        if (!yaml_parser_roll_indent(parser, parser->mark.column, -1,
                    YAML_BLOCK_MAPPING_START_TOKEN, parser->mark))
            return 0;
    }

    /* Reset any potential simple keys on the current flow level. */

    if (!yaml_parser_remove_simple_key(parser))
        return 0;

    /* Simple keys are allowed after '?' in the block context. */

    parser->simple_key_allowed = (!parser->flow_level);

    /* Consume the token. */

    start_mark = parser->mark;
    SKIP(parser);
    end_mark = parser->mark;

    /* Create the KEY token and append it to the queue. */

    TOKEN_INIT(token, YAML_KEY_TOKEN, start_mark, end_mark);

    if (!ENQUEUE(parser, parser->tokens, token))
        return 0;

    return 1;
}

/*
 * Produce the VALUE token.
 */

static int
yaml_parser_fetch_value(yaml_parser_t *parser)
{
    yaml_mark_t start_mark, end_mark;
    yaml_token_t token;
    yaml_simple_key_t *simple_key = parser->simple_keys.top-1;

    /* Have we found a simple key? */

    if (simple_key->possible)
    {

        /* Create the KEY token and insert it into the queue. */

        TOKEN_INIT(token, YAML_KEY_TOKEN, simple_key->mark, simple_key->mark);

        if (!QUEUE_INSERT(parser, parser->tokens,
                    simple_key->token_number - parser->tokens_parsed, token))
            return 0;

        /* In the block context, we may need to add the BLOCK-MAPPING-START token. */

        if (!yaml_parser_roll_indent(parser, simple_key->mark.column,
                    simple_key->token_number,
                    YAML_BLOCK_MAPPING_START_TOKEN, simple_key->mark))
            return 0;

        /* Remove the simple key. */

        simple_key->possible = 0;
        parser->possible_simple_key_count --;

        /* A simple key cannot follow another simple key. */

        parser->simple_key_allowed = 0;
    }
    else
    {
        /* The ':' indicator follows a complex key. */

        /* In the block context, extra checks are required. */

        if (!parser->flow_level)
        {
            /* Check if we are allowed to start a complex value. */

            if (!parser->simple_key_allowed) {
                return yaml_parser_set_scanner_error(parser, NULL, parser->mark,
                        "mapping values are not allowed in this context");
            }

            /* Add the BLOCK-MAPPING-START token if needed. */

            if (!yaml_parser_roll_indent(parser, parser->mark.column, -1,
                        YAML_BLOCK_MAPPING_START_TOKEN, parser->mark))
                return 0;
        }

        /* Simple keys after ':' are allowed in the block context. */

        parser->simple_key_allowed = (!parser->flow_level);
    }

    /* Consume the token. */

    start_mark = parser->mark;
    SKIP(parser);
    end_mark = parser->mark;

    /* Create the VALUE token and append it to the queue. */

    TOKEN_INIT(token, YAML_VALUE_TOKEN, start_mark, end_mark);

    if (!ENQUEUE(parser, parser->tokens, token))
        return 0;

    return 1;
}

/*
 * Produce the ALIAS or ANCHOR token.
 */

static int
yaml_parser_fetch_anchor(yaml_parser_t *parser, yaml_token_type_t type)
{
    /* An anchor or an alias could be a simple key. */

    if (!yaml_parser_save_simple_key(parser))
        return 0;

    /* A simple key cannot follow an anchor or an alias. */

    parser->simple_key_allowed = 0;

    /* Reserve queue space and scan directly into the tail slot. */

    if (!ENQUEUE_RESERVE(parser, parser->tokens))
        return 0;

    if (!yaml_parser_scan_anchor(parser, parser->tokens.tail, type))
        return 0;

    ENQUEUE_COMMIT(parser->tokens);
    return 1;
}

/*
 * Produce the TAG token.
 */

static int
yaml_parser_fetch_tag(yaml_parser_t *parser)
{
    /* A tag could be a simple key. */

    if (!yaml_parser_save_simple_key(parser))
        return 0;

    /* A simple key cannot follow a tag. */

    parser->simple_key_allowed = 0;

    /* Reserve queue space and scan directly into the tail slot. */

    if (!ENQUEUE_RESERVE(parser, parser->tokens))
        return 0;

    if (!yaml_parser_scan_tag(parser, parser->tokens.tail))
        return 0;

    ENQUEUE_COMMIT(parser->tokens);

    return 1;
}

/*
 * Produce the SCALAR(...,literal) or SCALAR(...,folded) tokens.
 */

static int
yaml_parser_fetch_block_scalar(yaml_parser_t *parser, int literal)
{
    /* Remove any potential simple keys. */

    if (!yaml_parser_remove_simple_key(parser))
        return 0;

    /* A simple key may follow a block scalar. */

    parser->simple_key_allowed = 1;

    /* Reserve queue space and scan directly into the tail slot. */

    if (!ENQUEUE_RESERVE(parser, parser->tokens))
        return 0;

    if (!yaml_parser_scan_block_scalar(parser, parser->tokens.tail, literal))
        return 0;

    ENQUEUE_COMMIT(parser->tokens);

    return 1;
}

/*
 * Produce the SCALAR(...,single-quoted) or SCALAR(...,double-quoted) tokens.
 */

static int
yaml_parser_fetch_flow_scalar(yaml_parser_t *parser, int single)
{
    /* A plain scalar could be a simple key. */

    if (!yaml_parser_save_simple_key(parser))
        return 0;

    /* A simple key cannot follow a flow scalar. */

    parser->simple_key_allowed = 0;

    /* Reserve queue space and scan directly into the tail slot. */

    if (!ENQUEUE_RESERVE(parser, parser->tokens))
        return 0;

    if (!yaml_parser_scan_flow_scalar(parser, parser->tokens.tail, single))
        return 0;

    ENQUEUE_COMMIT(parser->tokens);

    return 1;
}

/*
 * Produce the SCALAR(...,plain) token.
 */

static int
yaml_parser_fetch_plain_scalar(yaml_parser_t *parser)
{
    /* A plain scalar could be a simple key. */

    if (!yaml_parser_save_simple_key(parser))
        return 0;

    /* A simple key cannot follow a flow scalar. */

    parser->simple_key_allowed = 0;

    /* Reserve queue space and scan directly into the tail slot,
     * avoiding an 80-byte copy from a stack temporary. */

    if (!ENQUEUE_RESERVE(parser, parser->tokens))
        return 0;

    if (!yaml_parser_scan_plain_scalar(parser, parser->tokens.tail)) {
        return 0;
    }

    ENQUEUE_COMMIT(parser->tokens);

    return 1;
}

/*
 * Eat whitespaces and comments until the next token is found.
 */

static int
yaml_parser_scan_to_next_token(yaml_parser_t *parser)
{
    int allow_tabs = parser->flow_level || !parser->simple_key_allowed;

    /*
     * Fast path: skip whitespace (spaces, tabs, line breaks) when the
     * buffer already has data and we're not at column 0 (no BOM to check).
     * Handles the common case of "key: value\nkey2: value2" where we
     * need to skip spaces, a single newline, and then more spaces.
     */
    if (__builtin_expect(parser->unread >= 2 && parser->mark.column > 0, 1)) {
        yaml_char_t *ptr = parser->buffer.pointer;
        yaml_char_t *limit = ptr + parser->unread;
        /* Skip spaces (and tabs where allowed) */
        if (allow_tabs) {
            while (ptr < limit && (*ptr == ' ' || *ptr == '\t'))
                ptr++;
        } else {
            while (ptr < limit && *ptr == ' ')
                ptr++;
        }
        if (ptr > parser->buffer.pointer) {
            size_t count = (size_t)(ptr - parser->buffer.pointer);
            parser->buffer.pointer = ptr;
            parser->mark.index += count;
            parser->mark.column += count;
            parser->unread -= count;
        }
        /* If the next character is not a comment or line break, we're done */
        if (parser->unread > 0) {
            yaml_char_t ch = *parser->buffer.pointer;
            if (ch != '#' && ch != '\n' && ch != '\r'
                    && ch != '\0' && (ch & 0x80) == 0) {
                /* No BOM, comment, line break, or non-ASCII ahead -- done */
                return 1;
            }
            /*
             * Fast path for line break in block context: skip a single
             * LF/CR/CRLF, set simple_key_allowed, skip leading spaces,
             * and check the next char.  Avoids the full slow-path loop
             * for the common "value\nkey" pattern.
             */
            if (!parser->flow_level && (ch == '\n' || ch == '\r')
                    && parser->unread >= 3) {
                ptr = parser->buffer.pointer;
                limit = ptr + parser->unread;
                /* Skip the line break */
                size_t lb_len = 1;
                if (*ptr == '\r' && ptr + 1 < limit && ptr[1] == '\n')
                    lb_len = 2;
                ptr += lb_len;
                parser->buffer.pointer = ptr;
                parser->mark.index += lb_len;
                parser->mark.line ++;
                parser->mark.column = 0;
                parser->unread -= lb_len;
                /* A new line may start a simple key in block context */
                parser->simple_key_allowed = 1;
                allow_tabs = 0; /* block context, simple_key_allowed=1 */
                /* Skip leading spaces on the new line */
                limit = ptr + parser->unread;
                while (ptr < limit && *ptr == ' ')
                    ptr++;
                if (ptr > parser->buffer.pointer) {
                    size_t count = (size_t)(ptr - parser->buffer.pointer);
                    parser->buffer.pointer = ptr;
                    parser->mark.index += count;
                    parser->mark.column += count;
                    parser->unread -= count;
                }
                /* Check next char: if it's a normal token start, done */
                if (parser->unread > 0) {
                    ch = *parser->buffer.pointer;
                    if (ch != '#' && ch != '\n' && ch != '\r'
                            && ch != '\0' && (ch & 0x80) == 0) {
                        return 1;
                    }
                }
                /* Multiple line breaks or other cases: fall through
                 * to the slow path */
            }
        }
    }

    /* Until the next token is not found. */

    while (1)
    {
        /* Allow the BOM mark to start a line. */

        if (!CACHE(parser, 1)) return 0;

        if (parser->mark.column == 0 && IS_BOM(parser->buffer)) {
            SKIP(parser);
            if (!CACHE(parser, 1)) return 0;
        }

        /*
         * Eat whitespaces: batch-skip all leading spaces and tabs
         * (where tabs are allowed). Repeat after buffer refills to
         * handle whitespace runs that span multiple buffer loads.
         */

        while (1) {
            yaml_char_t *ptr = parser->buffer.pointer;
            yaml_char_t *limit = ptr + parser->unread;
            if (allow_tabs) {
                while (ptr < limit && (*ptr == ' ' || *ptr == '\t')) {
                    ptr++;
                }
            } else {
                while (ptr < limit && *ptr == ' ') {
                    ptr++;
                }
            }
            if (ptr == parser->buffer.pointer) break;
            size_t count = (size_t)(ptr - parser->buffer.pointer);
            parser->buffer.pointer = ptr;
            parser->mark.index += count;
            parser->mark.column += count;
            parser->unread -= count;
            if (parser->unread == 0) {
                if (!CACHE(parser, 1)) return 0;
                continue; /* Buffer refilled, check for more whitespace */
            }
            break; /* Stopped within buffer, not all whitespace */
        }

        /* Eat a comment until a line break. */

        if (CHECK(parser->buffer, '#')) {
            /* Batch-skip ASCII comment content using SIMD when available.
             * Stop at any non-ASCII byte (>= 0x80) so that
             * unread (which counts characters) is decremented
             * correctly -- within the ASCII region, bytes == chars.
             */
            {
                yaml_char_t *ptr = parser->buffer.pointer;
                size_t avail = parser->unread;
                size_t n;

#ifdef HAVE_SIMD_SCAN
                n = yaml_scan_block_scalar_sse2(ptr, avail);
#else
                {
                    yaml_char_t *p = ptr;
                    yaml_char_t *limit = ptr + avail;
                    while (p < limit) {
                        unsigned char ch = *p;
                        if (ch == '\r' || ch == '\n' || ch == '\0'
                                || (ch & 0x80))
                            break;
                        p++;
                    }
                    n = (size_t)(p - ptr);
                }
#endif
                if (n > 0) {
                    parser->buffer.pointer += n;
                    parser->mark.index += n;
                    parser->mark.column += n;
                    parser->unread -= n;
                }
            }
            if (!CACHE(parser, 1)) return 0;
            while (!IS_BREAKZ(parser->buffer)) {
                SKIP(parser);
                if (!CACHE(parser, 1)) return 0;
            }
        }

        /* If it is a line break, eat it. */

        if (IS_BREAK(parser->buffer))
        {
            if (!CACHE(parser, 2)) return 0;
            SKIP_LINE(parser);

            /* In the block context, a new line may start a simple key. */

            if (!parser->flow_level) {
                parser->simple_key_allowed = 1;
            }

            /* Update tab allowance after line break */
            allow_tabs = parser->flow_level || !parser->simple_key_allowed;
        }
        else
        {
            /* We have found a token. */

            break;
        }
    }

    return 1;
}

/*
 * Scan a YAML-DIRECTIVE or TAG-DIRECTIVE token.
 *
 * Scope:
 *      %YAML    1.1    # a comment \n
 *      ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
 *      %TAG    !yaml!  tag:yaml.org,2002:  \n
 *      ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
 */

int
yaml_parser_scan_directive(yaml_parser_t *parser, yaml_token_t *token)
{
    yaml_mark_t start_mark, end_mark;
    yaml_char_t *name = NULL;
    int major, minor;
    yaml_char_t *handle = NULL, *prefix = NULL;

    /* Eat '%'. */

    start_mark = parser->mark;

    SKIP(parser);

    /* Scan the directive name. */

    if (!yaml_parser_scan_directive_name(parser, start_mark, &name))
        goto error;

    /* Is it a YAML directive? */

    if (strcmp((char *)name, "YAML") == 0)
    {
        /* Scan the VERSION directive value. */

        if (!yaml_parser_scan_version_directive_value(parser, start_mark,
                    &major, &minor))
            goto error;

        end_mark = parser->mark;

        /* Create a VERSION-DIRECTIVE token. */

        VERSION_DIRECTIVE_TOKEN_INIT(*token, major, minor,
                start_mark, end_mark);
    }

    /* Is it a TAG directive? */

    else if (strcmp((char *)name, "TAG") == 0)
    {
        /* Scan the TAG directive value. */

        if (!yaml_parser_scan_tag_directive_value(parser, start_mark,
                    &handle, &prefix))
            goto error;

        end_mark = parser->mark;

        /* Create a TAG-DIRECTIVE token. */

        TAG_DIRECTIVE_TOKEN_INIT(*token, handle, prefix,
                start_mark, end_mark);
    }

    /* Unknown directive. */

    else
    {
        yaml_parser_set_scanner_error(parser, "while scanning a directive",
                start_mark, "found unknown directive name");
        goto error;
    }

    /* Eat the rest of the line including any comments. */

    if (!CACHE(parser, 1)) goto error;

    while (IS_BLANK(parser->buffer)) {
        SKIP(parser);
        if (!CACHE(parser, 1)) goto error;
    }

    if (CHECK(parser->buffer, '#')) {
        while (!IS_BREAKZ(parser->buffer)) {
            SKIP(parser);
            if (!CACHE(parser, 1)) goto error;
        }
    }

    /* Check if we are at the end of the line. */

    if (!IS_BREAKZ(parser->buffer)) {
        yaml_parser_set_scanner_error(parser, "while scanning a directive",
                start_mark, "did not find expected comment or line break");
        goto error;
    }

    /* Eat a line break. */

    if (IS_BREAK(parser->buffer)) {
        if (!CACHE(parser, 2)) goto error;
        SKIP_LINE(parser);
    }

    yaml_free(name);

    return 1;

error:
    yaml_free(prefix);
    yaml_free(handle);
    yaml_free(name);
    return 0;
}

/*
 * Scan the directive name.
 *
 * Scope:
 *      %YAML   1.1     # a comment \n
 *       ^^^^
 *      %TAG    !yaml!  tag:yaml.org,2002:  \n
 *       ^^^
 */

static int
yaml_parser_scan_directive_name(yaml_parser_t *parser,
        yaml_mark_t start_mark, yaml_char_t **name)
{
    yaml_string_t string = NULL_STRING;

    if (!STRING_INIT(parser, string, INITIAL_STRING_SIZE)) goto error;

    /* Consume the directive name. */

    if (!CACHE(parser, 1)) goto error;

    while (IS_ALPHA(parser->buffer))
    {
        if (!READ(parser, string)) goto error;
        if (!CACHE(parser, 1)) goto error;
    }

    /* Check if the name is empty. */

    if (string.start == string.pointer) {
        yaml_parser_set_scanner_error(parser, "while scanning a directive",
                start_mark, "could not find expected directive name");
        goto error;
    }

    /* Check for an blank character after the name. */

    if (!IS_BLANKZ(parser->buffer)) {
        yaml_parser_set_scanner_error(parser, "while scanning a directive",
                start_mark, "found unexpected non-alphabetical character");
        goto error;
    }

    *string.pointer = '\0';
    *name = string.start;

    return 1;

error:
    STRING_DEL(parser, string);
    return 0;
}

/*
 * Scan the value of VERSION-DIRECTIVE.
 *
 * Scope:
 *      %YAML   1.1     # a comment \n
 *           ^^^^^^
 */

static int
yaml_parser_scan_version_directive_value(yaml_parser_t *parser,
        yaml_mark_t start_mark, int *major, int *minor)
{
    /* Eat whitespaces. */

    if (!CACHE(parser, 1)) return 0;

    while (IS_BLANK(parser->buffer)) {
        SKIP(parser);
        if (!CACHE(parser, 1)) return 0;
    }

    /* Consume the major version number. */

    if (!yaml_parser_scan_version_directive_number(parser, start_mark, major))
        return 0;

    /* Eat '.'. */

    if (!CHECK(parser->buffer, '.')) {
        return yaml_parser_set_scanner_error(parser, "while scanning a %YAML directive",
                start_mark, "did not find expected digit or '.' character");
    }

    SKIP(parser);

    /* Consume the minor version number. */

    if (!yaml_parser_scan_version_directive_number(parser, start_mark, minor))
        return 0;

    return 1;
}

#define MAX_NUMBER_LENGTH   9

/*
 * Scan the version number of VERSION-DIRECTIVE.
 *
 * Scope:
 *      %YAML   1.1     # a comment \n
 *              ^
 *      %YAML   1.1     # a comment \n
 *                ^
 */

static int
yaml_parser_scan_version_directive_number(yaml_parser_t *parser,
        yaml_mark_t start_mark, int *number)
{
    int value = 0;
    size_t length = 0;

    /* Repeat while the next character is digit. */

    if (!CACHE(parser, 1)) return 0;

    while (IS_DIGIT(parser->buffer))
    {
        /* Check if the number is too long. */

        if (++length > MAX_NUMBER_LENGTH) {
            return yaml_parser_set_scanner_error(parser, "while scanning a %YAML directive",
                    start_mark, "found extremely long version number");
        }

        value = value*10 + AS_DIGIT(parser->buffer);

        SKIP(parser);

        if (!CACHE(parser, 1)) return 0;
    }

    /* Check if the number was present. */

    if (!length) {
        return yaml_parser_set_scanner_error(parser, "while scanning a %YAML directive",
                start_mark, "did not find expected version number");
    }

    *number = value;

    return 1;
}

/*
 * Scan the value of a TAG-DIRECTIVE token.
 *
 * Scope:
 *      %TAG    !yaml!  tag:yaml.org,2002:  \n
 *          ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
 */

static int
yaml_parser_scan_tag_directive_value(yaml_parser_t *parser,
        yaml_mark_t start_mark, yaml_char_t **handle, yaml_char_t **prefix)
{
    yaml_char_t *handle_value = NULL;
    yaml_char_t *prefix_value = NULL;

    /* Eat whitespaces. */

    if (!CACHE(parser, 1)) goto error;

    while (IS_BLANK(parser->buffer)) {
        SKIP(parser);
        if (!CACHE(parser, 1)) goto error;
    }

    /* Scan a handle. */

    if (!yaml_parser_scan_tag_handle(parser, 1, start_mark, &handle_value))
        goto error;

    /* Expect a whitespace. */

    if (!CACHE(parser, 1)) goto error;

    if (!IS_BLANK(parser->buffer)) {
        yaml_parser_set_scanner_error(parser, "while scanning a %TAG directive",
                start_mark, "did not find expected whitespace");
        goto error;
    }

    /* Eat whitespaces. */

    while (IS_BLANK(parser->buffer)) {
        SKIP(parser);
        if (!CACHE(parser, 1)) goto error;
    }

    /* Scan a prefix. */

    if (!yaml_parser_scan_tag_uri(parser, 1, 1, NULL, start_mark, &prefix_value))
        goto error;

    /* Expect a whitespace or line break. */

    if (!CACHE(parser, 1)) goto error;

    if (!IS_BLANKZ(parser->buffer)) {
        yaml_parser_set_scanner_error(parser, "while scanning a %TAG directive",
                start_mark, "did not find expected whitespace or line break");
        goto error;
    }

    *handle = handle_value;
    *prefix = prefix_value;

    return 1;

error:
    yaml_free(handle_value);
    yaml_free(prefix_value);
    return 0;
}

static int
yaml_parser_scan_anchor(yaml_parser_t *parser, yaml_token_t *token,
        yaml_token_type_t type)
{
    int length = 0;
    yaml_mark_t start_mark, end_mark;
    yaml_string_t string = NULL_STRING;

    if (!STRING_INIT(parser, string, INITIAL_STRING_SIZE)) goto error;

    /* Eat the indicator character. */

    start_mark = parser->mark;

    SKIP(parser);

    /* Consume the value (batch-copy alphanumeric ASCII). */

    if (!CACHE(parser, 1)) goto error;

    while (IS_ALPHA(parser->buffer)) {
        /* Batch-copy anchor/alias characters */
        {
            yaml_char_t *src = parser->buffer.pointer;
            yaml_char_t *lim = src + parser->unread;
            yaml_char_t *start = src;
            while (src < lim) {
                unsigned char ch = *src;
                if ((ch >= '0' && ch <= '9')
                    || (ch >= 'A' && ch <= 'Z')
                    || (ch >= 'a' && ch <= 'z')
                    || ch == '_' || ch == '-') {
                    src++;
                } else {
                    break;
                }
            }
            if (src > start) {
                size_t n = (size_t)(src - start);
                while (string.pointer + n + 5 >= string.end) {
                    if (!yaml_string_extend(&string.start,
                            &string.pointer, &string.end)) {
                        parser->error = YAML_MEMORY_ERROR;
                        goto error;
                    }
                }
                memcpy(string.pointer, start, n);
                string.pointer += n;
                parser->buffer.pointer = src;
                parser->mark.index += n;
                parser->mark.column += n;
                parser->unread -= n;
                length += (int)n;
                if (parser->unread == 0) {
                    if (!CACHE(parser, 1)) goto error;
                }
                continue;
            }
        }
        if (!READ(parser, string)) goto error;
        if (!CACHE(parser, 1)) goto error;
        length ++;
    }

    end_mark = parser->mark;

    /*
     * Check if length of the anchor is greater than 0 and it is followed by
     * a whitespace character or one of the indicators:
     *
     *      '?', ':', ',', ']', '}', '%', '@', '`'.
     */

    if (!length || !(IS_BLANKZ(parser->buffer) || CHECK(parser->buffer, '?')
                || CHECK(parser->buffer, ':') || CHECK(parser->buffer, ',')
                || CHECK(parser->buffer, ']') || CHECK(parser->buffer, '}')
                || CHECK(parser->buffer, '%') || CHECK(parser->buffer, '@')
                || CHECK(parser->buffer, '`'))) {
        yaml_parser_set_scanner_error(parser, type == YAML_ANCHOR_TOKEN ?
                "while scanning an anchor" : "while scanning an alias", start_mark,
                "did not find expected alphabetic or numeric character");
        goto error;
    }

    /* Create a token. */

    *string.pointer = '\0';
    if (type == YAML_ANCHOR_TOKEN) {
        ANCHOR_TOKEN_INIT(*token, string.start, start_mark, end_mark);
    }
    else {
        ALIAS_TOKEN_INIT(*token, string.start, start_mark, end_mark);
    }

    return 1;

error:
    STRING_DEL(parser, string);
    return 0;
}

/*
 * Scan a TAG token.
 */

static int
yaml_parser_scan_tag(yaml_parser_t *parser, yaml_token_t *token)
{
    yaml_char_t *handle = NULL;
    yaml_char_t *suffix = NULL;
    yaml_mark_t start_mark, end_mark;

    start_mark = parser->mark;

    /* Check if the tag is in the canonical form. */

    if (!CACHE(parser, 2)) goto error;

    if (CHECK_AT(parser->buffer, '<', 1))
    {
        /* Set the handle to '' */

        handle = YAML_MALLOC(1);
        if (!handle) goto error;
        handle[0] = '\0';

        /* Eat '!<' */

        SKIP(parser);
        SKIP(parser);

        /* Consume the tag value. */

        if (!yaml_parser_scan_tag_uri(parser, 1, 0, NULL, start_mark, &suffix))
            goto error;

        /* Check for '>' and eat it. */

        if (!CHECK(parser->buffer, '>')) {
            yaml_parser_set_scanner_error(parser, "while scanning a tag",
                    start_mark, "did not find the expected '>'");
            goto error;
        }

        SKIP(parser);
    }
    else
    {
        /* The tag has either the '!suffix' or the '!handle!suffix' form. */

        /* First, try to scan a handle. */

        if (!yaml_parser_scan_tag_handle(parser, 0, start_mark, &handle))
            goto error;

        /* Check if it is, indeed, handle. */

        if (handle[0] == '!' && handle[1] != '\0' && handle[strlen((char *)handle)-1] == '!')
        {
            /* Scan the suffix now. */

            if (!yaml_parser_scan_tag_uri(parser, 0, 0, NULL, start_mark, &suffix))
                goto error;
        }
        else
        {
            /* It wasn't a handle after all.  Scan the rest of the tag. */

            if (!yaml_parser_scan_tag_uri(parser, 0, 0, handle, start_mark, &suffix))
                goto error;

            /* Set the handle to '!'. */

            yaml_free(handle);
            handle = YAML_MALLOC(2);
            if (!handle) goto error;
            handle[0] = '!';
            handle[1] = '\0';

            /*
             * A special case: the '!' tag.  Set the handle to '' and the
             * suffix to '!'.
             */

            if (suffix[0] == '\0') {
                yaml_char_t *tmp = handle;
                handle = suffix;
                suffix = tmp;
            }
        }
    }

    /* Check the character which ends the tag. */

    if (!CACHE(parser, 1)) goto error;

    if (!IS_BLANKZ(parser->buffer)) {
        if (!parser->flow_level || !CHECK(parser->buffer, ',') ) {
            yaml_parser_set_scanner_error(parser, "while scanning a tag",
                    start_mark, "did not find expected whitespace or line break");
            goto error;
        }
    }

    end_mark = parser->mark;

    /* Create a token. */

    TAG_TOKEN_INIT(*token, handle, suffix, start_mark, end_mark);

    return 1;

error:
    yaml_free(handle);
    yaml_free(suffix);
    return 0;
}

/*
 * Scan a tag handle.
 */

static int
yaml_parser_scan_tag_handle(yaml_parser_t *parser, int directive,
        yaml_mark_t start_mark, yaml_char_t **handle)
{
    yaml_string_t string = NULL_STRING;

    if (!STRING_INIT(parser, string, INITIAL_STRING_SIZE)) goto error;

    /* Check the initial '!' character. */

    if (!CACHE(parser, 1)) goto error;

    if (!CHECK(parser->buffer, '!')) {
        yaml_parser_set_scanner_error(parser, directive ?
                "while scanning a tag directive" : "while scanning a tag",
                start_mark, "did not find expected '!'");
        goto error;
    }

    /* Copy the '!' character. */

    if (!READ(parser, string)) goto error;

    /* Copy all subsequent alphabetical and numerical characters. */

    if (!CACHE(parser, 1)) goto error;

    while (IS_ALPHA(parser->buffer))
    {
        if (!READ(parser, string)) goto error;
        if (!CACHE(parser, 1)) goto error;
    }

    /* Check if the trailing character is '!' and copy it. */

    if (CHECK(parser->buffer, '!'))
    {
        if (!READ(parser, string)) goto error;
    }
    else
    {
        /*
         * It's either the '!' tag or not really a tag handle.  If it's a %TAG
         * directive, it's an error.  If it's a tag token, it must be a part of
         * URI.
         */

        *string.pointer = '\0';
        if (directive && !(string.start[0] == '!' && string.start[1] == '\0')) {
            yaml_parser_set_scanner_error(parser, "while parsing a tag directive",
                    start_mark, "did not find expected '!'");
            goto error;
        }
    }

    *string.pointer = '\0';
    *handle = string.start;

    return 1;

error:
    STRING_DEL(parser, string);
    return 0;
}

/*
 * Scan a tag.
 */

static int
yaml_parser_scan_tag_uri(yaml_parser_t *parser, int uri_char, int directive,
        yaml_char_t *head, yaml_mark_t start_mark, yaml_char_t **uri)
{
    size_t length = head ? strlen((char *)head) : 0;
    yaml_string_t string = NULL_STRING;

    if (!STRING_INIT(parser, string, INITIAL_STRING_SIZE)) goto error;

    /* Resize the string to include the head. */

    while ((size_t)(string.end - string.start) <= length) {
        if (!yaml_string_extend(&string.start, &string.pointer, &string.end)) {
            parser->error = YAML_MEMORY_ERROR;
            goto error;
        }
    }

    /*
     * Copy the head if needed.
     *
     * Note that we don't copy the leading '!' character.
     */

    if (length > 1) {
        memcpy(string.start, head+1, length-1);
        string.pointer += length-1;
    }

    /* Scan the tag. */

    if (!CACHE(parser, 1)) goto error;

    /*
     * The set of characters that may appear in URI is as follows:
     *
     *      '0'-'9', 'A'-'Z', 'a'-'z', '_', '-', ';', '/', '?', ':', '@', '&',
     *      '=', '+', '$', '.', '!', '~', '*', '\'', '(', ')', '%'.
     *
     * If we are inside a verbatim tag <...> (parameter uri_char is true)
     * then also the following flow indicators are allowed:
     *      ',', '[', ']'
     */

    while (IS_ALPHA(parser->buffer) || CHECK(parser->buffer, ';')
            || CHECK(parser->buffer, '/') || CHECK(parser->buffer, '?')
            || CHECK(parser->buffer, ':') || CHECK(parser->buffer, '@')
            || CHECK(parser->buffer, '&') || CHECK(parser->buffer, '=')
            || CHECK(parser->buffer, '+') || CHECK(parser->buffer, '$')
            || CHECK(parser->buffer, '.') || CHECK(parser->buffer, '%')
            || CHECK(parser->buffer, '!') || CHECK(parser->buffer, '~')
            || CHECK(parser->buffer, '*') || CHECK(parser->buffer, '\'')
            || CHECK(parser->buffer, '(') || CHECK(parser->buffer, ')')
            || (uri_char && (
                CHECK(parser->buffer, ',')
                || CHECK(parser->buffer, '[') || CHECK(parser->buffer, ']')
                )
            ))
    {
        /* Check if it is a URI-escape sequence. */

        if (CHECK(parser->buffer, '%')) {
            if (!STRING_EXTEND(parser, string))
                goto error;

            if (!yaml_parser_scan_uri_escapes(parser,
                        directive, start_mark, &string)) goto error;
        }
        else {
            if (!READ(parser, string)) goto error;
        }

        length ++;
        if (!CACHE(parser, 1)) goto error;
    }

    /* Check if the tag is non-empty. */

    if (!length) {
        if (!STRING_EXTEND(parser, string))
            goto error;

        yaml_parser_set_scanner_error(parser, directive ?
                "while parsing a %TAG directive" : "while parsing a tag",
                start_mark, "did not find expected tag URI");
        goto error;
    }

    *string.pointer = '\0';
    *uri = string.start;

    return 1;

error:
    STRING_DEL(parser, string);
    return 0;
}

/*
 * Decode an URI-escape sequence corresponding to a single UTF-8 character.
 */

static int
yaml_parser_scan_uri_escapes(yaml_parser_t *parser, int directive,
        yaml_mark_t start_mark, yaml_string_t *string)
{
    int width = 0;

    /* Decode the required number of characters. */

    do {

        unsigned char octet = 0;

        /* Check for a URI-escaped octet. */

        if (!CACHE(parser, 3)) return 0;

        if (!(CHECK(parser->buffer, '%')
                    && IS_HEX_AT(parser->buffer, 1)
                    && IS_HEX_AT(parser->buffer, 2))) {
            return yaml_parser_set_scanner_error(parser, directive ?
                    "while parsing a %TAG directive" : "while parsing a tag",
                    start_mark, "did not find URI escaped octet");
        }

        /* Get the octet. */

        octet = (AS_HEX_AT(parser->buffer, 1) << 4) + AS_HEX_AT(parser->buffer, 2);

        /* If it is the leading octet, determine the length of the UTF-8 sequence. */

        if (!width)
        {
            width = (octet & 0x80) == 0x00 ? 1 :
                    (octet & 0xE0) == 0xC0 ? 2 :
                    (octet & 0xF0) == 0xE0 ? 3 :
                    (octet & 0xF8) == 0xF0 ? 4 : 0;
            if (!width) {
                return yaml_parser_set_scanner_error(parser, directive ?
                        "while parsing a %TAG directive" : "while parsing a tag",
                        start_mark, "found an incorrect leading UTF-8 octet");
            }
        }
        else
        {
            /* Check if the trailing octet is correct. */

            if ((octet & 0xC0) != 0x80) {
                return yaml_parser_set_scanner_error(parser, directive ?
                        "while parsing a %TAG directive" : "while parsing a tag",
                        start_mark, "found an incorrect trailing UTF-8 octet");
            }
        }

        /* Copy the octet and move the pointers. */

        *(string->pointer++) = octet;
        SKIP(parser);
        SKIP(parser);
        SKIP(parser);

    } while (--width);

    return 1;
}

/*
 * Scan a block scalar.
 */

static int
yaml_parser_scan_block_scalar(yaml_parser_t *parser, yaml_token_t *token,
        int literal)
{
    yaml_mark_t start_mark;
    yaml_mark_t end_mark;
    yaml_string_t string = NULL_STRING;
    yaml_string_t leading_break = NULL_STRING;
    yaml_string_t trailing_breaks = NULL_STRING;
    int chomping = 0;
    int increment = 0;
    int indent = 0;
    int leading_blank = 0;
    int trailing_blank = 0;

    if (!STRING_INIT(parser, string, INITIAL_STRING_SIZE)) goto error;
    if (!STRING_INIT(parser, leading_break, INITIAL_STRING_SIZE)) goto error;
    if (!STRING_INIT(parser, trailing_breaks, INITIAL_STRING_SIZE)) goto error;

    /* Eat the indicator '|' or '>'. */

    start_mark = parser->mark;

    SKIP(parser);

    /* Scan the additional block scalar indicators. */

    if (!CACHE(parser, 1)) goto error;

    /* Check for a chomping indicator. */

    if (CHECK(parser->buffer, '+') || CHECK(parser->buffer, '-'))
    {
        /* Set the chomping method and eat the indicator. */

        chomping = CHECK(parser->buffer, '+') ? +1 : -1;

        SKIP(parser);

        /* Check for an indentation indicator. */

        if (!CACHE(parser, 1)) goto error;

        if (IS_DIGIT(parser->buffer))
        {
            /* Check that the indentation is greater than 0. */

            if (CHECK(parser->buffer, '0')) {
                yaml_parser_set_scanner_error(parser, "while scanning a block scalar",
                        start_mark, "found an indentation indicator equal to 0");
                goto error;
            }

            /* Get the indentation level and eat the indicator. */

            increment = AS_DIGIT(parser->buffer);

            SKIP(parser);
        }
    }

    /* Do the same as above, but in the opposite order. */

    else if (IS_DIGIT(parser->buffer))
    {
        if (CHECK(parser->buffer, '0')) {
            yaml_parser_set_scanner_error(parser, "while scanning a block scalar",
                    start_mark, "found an indentation indicator equal to 0");
            goto error;
        }

        increment = AS_DIGIT(parser->buffer);

        SKIP(parser);

        if (!CACHE(parser, 1)) goto error;

        if (CHECK(parser->buffer, '+') || CHECK(parser->buffer, '-')) {
            chomping = CHECK(parser->buffer, '+') ? +1 : -1;

            SKIP(parser);
        }
    }

    /* Eat whitespaces and comments to the end of the line. */

    if (!CACHE(parser, 1)) goto error;

    while (IS_BLANK(parser->buffer)) {
        SKIP(parser);
        if (!CACHE(parser, 1)) goto error;
    }

    if (CHECK(parser->buffer, '#')) {
        while (!IS_BREAKZ(parser->buffer)) {
            SKIP(parser);
            if (!CACHE(parser, 1)) goto error;
        }
    }

    /* Check if we are at the end of the line. */

    if (!IS_BREAKZ(parser->buffer)) {
        yaml_parser_set_scanner_error(parser, "while scanning a block scalar",
                start_mark, "did not find expected comment or line break");
        goto error;
    }

    /* Eat a line break. */

    if (IS_BREAK(parser->buffer)) {
        if (!CACHE(parser, 2)) goto error;
        SKIP_LINE(parser);
    }

    end_mark = parser->mark;

    /* Set the indentation level if it was specified. */

    if (increment) {
        indent = parser->indent >= 0 ? parser->indent+increment : increment;
    }

    /* Scan the leading line breaks and determine the indentation level if needed. */

    if (!yaml_parser_scan_block_scalar_breaks(parser, &indent, &trailing_breaks,
                start_mark, &end_mark)) goto error;

    /* Scan the block scalar content. */

    if (!CACHE(parser, 1)) goto error;

    while ((int)parser->mark.column == indent && !(IS_Z(parser->buffer)))
    {
        /*
         * We are at the beginning of a non-empty line.
         */

        /* Is it a trailing whitespace? */

        trailing_blank = IS_BLANK(parser->buffer);

        /* Check if we need to fold the leading line break. */

        if (!literal && (*leading_break.start == '\n')
                && !leading_blank && !trailing_blank)
        {
            /* Do we need to join the lines by space? */

            if (*trailing_breaks.start == '\0') {
                if (!STRING_EXTEND(parser, string)) goto error;
                *(string.pointer ++) = ' ';
            }

            CLEAR(parser, leading_break);
        }
        else {
            if (!JOIN(parser, string, leading_break)) goto error;
            CLEAR(parser, leading_break);
        }

        /* Append the remaining line breaks. */

        if (!JOIN(parser, string, trailing_breaks)) goto error;
        CLEAR(parser, trailing_breaks);

        /* Is it a leading whitespace? */

        leading_blank = IS_BLANK(parser->buffer);

        /* Consume the current line. */

        while (!IS_BREAKZ(parser->buffer)) {
            /* Batch copy ASCII bytes until line break.
             * Uses SIMD when available. */
            if (parser->unread >= 2) {
                yaml_char_t *src = parser->buffer.pointer;
                size_t avail = parser->unread;
                size_t n;

#ifdef HAVE_SIMD_SCAN
                n = yaml_scan_block_scalar_sse2(src, avail);
#else
                {
                    yaml_char_t *p = src;
                    yaml_char_t *lim = src + avail;
                    while (p < lim) {
                        unsigned char ch = *p;
                        if (ch == '\0' || ch == '\n' || ch == '\r'
                            || ch >= 0x80) break;
                        p++;
                    }
                    n = (size_t)(p - src);
                }
#endif
                if (n > 0) {
                    while (string.pointer + n + 5 >= string.end) {
                        if (!yaml_string_extend(&string.start,
                                &string.pointer, &string.end)) {
                            parser->error = YAML_MEMORY_ERROR;
                            goto error;
                        }
                    }
                    memcpy(string.pointer, src, n);
                    string.pointer += n;
                    parser->buffer.pointer += n;
                    parser->mark.index += n;
                    parser->mark.column += n;
                    parser->unread -= n;
                    if (!CACHE(parser, 1)) goto error;
                    continue;
                }
            }
            if (!READ(parser, string)) goto error;
            if (!CACHE(parser, 1)) goto error;
        }

        /* Consume the line break. */

        if (!CACHE(parser, 2)) goto error;

        if (!READ_LINE(parser, leading_break)) goto error;

        /* Eat the following indentation spaces and line breaks. */

        if (!yaml_parser_scan_block_scalar_breaks(parser,
                    &indent, &trailing_breaks, start_mark, &end_mark)) goto error;
    }

    /* Chomp the tail. */

    if (chomping != -1) {
        if (!JOIN(parser, string, leading_break)) goto error;
    }
    if (chomping == 1) {
        if (!JOIN(parser, string, trailing_breaks)) goto error;
    }

    /* Create a token. */

    *string.pointer = '\0';
    SCALAR_TOKEN_INIT(*token, string.start, string.pointer-string.start,
            literal ? YAML_LITERAL_SCALAR_STYLE : YAML_FOLDED_SCALAR_STYLE,
            start_mark, end_mark);

    STRING_DEL(parser, leading_break);
    STRING_DEL(parser, trailing_breaks);

    return 1;

error:
    STRING_DEL(parser, string);
    STRING_DEL(parser, leading_break);
    STRING_DEL(parser, trailing_breaks);

    return 0;
}

/*
 * Scan indentation spaces and line breaks for a block scalar.  Determine the
 * indentation level if needed.
 */

static int
yaml_parser_scan_block_scalar_breaks(yaml_parser_t *parser,
        int *indent, yaml_string_t *breaks,
        yaml_mark_t start_mark, yaml_mark_t *end_mark)
{
    int max_indent = 0;

    *end_mark = parser->mark;

    /* Eat the indentation spaces and line breaks. */

    while (1)
    {
        /* Eat the indentation spaces (batch skip). */

        if (!CACHE(parser, 1)) return 0;

        {
            int target_col = *indent;
            while ((!target_col || (int)parser->mark.column < target_col)
                    && IS_SPACE(parser->buffer))
            {
                /* Batch-skip spaces up to indent limit or buffer end */
                yaml_char_t *ptr = parser->buffer.pointer;
                yaml_char_t *limit = ptr + parser->unread;
                if (target_col) {
                    size_t max_skip = (size_t)(target_col - (int)parser->mark.column);
                    if ((size_t)(limit - ptr) > max_skip)
                        limit = ptr + max_skip;
                }
                while (ptr < limit && *ptr == ' ') ptr++;
                if (ptr > parser->buffer.pointer) {
                    size_t n = (size_t)(ptr - parser->buffer.pointer);
                    parser->buffer.pointer = ptr;
                    parser->mark.index += n;
                    parser->mark.column += n;
                    parser->unread -= n;
                }
                if (parser->unread == 0) {
                    if (!CACHE(parser, 1)) return 0;
                }
            }
        }

        if ((int)parser->mark.column > max_indent)
            max_indent = (int)parser->mark.column;

        /* Check for a tab character messing the indentation. */

        if ((!*indent || (int)parser->mark.column < *indent)
                && IS_TAB(parser->buffer)) {
            return yaml_parser_set_scanner_error(parser, "while scanning a block scalar",
                    start_mark, "found a tab character where an indentation space is expected");
        }

        /* Have we found a non-empty line? */

        if (!IS_BREAK(parser->buffer)) break;

        /* Consume the line break. */

        if (!CACHE(parser, 2)) return 0;
        if (!READ_LINE(parser, *breaks)) return 0;
        *end_mark = parser->mark;
    }

    /* Determine the indentation level if needed. */

    if (!*indent) {
        *indent = max_indent;
        if (*indent < parser->indent + 1)
            *indent = parser->indent + 1;
        if (*indent < 1)
            *indent = 1;
    }

   return 1;
}

/*
 * Scan a quoted scalar.
 */

static int
yaml_parser_scan_flow_scalar(yaml_parser_t *parser, yaml_token_t *token,
        int single)
{
    yaml_mark_t start_mark;
    yaml_mark_t end_mark;
    yaml_string_t string = NULL_STRING;
    yaml_string_t leading_break = NULL_STRING;
    yaml_string_t trailing_breaks = NULL_STRING;
    yaml_string_t whitespaces = NULL_STRING;
    int leading_blanks;

    /* Eat the left quote. */

    start_mark = parser->mark;

    SKIP(parser);

    /*
     * Fast path for simple quoted scalars: scan for the closing quote
     * in the buffer without escape sequences, line breaks, or non-ASCII.
     * For single-quoted strings: no '' escape pairs allowed.
     * For double-quoted strings: no backslash escapes allowed.
     */
    if (__builtin_expect(parser->unread >= 1, 1)) {
        yaml_char_t *src = parser->buffer.pointer;
        size_t avail = parser->unread;
        size_t n;
        yaml_char_t quote_ch = single ? '\'' : '"';

#ifdef HAVE_SIMD_SCAN
        n = yaml_scan_flow_scalar_sse2(src, avail, quote_ch);
#else
        {
            yaml_char_t *p = src;
            yaml_char_t *lim = src + avail;
            if (single) {
                while (p < lim) {
                    unsigned char ch = *p;
                    if (ch <= 0x20 || ch >= 0x80 || ch == '\'')
                        break;
                    p++;
                }
            } else {
                while (p < lim) {
                    unsigned char ch = *p;
                    if (ch <= 0x20 || ch >= 0x80 || ch == '"' || ch == '\\')
                        break;
                    p++;
                }
            }
            n = (size_t)(p - src);
        }
#endif
        /* Fast path: the scan stopped at the closing quote.
         * For single-quoted strings, we must see the char after the quote
         * to verify it's not a '' escape pair.  If the quote is at the
         * end of the buffer, fall through to the slow path which will
         * CACHE more data. */
        if (n < avail && src[n] == quote_ch
                && (!single || (n + 1 < avail && src[n + 1] != '\''))) {
            yaml_char_t *value = (yaml_char_t *)yaml_malloc_internal(n + 1);
            if (!value) {
                parser->error = YAML_MEMORY_ERROR;
                return 0;
            }
            memcpy(value, src, n);
            value[n] = '\0';

            parser->buffer.pointer += n + 1; /* skip content + closing quote */
            parser->mark.index += n + 1;
            parser->mark.column += n + 1;
            parser->unread -= n + 1;
            end_mark = parser->mark;

            SCALAR_TOKEN_INIT(*token, value, n,
                    single ? YAML_SINGLE_QUOTED_SCALAR_STYLE
                           : YAML_DOUBLE_QUOTED_SCALAR_STYLE,
                    start_mark, end_mark);
            return 1;
        }
    }

    if (!STRING_INIT(parser, string, INITIAL_STRING_SIZE)) goto error;
    /* Auxiliary buffers allocated lazily -- only needed for multiline scalars */

    /* Consume the content of the quoted scalar. */

    while (1)
    {
        /* Check that there are no document indicators at the beginning of the line. */

        if (!CACHE(parser, 4)) goto error;

        if (parser->mark.column == 0 &&
            ((CHECK_AT(parser->buffer, '-', 0) &&
              CHECK_AT(parser->buffer, '-', 1) &&
              CHECK_AT(parser->buffer, '-', 2)) ||
             (CHECK_AT(parser->buffer, '.', 0) &&
              CHECK_AT(parser->buffer, '.', 1) &&
              CHECK_AT(parser->buffer, '.', 2))) &&
            IS_BLANKZ_AT(parser->buffer, 3))
        {
            yaml_parser_set_scanner_error(parser, "while scanning a quoted scalar",
                    start_mark, "found unexpected document indicator");
            goto error;
        }

        /* Check for EOF. */

        if (IS_Z(parser->buffer)) {
            yaml_parser_set_scanner_error(parser, "while scanning a quoted scalar",
                    start_mark, "found unexpected end of stream");
            goto error;
        }

        /* Consume non-blank characters. */

        if (!CACHE(parser, 2)) goto error;

        leading_blanks = 0;

        while (!IS_BLANKZ(parser->buffer))
        {
            /* Check for an escaped single quote. */

            if (single && CHECK_AT(parser->buffer, '\'', 0)
                    && CHECK_AT(parser->buffer, '\'', 1))
            {
                if (!STRING_EXTEND(parser, string)) goto error;
                *(string.pointer++) = '\'';
                SKIP(parser);
                SKIP(parser);
            }

            /* Check for the right quote. */

            else if (CHECK(parser->buffer, single ? '\'' : '"'))
            {
                break;
            }

            /* Check for an escaped line break. */

            else if (!single && CHECK(parser->buffer, '\\')
                    && IS_BREAK_AT(parser->buffer, 1))
            {
                if (!CACHE(parser, 3)) goto error;
                SKIP(parser);
                SKIP_LINE(parser);
                leading_blanks = 1;
                break;
            }

            /* Check for an escape sequence. */

            else if (!single && CHECK(parser->buffer, '\\'))
            {
                size_t code_length = 0;

                if (!STRING_EXTEND(parser, string)) goto error;

                /* Check the escape character. */

                switch (parser->buffer.pointer[1])
                {
                    case '0':
                        *(string.pointer++) = '\0';
                        break;

                    case 'a':
                        *(string.pointer++) = '\x07';
                        break;

                    case 'b':
                        *(string.pointer++) = '\x08';
                        break;

                    case 't':
                    case '\t':
                        *(string.pointer++) = '\x09';
                        break;

                    case 'n':
                        *(string.pointer++) = '\x0A';
                        break;

                    case 'v':
                        *(string.pointer++) = '\x0B';
                        break;

                    case 'f':
                        *(string.pointer++) = '\x0C';
                        break;

                    case 'r':
                        *(string.pointer++) = '\x0D';
                        break;

                    case 'e':
                        *(string.pointer++) = '\x1B';
                        break;

                    case ' ':
                        *(string.pointer++) = '\x20';
                        break;

                    case '"':
                        *(string.pointer++) = '"';
                        break;

                    case '/':
                        *(string.pointer++) = '/';
                        break;

                    case '\\':
                        *(string.pointer++) = '\\';
                        break;

                    case 'N':   /* NEL (#x85) */
                        *(string.pointer++) = '\xC2';
                        *(string.pointer++) = '\x85';
                        break;

                    case '_':   /* #xA0 */
                        *(string.pointer++) = '\xC2';
                        *(string.pointer++) = '\xA0';
                        break;

                    case 'L':   /* LS (#x2028) */
                        *(string.pointer++) = '\xE2';
                        *(string.pointer++) = '\x80';
                        *(string.pointer++) = '\xA8';
                        break;

                    case 'P':   /* PS (#x2029) */
                        *(string.pointer++) = '\xE2';
                        *(string.pointer++) = '\x80';
                        *(string.pointer++) = '\xA9';
                        break;

                    case 'x':
                        code_length = 2;
                        break;

                    case 'u':
                        code_length = 4;
                        break;

                    case 'U':
                        code_length = 8;
                        break;

                    default:
                        yaml_parser_set_scanner_error(parser, "while parsing a quoted scalar",
                                start_mark, "found unknown escape character");
                        goto error;
                }

                SKIP(parser);
                SKIP(parser);

                /* Consume an arbitrary escape code. */

                if (code_length)
                {
                    unsigned int value = 0;
                    size_t k;

                    /* Scan the character value. */

                    if (!CACHE(parser, code_length)) goto error;

                    for (k = 0; k < code_length; k ++) {
                        if (!IS_HEX_AT(parser->buffer, k)) {
                            yaml_parser_set_scanner_error(parser, "while parsing a quoted scalar",
                                    start_mark, "did not find expected hexdecimal number");
                            goto error;
                        }
                        value = (value << 4) + AS_HEX_AT(parser->buffer, k);
                    }

                    /* Check the value and write the character. */

                    if ((value >= 0xD800 && value <= 0xDFFF) || value > 0x10FFFF) {
                        yaml_parser_set_scanner_error(parser, "while parsing a quoted scalar",
                                start_mark, "found invalid Unicode character escape code");
                        goto error;
                    }

                    if (value <= 0x7F) {
                        *(string.pointer++) = value;
                    }
                    else if (value <= 0x7FF) {
                        *(string.pointer++) = 0xC0 + (value >> 6);
                        *(string.pointer++) = 0x80 + (value & 0x3F);
                    }
                    else if (value <= 0xFFFF) {
                        *(string.pointer++) = 0xE0 + (value >> 12);
                        *(string.pointer++) = 0x80 + ((value >> 6) & 0x3F);
                        *(string.pointer++) = 0x80 + (value & 0x3F);
                    }
                    else {
                        *(string.pointer++) = 0xF0 + (value >> 18);
                        *(string.pointer++) = 0x80 + ((value >> 12) & 0x3F);
                        *(string.pointer++) = 0x80 + ((value >> 6) & 0x3F);
                        *(string.pointer++) = 0x80 + (value & 0x3F);
                    }

                    /* Advance the pointer. */

                    for (k = 0; k < code_length; k ++) {
                        SKIP(parser);
                    }
                }
            }

            else
            {
                /*
                 * Non-escaped non-blank character. Try batch copy for
                 * ASCII characters that cannot be special in this context.
                 * Uses SIMD when available.
                 */
                {
                    yaml_char_t *src = parser->buffer.pointer;
                    size_t avail = parser->unread - 1;
                    size_t n;

#ifdef HAVE_SIMD_SCAN
                    n = yaml_scan_flow_scalar_sse2(src, avail,
                            single ? '\'' : '"');
#else
                    {
                        yaml_char_t *p = src;
                        yaml_char_t *lim = src + avail;

                        if (single) {
                            while (p < lim) {
                                unsigned char ch = *p;
                                if (ch <= 0x20 || ch >= 0x80 || ch == '\'')
                                    break;
                                p++;
                            }
                        } else {
                            while (p < lim) {
                                unsigned char ch = *p;
                                if (ch <= 0x20 || ch >= 0x80
                                    || ch == '"' || ch == '\\')
                                    break;
                                p++;
                            }
                        }
                        n = (size_t)(p - src);
                    }
#endif

                    if (n > 0) {
                        while (string.pointer + n + 5 >= string.end) {
                            if (!yaml_string_extend(&string.start,
                                    &string.pointer, &string.end)) {
                                parser->error = YAML_MEMORY_ERROR;
                                goto error;
                            }
                        }
                        memcpy(string.pointer, src, n);
                        string.pointer += n;
                        parser->buffer.pointer += n;
                        parser->mark.index += n;
                        parser->mark.column += n;
                        parser->unread -= n;
                        if (!CACHE(parser, 2)) goto error;
                        continue;
                    }
                }

                if (!READ(parser, string)) goto error;
            }

            if (!CACHE(parser, 2)) goto error;
        }

        /* Check if we are at the end of the scalar. */

        /* Fix for crash uninitialized value crash
         * Credit for the bug and input is to OSS Fuzz
         * Credit for the fix to Alex Gaynor
         */
        if (!CACHE(parser, 1)) goto error;
        if (CHECK(parser->buffer, single ? '\'' : '"'))
            break;

        /* Consume blank characters. */

        if (!CACHE(parser, 1)) goto error;

        while (IS_BLANK(parser->buffer) || IS_BREAK(parser->buffer))
        {
            if (IS_BLANK(parser->buffer))
            {
                /* Consume a space or a tab character. */

                if (!leading_blanks) {
                    if (!STRING_INIT_LAZY(parser, whitespaces, INITIAL_STRING_SIZE)) goto error;
                    if (!READ(parser, whitespaces)) goto error;
                }
                else {
                    SKIP(parser);
                }
            }
            else
            {
                if (!CACHE(parser, 2)) goto error;

                /* Check if it is a first line break. */

                if (!leading_blanks)
                {
                    if (whitespaces.start) {
                        CLEAR(parser, whitespaces);
                    }
                    if (!STRING_INIT_LAZY(parser, leading_break, INITIAL_STRING_SIZE)) goto error;
                    if (!READ_LINE(parser, leading_break)) goto error;
                    leading_blanks = 1;
                }
                else
                {
                    if (!STRING_INIT_LAZY(parser, trailing_breaks, INITIAL_STRING_SIZE)) goto error;
                    if (!READ_LINE(parser, trailing_breaks)) goto error;
                }
            }
            if (!CACHE(parser, 1)) goto error;
        }

        /* Join the whitespaces or fold line breaks. */

        if (leading_blanks)
        {
            /* Do we need to fold line breaks? */

            if (!leading_break.start || leading_break.start[0] == '\0') {
                /* Escaped line break: no stored break content, just continue */
            }
            else if (leading_break.start[0] == '\n') {
                if (!trailing_breaks.start || trailing_breaks.start[0] == '\0') {
                    if (!STRING_EXTEND(parser, string)) goto error;
                    *(string.pointer++) = ' ';
                }
                else {
                    if (!JOIN(parser, string, trailing_breaks)) goto error;
                    CLEAR(parser, trailing_breaks);
                }
                CLEAR(parser, leading_break);
            }
            else {
                if (!JOIN(parser, string, leading_break)) goto error;
                if (trailing_breaks.start) {
                    if (!JOIN(parser, string, trailing_breaks)) goto error;
                    CLEAR(parser, trailing_breaks);
                }
                CLEAR(parser, leading_break);
            }

            leading_blanks = 0;
        }
        else
        {
            if (whitespaces.start) {
                if (!JOIN(parser, string, whitespaces)) goto error;
                CLEAR(parser, whitespaces);
            }
        }
    }

    /* Eat the right quote. */

    SKIP(parser);

    end_mark = parser->mark;

    /* Create a token. */

    *string.pointer = '\0';
    SCALAR_TOKEN_INIT(*token, string.start, string.pointer-string.start,
            single ? YAML_SINGLE_QUOTED_SCALAR_STYLE : YAML_DOUBLE_QUOTED_SCALAR_STYLE,
            start_mark, end_mark);

    STRING_DEL(parser, leading_break);
    STRING_DEL(parser, trailing_breaks);
    STRING_DEL(parser, whitespaces);

    return 1;

error:
    STRING_DEL(parser, string);
    STRING_DEL(parser, leading_break);
    STRING_DEL(parser, trailing_breaks);
    STRING_DEL(parser, whitespaces);

    return 0;
}

/*
 * Scan a plain scalar.
 */

static int
yaml_parser_scan_plain_scalar(yaml_parser_t *parser, yaml_token_t *token)
{
    yaml_mark_t start_mark;
    yaml_mark_t end_mark;
    yaml_string_t string = NULL_STRING;
    yaml_string_t leading_break = NULL_STRING;
    yaml_string_t trailing_breaks = NULL_STRING;
    yaml_string_t whitespaces = NULL_STRING;
    int leading_blanks = 0;
    int indent = parser->indent+1;

    start_mark = end_mark = parser->mark;

    /*
     * Fast path for single-line plain scalars: scan the decoded buffer
     * directly without allocating an intermediate string buffer.
     * This avoids STRING_INIT malloc and the incremental copy loop.
     */
    if (__builtin_expect(parser->unread >= 2, 1)) {
        yaml_char_t *src = parser->buffer.pointer;
        size_t avail = parser->unread - 1; /* reserve 1 for lookahead */
        size_t n;

#ifdef HAVE_SIMD_SCAN
        n = !parser->flow_level
            ? yaml_scan_plain_block_sse2(src, avail)
            : yaml_scan_plain_flow_sse2(src, avail);
#else
        {
            yaml_char_t *p = src;
            yaml_char_t *lim = src + avail;
            if (!parser->flow_level) {
                while (p < lim) {
                    unsigned char ch = *p;
                    if (ch <= 0x20 || ch == '#' || ch == ':' || ch >= 0x80)
                        break;
                    p++;
                }
            } else {
                while (p < lim) {
                    unsigned char ch = *p;
                    if (ch <= 0x20 || ch == '#' || ch == ':' || ch >= 0x80
                        || ch == ',' || ch == '[' || ch == ']'
                        || ch == '{' || ch == '}')
                        break;
                    p++;
                }
            }
            n = (size_t)(p - src);
        }
#endif
        /*
         * Check if the fast path applies:
         * - We found at least 1 character
         * - The character after the scalar is a definitive terminator:
         *   NUL (end of stream), colon+blank, or flow indicator
         * - Newlines are NOT definitive -- plain scalars can span lines
         * - Spaces/tabs are NOT terminators -- they are intra-scalar whitespace
         */
        if (n > 0) {
            yaml_char_t next_ch = src[n];
            int is_end = 0;

            if (next_ch == '\0') {
                is_end = 1;
            } else if ((next_ch == '\n' || next_ch == '\r') && !parser->flow_level) {
                /* Check if the next line has less indentation, ending the scalar.
                 * Skip past the line break (LF, CR, or CR+LF) and count spaces. */
                size_t skip = 1;
                if (next_ch == '\r' && n + skip < parser->unread
                        && src[n + skip] == '\n')
                    skip++;
                size_t next_col = 0;
                while (n + skip + next_col < parser->unread
                        && src[n + skip + next_col] == ' ')
                    next_col++;
                /* If we can see a non-space char and it's at lower indent, scalar ends */
                if (n + skip + next_col < parser->unread
                        && src[n + skip + next_col] != ' '
                        && (int)next_col < indent) {
                    is_end = 1;
                }
            } else if (next_ch == ':' && n + 1 < parser->unread
                    && (src[n+1] == ' ' || src[n+1] == '\t' || src[n+1] == '\n'
                        || src[n+1] == '\r' || src[n+1] == '\0')) {
                is_end = 1;
            } else if (parser->flow_level
                    && (next_ch == ',' || next_ch == '[' || next_ch == ']'
                        || next_ch == '{' || next_ch == '}')) {
                is_end = 1;
            }

            if (is_end) {
                /* Fast path: single-line scalar, all content in buffer */
                yaml_char_t *value = (yaml_char_t *)yaml_malloc_internal(n + 1);
                if (!value) {
                    parser->error = YAML_MEMORY_ERROR;
                    return 0;
                }
                memcpy(value, src, n);
                value[n] = '\0';

                parser->buffer.pointer += n;
                parser->mark.index += n;
                parser->mark.column += n;
                parser->unread -= n;
                end_mark = parser->mark;

                SCALAR_TOKEN_INIT(*token, value, n,
                        YAML_PLAIN_SCALAR_STYLE, start_mark, end_mark);
                return 1;
            }
        }
    }

    if (!STRING_INIT(parser, string, INITIAL_STRING_SIZE)) goto error;
    /* Auxiliary buffers allocated lazily -- only needed for multiline scalars */

    /* Consume the content of the plain scalar. */

    while (1)
    {
        /* Check for a document indicator (only at column 0). */

        if (parser->mark.column == 0) {
            if (!CACHE(parser, 4)) goto error;
            if (((CHECK_AT(parser->buffer, '-', 0) &&
                  CHECK_AT(parser->buffer, '-', 1) &&
                  CHECK_AT(parser->buffer, '-', 2)) ||
                 (CHECK_AT(parser->buffer, '.', 0) &&
                  CHECK_AT(parser->buffer, '.', 1) &&
                  CHECK_AT(parser->buffer, '.', 2))) &&
                IS_BLANKZ_AT(parser->buffer, 3)) break;
        } else {
            if (!CACHE(parser, 2)) goto error;
        }

        /* Check for a comment. */

        if (CHECK(parser->buffer, '#'))
            break;

        /* Consume non-blank characters. */

        while (!IS_BLANKZ(parser->buffer))
        {
            /* Check for "x:" + one of ',?[]{}' in the flow context. TODO: Fix the test "spec-08-13".
             * This is not completely according to the spec
             * See http://yaml.org/spec/1.1/#id907281 9.1.3. Plain
             */

            if (parser->flow_level
                    && CHECK(parser->buffer, ':')
                    && (
                        CHECK_AT(parser->buffer, ',', 1)
                        || CHECK_AT(parser->buffer, '?', 1)
                        || CHECK_AT(parser->buffer, '[', 1)
                        || CHECK_AT(parser->buffer, ']', 1)
                        || CHECK_AT(parser->buffer, '{', 1)
                        || CHECK_AT(parser->buffer, '}', 1)
                    )
                    ) {
                yaml_parser_set_scanner_error(parser, "while scanning a plain scalar",
                        start_mark, "found unexpected ':'");
                goto error;
            }

            /* Check for indicators that may end a plain scalar. */

            if ((CHECK(parser->buffer, ':') && IS_BLANKZ_AT(parser->buffer, 1))
                    || (parser->flow_level &&
                        (CHECK(parser->buffer, ',')
                         || CHECK(parser->buffer, '[')
                         || CHECK(parser->buffer, ']') || CHECK(parser->buffer, '{')
                         || CHECK(parser->buffer, '}'))))
                break;

            /* Check if we need to join whitespaces and breaks. */

            if (leading_blanks || (whitespaces.start && whitespaces.start != whitespaces.pointer))
            {
                if (leading_blanks)
                {
                    /* Do we need to fold line breaks? */

                    if (leading_break.start[0] == '\n') {
                        if (!trailing_breaks.start || trailing_breaks.start[0] == '\0') {
                            if (!STRING_EXTEND(parser, string)) goto error;
                            *(string.pointer++) = ' ';
                        }
                        else {
                            if (!JOIN(parser, string, trailing_breaks)) goto error;
                            CLEAR(parser, trailing_breaks);
                        }
                        CLEAR(parser, leading_break);
                    }
                    else {
                        if (!JOIN(parser, string, leading_break)) goto error;
                        if (trailing_breaks.start) {
                            if (!JOIN(parser, string, trailing_breaks)) goto error;
                            CLEAR(parser, trailing_breaks);
                        }
                        CLEAR(parser, leading_break);
                    }

                    leading_blanks = 0;
                }
                else
                {
                    if (!JOIN(parser, string, whitespaces)) goto error;
                    CLEAR(parser, whitespaces);
                }
            }

            /*
             * Fast path: batch-copy ASCII characters that cannot end
             * a plain scalar. Uses SIMD when available.
             */
            {
                yaml_char_t *src = parser->buffer.pointer;
                size_t avail = parser->unread - 1;
                size_t n;

#ifdef HAVE_SIMD_SCAN
                n = !parser->flow_level
                    ? yaml_scan_plain_block_sse2(src, avail)
                    : yaml_scan_plain_flow_sse2(src, avail);
#else
                {
                    yaml_char_t *p = src;
                    yaml_char_t *lim = src + avail;
                    if (!parser->flow_level) {
                        while (p < lim) {
                            unsigned char ch = *p;
                            if (ch <= 0x20 || ch == '#' || ch == ':' || ch >= 0x80)
                                break;
                            p++;
                        }
                    } else {
                        while (p < lim) {
                            unsigned char ch = *p;
                            if (ch <= 0x20 || ch == '#' || ch == ':' || ch >= 0x80
                                || ch == ',' || ch == '[' || ch == ']'
                                || ch == '{' || ch == '}')
                                break;
                            p++;
                        }
                    }
                    n = (size_t)(p - src);
                }
#endif

                if (n > 0) {
                    while (string.pointer + n + 5 >= string.end) {
                        if (!yaml_string_extend(&string.start,
                                &string.pointer, &string.end)) {
                            parser->error = YAML_MEMORY_ERROR;
                            goto error;
                        }
                    }
                    memcpy(string.pointer, src, n);
                    string.pointer += n;
                    parser->buffer.pointer += n;
                    parser->mark.index += n;
                    parser->mark.column += n;
                    parser->unread -= n;

                    end_mark = parser->mark;
                    if (!CACHE(parser, 2)) goto error;
                    continue;
                }
            }

            /* Copy a single character (non-ASCII or special). */

            if (!READ(parser, string)) goto error;

            end_mark = parser->mark;

            if (!CACHE(parser, 2)) goto error;
        }

        /* Is it the end? */

        if (!(IS_BLANK(parser->buffer) || IS_BREAK(parser->buffer)))
            break;

        /* Consume blank characters. */

        if (!CACHE(parser, 1)) goto error;

        while (IS_BLANK(parser->buffer) || IS_BREAK(parser->buffer))
        {
            if (IS_BLANK(parser->buffer))
            {
                /* Check for tab characters that abuse indentation. */

                if (leading_blanks && (int)parser->mark.column < indent
                        && IS_TAB(parser->buffer)) {
                    yaml_parser_set_scanner_error(parser, "while scanning a plain scalar",
                            start_mark, "found a tab character that violates indentation");
                    goto error;
                }

                /* Consume a space or a tab character. */

                if (!leading_blanks) {
                    if (!STRING_INIT_LAZY(parser, whitespaces, INITIAL_STRING_SIZE)) goto error;
                    if (!READ(parser, whitespaces)) goto error;
                }
                else {
                    SKIP(parser);
                }
            }
            else
            {
                if (!CACHE(parser, 2)) goto error;

                /* Check if it is a first line break. */

                if (!leading_blanks)
                {
                    if (whitespaces.start) {
                        CLEAR(parser, whitespaces);
                    }
                    if (!STRING_INIT_LAZY(parser, leading_break, INITIAL_STRING_SIZE)) goto error;
                    if (!READ_LINE(parser, leading_break)) goto error;
                    leading_blanks = 1;
                }
                else
                {
                    if (!STRING_INIT_LAZY(parser, trailing_breaks, INITIAL_STRING_SIZE)) goto error;
                    if (!READ_LINE(parser, trailing_breaks)) goto error;
                }
            }
            if (!CACHE(parser, 1)) goto error;
        }

        /* Check indentation level. */

        if (!parser->flow_level && (int)parser->mark.column < indent)
            break;
    }

    /* Create a token. */

    *string.pointer = '\0';
    SCALAR_TOKEN_INIT(*token, string.start, string.pointer-string.start,
            YAML_PLAIN_SCALAR_STYLE, start_mark, end_mark);

    /* Note that we change the 'simple_key_allowed' flag. */

    if (leading_blanks) {
        parser->simple_key_allowed = 1;
    }

    STRING_DEL(parser, leading_break);
    STRING_DEL(parser, trailing_breaks);
    STRING_DEL(parser, whitespaces);

    return 1;

error:
    STRING_DEL(parser, string);
    STRING_DEL(parser, leading_break);
    STRING_DEL(parser, trailing_breaks);
    STRING_DEL(parser, whitespaces);

    return 0;
}
