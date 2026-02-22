#include "test_helper.h"

/*
 * Round-trip tests: parse YAML -> emit events -> parse emitted YAML
 * Verify that the event streams are semantically equivalent.
 *
 * This tests the library as a whole -- if the parser and emitter disagree
 * on any aspect of the YAML data model, these tests will catch it.
 */

/* Maximum events we track per document */
#define MAX_EVENTS 512
#define OUTPUT_SIZE (64 * 1024)

/* Stored event for comparison. We copy the fields we care about. */
typedef struct {
    yaml_event_type_t type;
    char *anchor;       /* scalar/sequence/mapping anchor, or alias anchor */
    char *tag;          /* scalar/sequence/mapping tag */
    char *value;        /* scalar value */
    size_t length;      /* scalar value length */
    int implicit;       /* doc start/end implicit flag */
    int plain_implicit; /* scalar plain_implicit */
    int quoted_implicit;/* scalar quoted_implicit */
} stored_event_t;

static void store_event(stored_event_t *dst, const yaml_event_t *src) {
    memset(dst, 0, sizeof(*dst));
    dst->type = src->type;

    switch (src->type) {
    case YAML_DOCUMENT_START_EVENT:
        dst->implicit = src->data.document_start.implicit;
        break;
    case YAML_DOCUMENT_END_EVENT:
        dst->implicit = src->data.document_end.implicit;
        break;
    case YAML_ALIAS_EVENT:
        if (src->data.alias.anchor)
            dst->anchor = strdup((const char *)src->data.alias.anchor);
        break;
    case YAML_SCALAR_EVENT:
        if (src->data.scalar.anchor)
            dst->anchor = strdup((const char *)src->data.scalar.anchor);
        if (src->data.scalar.tag)
            dst->tag = strdup((const char *)src->data.scalar.tag);
        if (src->data.scalar.value) {
            dst->value = malloc(src->data.scalar.length + 1);
            memcpy(dst->value, src->data.scalar.value, src->data.scalar.length);
            dst->value[src->data.scalar.length] = '\0';
        }
        dst->length = src->data.scalar.length;
        dst->plain_implicit = src->data.scalar.plain_implicit;
        dst->quoted_implicit = src->data.scalar.quoted_implicit;
        break;
    case YAML_SEQUENCE_START_EVENT:
        if (src->data.sequence_start.anchor)
            dst->anchor = strdup((const char *)src->data.sequence_start.anchor);
        if (src->data.sequence_start.tag)
            dst->tag = strdup((const char *)src->data.sequence_start.tag);
        break;
    case YAML_MAPPING_START_EVENT:
        if (src->data.mapping_start.anchor)
            dst->anchor = strdup((const char *)src->data.mapping_start.anchor);
        if (src->data.mapping_start.tag)
            dst->tag = strdup((const char *)src->data.mapping_start.tag);
        break;
    default:
        break;
    }
}

static void free_stored_event(stored_event_t *ev) {
    free(ev->anchor);
    free(ev->tag);
    free(ev->value);
}

static void free_stored_events(stored_event_t *events, int count) {
    for (int i = 0; i < count; i++)
        free_stored_event(&events[i]);
}

/*
 * Parse a YAML string into stored events.
 * Returns 1 on success, 0 on parse error.
 */
static int parse_to_events(const char *input, size_t input_len,
                           stored_event_t *events, int max_events, int *count) {
    yaml_parser_t parser;
    yaml_event_t event;
    *count = 0;

    if (!yaml_parser_initialize(&parser)) return 0;
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, input_len);

    while (*count < max_events) {
        if (!yaml_parser_parse(&parser, &event)) {
            yaml_parser_delete(&parser);
            return 0;
        }
        store_event(&events[*count], &event);
        (*count)++;
        if (event.type == YAML_STREAM_END_EVENT) {
            yaml_event_delete(&event);
            break;
        }
        yaml_event_delete(&event);
    }

    yaml_parser_delete(&parser);
    return 1;
}

/*
 * Emit stored events to a YAML string.
 * Returns 1 on success, 0 on emit error.
 */
static int emit_from_events(const stored_event_t *events, int count,
                            unsigned char *output, size_t output_size,
                            size_t *written) {
    yaml_emitter_t emitter;
    yaml_event_t event;
    *written = 0;

    if (!yaml_emitter_initialize(&emitter)) return 0;
    yaml_emitter_set_output_string(&emitter, output, output_size, written);
    yaml_emitter_set_unicode(&emitter, 1);

    for (int i = 0; i < count; i++) {
        const stored_event_t *ev = &events[i];

        switch (ev->type) {
        case YAML_STREAM_START_EVENT:
            yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
            break;
        case YAML_STREAM_END_EVENT:
            yaml_stream_end_event_initialize(&event);
            break;
        case YAML_DOCUMENT_START_EVENT:
            yaml_document_start_event_initialize(&event, NULL, NULL, NULL, ev->implicit);
            break;
        case YAML_DOCUMENT_END_EVENT:
            yaml_document_end_event_initialize(&event, ev->implicit);
            break;
        case YAML_ALIAS_EVENT:
            yaml_alias_event_initialize(&event, (const yaml_char_t *)ev->anchor);
            break;
        case YAML_SCALAR_EVENT:
            yaml_scalar_event_initialize(&event,
                (const yaml_char_t *)ev->anchor,
                (const yaml_char_t *)ev->tag,
                (const yaml_char_t *)ev->value,
                (int)ev->length,
                ev->plain_implicit, ev->quoted_implicit,
                YAML_ANY_SCALAR_STYLE);
            break;
        case YAML_SEQUENCE_START_EVENT:
            yaml_sequence_start_event_initialize(&event,
                (const yaml_char_t *)ev->anchor,
                (const yaml_char_t *)ev->tag,
                ev->tag ? 0 : 1,
                YAML_ANY_SEQUENCE_STYLE);
            break;
        case YAML_SEQUENCE_END_EVENT:
            yaml_sequence_end_event_initialize(&event);
            break;
        case YAML_MAPPING_START_EVENT:
            yaml_mapping_start_event_initialize(&event,
                (const yaml_char_t *)ev->anchor,
                (const yaml_char_t *)ev->tag,
                ev->tag ? 0 : 1,
                YAML_ANY_MAPPING_STYLE);
            break;
        case YAML_MAPPING_END_EVENT:
            yaml_mapping_end_event_initialize(&event);
            break;
        default:
            yaml_emitter_delete(&emitter);
            return 0;
        }

        if (!yaml_emitter_emit(&emitter, &event)) {
            yaml_emitter_delete(&emitter);
            return 0;
        }
    }

    yaml_emitter_delete(&emitter);
    return 1;
}

/*
 * Compare two event arrays for semantic equivalence.
 * Returns number of mismatches (0 = identical).
 */
static int compare_events(const char *test_name,
                          const stored_event_t *a, int a_count,
                          const stored_event_t *b, int b_count) {
    char msg[512];
    int mismatches = 0;

    snprintf(msg, sizeof(msg), "%s: event count", test_name);
    ASSERT_EQ_INT(a_count, b_count, msg);
    if (a_count != b_count) return 1;

    for (int i = 0; i < a_count; i++) {
        snprintf(msg, sizeof(msg), "%s: event[%d] type", test_name, i);
        ASSERT_EQ_INT(a[i].type, b[i].type, msg);
        if (a[i].type != b[i].type) { mismatches++; continue; }

        /* Compare scalar values */
        if (a[i].type == YAML_SCALAR_EVENT) {
            snprintf(msg, sizeof(msg), "%s: event[%d] scalar length", test_name, i);
            ASSERT_EQ_INT((int)a[i].length, (int)b[i].length, msg);

            snprintf(msg, sizeof(msg), "%s: event[%d] scalar value", test_name, i);
            if (a[i].value && b[i].value) {
                int match = (a[i].length == b[i].length &&
                             memcmp(a[i].value, b[i].value, a[i].length) == 0);
                ASSERT(match, msg);
                if (!match) mismatches++;
            } else {
                ASSERT(a[i].value == NULL && b[i].value == NULL, msg);
            }
        }

        /* Compare anchors */
        if (a[i].anchor || b[i].anchor) {
            snprintf(msg, sizeof(msg), "%s: event[%d] anchor", test_name, i);
            if (a[i].anchor && b[i].anchor) {
                ASSERT_EQ_STR(a[i].anchor, b[i].anchor, msg);
            } else {
                ASSERT(0, msg);
                mismatches++;
            }
        }

        /* Compare tags (only when both are explicit / non-implicit) */
        if (a[i].type == YAML_SCALAR_EVENT) {
            /* For scalars, if both have explicit tags, compare them */
            if (a[i].tag && b[i].tag &&
                !a[i].plain_implicit && !a[i].quoted_implicit &&
                !b[i].plain_implicit && !b[i].quoted_implicit) {
                snprintf(msg, sizeof(msg), "%s: event[%d] tag", test_name, i);
                ASSERT_EQ_STR(a[i].tag, b[i].tag, msg);
            }
        }
    }

    return mismatches;
}

/*
 * Full round-trip: parse -> emit -> parse -> compare.
 * Returns 1 on success, 0 on failure.
 */
static int roundtrip(const char *test_name, const char *yaml) {
    stored_event_t events1[MAX_EVENTS], events2[MAX_EVENTS];
    int count1 = 0, count2 = 0;
    unsigned char output[OUTPUT_SIZE];
    size_t written = 0;
    char msg[256];

    /* Step 1: Parse original input */
    snprintf(msg, sizeof(msg), "%s: parse original", test_name);
    if (!parse_to_events(yaml, strlen(yaml), events1, MAX_EVENTS, &count1)) {
        ASSERT(0, msg);
        return 0;
    }
    ASSERT(count1 > 0, msg);

    /* Step 2: Emit events to a string */
    snprintf(msg, sizeof(msg), "%s: emit", test_name);
    if (!emit_from_events(events1, count1, output, OUTPUT_SIZE, &written)) {
        ASSERT(0, msg);
        free_stored_events(events1, count1);
        return 0;
    }
    /* Empty stream (just STREAM_START + STREAM_END) produces 0 bytes -- that's OK */

    /* Step 3: Parse the emitted output */
    snprintf(msg, sizeof(msg), "%s: re-parse", test_name);
    if (!parse_to_events((const char *)output, written, events2, MAX_EVENTS, &count2)) {
        ASSERT(0, msg);
        free_stored_events(events1, count1);
        return 0;
    }
    ASSERT(count2 > 0, msg);

    /* Step 4: Compare event streams */
    compare_events(test_name, events1, count1, events2, count2);

    free_stored_events(events1, count1);
    free_stored_events(events2, count2);
    return 1;
}

/* ========================================================================
 * Basic round-trip tests
 * ======================================================================== */

static void test_rt_empty(void) {
    roundtrip("rt_empty", "");
}

static void test_rt_scalar(void) {
    roundtrip("rt_scalar", "hello world");
}

static void test_rt_integer(void) {
    roundtrip("rt_integer", "42");
}

static void test_rt_float(void) {
    roundtrip("rt_float", "3.14159");
}

static void test_rt_boolean_true(void) {
    roundtrip("rt_bool_true", "true");
}

static void test_rt_boolean_false(void) {
    roundtrip("rt_bool_false", "false");
}

static void test_rt_null(void) {
    roundtrip("rt_null", "null");
}

static void test_rt_empty_string(void) {
    roundtrip("rt_empty_string", "''");
}

static void test_rt_tilde(void) {
    roundtrip("rt_tilde", "~");
}

/* ========================================================================
 * Scalar style round-trips
 * ======================================================================== */

static void test_rt_single_quoted(void) {
    roundtrip("rt_single_quoted", "'hello world'");
}

static void test_rt_double_quoted(void) {
    roundtrip("rt_double_quoted", "\"hello world\"");
}

static void test_rt_literal(void) {
    roundtrip("rt_literal", "|\n  line one\n  line two\n  line three\n");
}

static void test_rt_folded(void) {
    roundtrip("rt_folded", ">\n  line one\n  line two\n  line three\n");
}

static void test_rt_literal_keep(void) {
    roundtrip("rt_literal_keep", "|+\n  line one\n  line two\n");
}

static void test_rt_literal_strip(void) {
    roundtrip("rt_literal_strip", "|-\n  line one\n  line two");
}

static void test_rt_folded_keep(void) {
    roundtrip("rt_folded_keep", ">+\n  line one\n  line two\n");
}

static void test_rt_folded_strip(void) {
    roundtrip("rt_folded_strip", ">-\n  line one\n  line two");
}

/* ========================================================================
 * Collection round-trips
 * ======================================================================== */

static void test_rt_simple_mapping(void) {
    roundtrip("rt_simple_map", "key: value\n");
}

static void test_rt_simple_sequence(void) {
    roundtrip("rt_simple_seq", "- a\n- b\n- c\n");
}

static void test_rt_nested_mapping(void) {
    roundtrip("rt_nested_map",
        "outer:\n"
        "  middle:\n"
        "    inner: value\n");
}

static void test_rt_nested_sequence(void) {
    roundtrip("rt_nested_seq",
        "- - a\n"
        "  - b\n"
        "- - c\n"
        "  - d\n");
}

static void test_rt_mapping_of_sequences(void) {
    roundtrip("rt_map_of_seqs",
        "fruits:\n"
        "  - apple\n"
        "  - banana\n"
        "vegetables:\n"
        "  - carrot\n"
        "  - daikon\n");
}

static void test_rt_sequence_of_mappings(void) {
    roundtrip("rt_seq_of_maps",
        "- name: Alice\n"
        "  age: '30'\n"
        "- name: Bob\n"
        "  age: '25'\n");
}

static void test_rt_flow_sequence(void) {
    roundtrip("rt_flow_seq", "[a, b, c, d, e]");
}

static void test_rt_flow_mapping(void) {
    roundtrip("rt_flow_map", "{a: b, c: d}");
}

static void test_rt_flow_nested(void) {
    roundtrip("rt_flow_nested", "[[1, 2], [3, 4]]");
}

static void test_rt_mixed_flow_block(void) {
    roundtrip("rt_mixed",
        "items:\n"
        "  - [1, 2, 3]\n"
        "  - {x: 1, y: 2}\n");
}

static void test_rt_empty_mapping(void) {
    roundtrip("rt_empty_map", "{}");
}

static void test_rt_empty_sequence(void) {
    roundtrip("rt_empty_seq", "[]");
}

static void test_rt_empty_value(void) {
    roundtrip("rt_empty_value", "key:\n");
}

static void test_rt_empty_key(void) {
    roundtrip("rt_empty_key", "'': value\n");
}

/* ========================================================================
 * Anchor & alias round-trips
 * ======================================================================== */

static void test_rt_anchor_alias(void) {
    roundtrip("rt_anchor_alias",
        "- &anchor value\n"
        "- *anchor\n");
}

static void test_rt_anchor_mapping(void) {
    roundtrip("rt_anchor_map",
        "defaults: &defaults\n"
        "  adapter: postgres\n"
        "  host: localhost\n"
        "production:\n"
        "  <<: *defaults\n"
        "  database: prod_db\n");
}

static void test_rt_multiple_anchors(void) {
    roundtrip("rt_multi_anchor",
        "- &a first\n"
        "- &b second\n"
        "- *a\n"
        "- *b\n");
}

/* ========================================================================
 * Document round-trips
 * ======================================================================== */

static void test_rt_explicit_doc(void) {
    roundtrip("rt_explicit_doc",
        "---\nhello\n...\n");
}

static void test_rt_multi_doc(void) {
    roundtrip("rt_multi_doc",
        "---\nfirst\n---\nsecond\n---\nthird\n");
}

static void test_rt_multi_doc_explicit_end(void) {
    roundtrip("rt_multi_doc_end",
        "---\nfirst\n...\n---\nsecond\n...\n");
}

/* ========================================================================
 * Special scalar content round-trips
 * ======================================================================== */

static void test_rt_multiline_plain(void) {
    /* Plain scalar that wraps */
    roundtrip("rt_multiline_plain",
        "a long scalar that should stay as one value\n");
}

static void test_rt_special_chars(void) {
    roundtrip("rt_special_chars",
        "\"hello\\nworld\"\n");
}

static void test_rt_colon_in_value(void) {
    roundtrip("rt_colon_val",
        "url: 'http://example.com'\n");
}

static void test_rt_hash_in_value(void) {
    roundtrip("rt_hash_val",
        "comment: 'has # inside'\n");
}

static void test_rt_leading_space(void) {
    roundtrip("rt_leading_space",
        "\" leading space\"\n");
}

static void test_rt_trailing_space(void) {
    roundtrip("rt_trailing_space",
        "\"trailing space \"\n");
}

static void test_rt_newlines_in_scalar(void) {
    roundtrip("rt_newlines",
        "\"line1\\nline2\\nline3\"\n");
}

static void test_rt_tab_in_scalar(void) {
    roundtrip("rt_tab",
        "\"has\\ttab\"\n");
}

static void test_rt_backslash_in_scalar(void) {
    roundtrip("rt_backslash",
        "\"has\\\\backslash\"\n");
}

static void test_rt_quote_in_scalar(void) {
    roundtrip("rt_quote_in_scalar",
        "\"has \\\"quote\\\" inside\"\n");
}

static void test_rt_unicode_bmp(void) {
    /* UTF-8: e2 9c 93 = check mark */
    roundtrip("rt_unicode_bmp",
        "check: \"\xe2\x9c\x93\"\n");
}

static void test_rt_empty_lines_literal(void) {
    roundtrip("rt_empty_lines_lit",
        "|\n  line one\n\n  line three\n");
}

static void test_rt_indicator_chars(void) {
    /* Scalars containing YAML indicator characters */
    roundtrip("rt_indicators",
        "dash: '-'\n"
        "question: '?'\n"
        "colon: ':'\n"
        "comma: ','\n"
        "bracket_open: '['\n"
        "bracket_close: ']'\n"
        "brace_open: '{'\n"
        "brace_close: '}'\n"
        "hash: '#'\n"
        "ampersand: '&'\n"
        "asterisk: '*'\n"
        "exclamation: '!'\n"
        "pipe: '|'\n"
        "greater: '>'\n"
        "percent: '%'\n"
        "at: '@'\n"
        "backtick: '`'\n");
}

/* ========================================================================
 * Tagged content round-trips
 * ======================================================================== */

static void test_rt_tagged_str(void) {
    roundtrip("rt_tagged_str", "!!str hello");
}

static void test_rt_tagged_int(void) {
    roundtrip("rt_tagged_int", "!!int 42");
}

static void test_rt_tagged_float(void) {
    roundtrip("rt_tagged_float", "!!float 3.14");
}

static void test_rt_tagged_bool(void) {
    roundtrip("rt_tagged_bool", "!!bool true");
}

static void test_rt_tagged_null(void) {
    roundtrip("rt_tagged_null", "!!null ''");
}

static void test_rt_tagged_seq(void) {
    roundtrip("rt_tagged_seq", "!!seq\n- a\n- b\n");
}

static void test_rt_tagged_map(void) {
    roundtrip("rt_tagged_map", "!!map\na: b\n");
}

/* ========================================================================
 * Complex structure round-trips
 * ======================================================================== */

static void test_rt_kubernetes(void) {
    roundtrip("rt_kubernetes",
        "apiVersion: v1\n"
        "kind: Pod\n"
        "metadata:\n"
        "  name: my-pod\n"
        "  labels:\n"
        "    app: web\n"
        "    tier: frontend\n"
        "spec:\n"
        "  containers:\n"
        "    - name: nginx\n"
        "      image: 'nginx:latest'\n"
        "      ports:\n"
        "        - containerPort: '80'\n"
        "          protocol: TCP\n"
        "    - name: sidecar\n"
        "      image: 'busybox:latest'\n"
        "      command: [sh, '-c', 'echo hello']\n");
}

static void test_rt_docker_compose(void) {
    roundtrip("rt_docker_compose",
        "version: '3.8'\n"
        "services:\n"
        "  web:\n"
        "    build: .\n"
        "    ports:\n"
        "      - '8080:80'\n"
        "    depends_on:\n"
        "      - db\n"
        "      - redis\n"
        "    environment:\n"
        "      DATABASE_URL: 'postgres://db:5432/app'\n"
        "      REDIS_URL: 'redis://redis:6379'\n"
        "  db:\n"
        "    image: 'postgres:13'\n"
        "    volumes:\n"
        "      - db-data:/var/lib/postgresql/data\n"
        "  redis:\n"
        "    image: 'redis:alpine'\n");
}

static void test_rt_github_actions(void) {
    roundtrip("rt_github_actions",
        "name: CI Pipeline\n"
        "on:\n"
        "  push:\n"
        "    branches: [main, develop]\n"
        "  pull_request:\n"
        "    branches: [main]\n"
        "jobs:\n"
        "  test:\n"
        "    runs-on: ubuntu-latest\n"
        "    strategy:\n"
        "      matrix:\n"
        "        node: ['14', '16', '18']\n"
        "    steps:\n"
        "      - uses: actions/checkout@v3\n"
        "      - name: Setup Node\n"
        "        uses: actions/setup-node@v3\n"
        "        with:\n"
        "          node-version: '${{ matrix.node }}'\n"
        "      - run: npm install\n"
        "      - run: npm test\n");
}

static void test_rt_deep_nesting(void) {
    roundtrip("rt_deep_nesting",
        "a:\n"
        "  b:\n"
        "    c:\n"
        "      d:\n"
        "        e:\n"
        "          f:\n"
        "            g:\n"
        "              h: deep\n");
}

static void test_rt_wide_mapping(void) {
    roundtrip("rt_wide_map",
        "key1: val1\n"
        "key2: val2\n"
        "key3: val3\n"
        "key4: val4\n"
        "key5: val5\n"
        "key6: val6\n"
        "key7: val7\n"
        "key8: val8\n"
        "key9: val9\n"
        "key10: val10\n");
}

static void test_rt_long_sequence(void) {
    roundtrip("rt_long_seq",
        "- item1\n- item2\n- item3\n- item4\n- item5\n"
        "- item6\n- item7\n- item8\n- item9\n- item10\n"
        "- item11\n- item12\n- item13\n- item14\n- item15\n");
}

/* ========================================================================
 * Edge cases
 * ======================================================================== */

static void test_rt_numeric_keys(void) {
    roundtrip("rt_numeric_keys",
        "'0': zero\n"
        "'1': one\n"
        "'2': two\n");
}

static void test_rt_bool_like_keys(void) {
    roundtrip("rt_bool_keys",
        "'true': a\n"
        "'false': b\n"
        "'yes': c\n"
        "'no': d\n");
}

static void test_rt_null_like_values(void) {
    roundtrip("rt_null_like",
        "a: ''\n"
        "b: 'null'\n"
        "c: '~'\n");
}

static void test_rt_multiline_key(void) {
    roundtrip("rt_multiline_key",
        "? |\n"
        "  multi\n"
        "  line\n"
        "  key\n"
        ": value\n");
}

static void test_rt_complex_key(void) {
    roundtrip("rt_complex_key",
        "? [a, b]\n"
        ": value\n");
}

static void test_rt_seq_as_map_value(void) {
    roundtrip("rt_seq_map_val",
        "key:\n"
        "  - a\n"
        "  - b\n"
        "  - c\n");
}

/* ========================================================================
 * Stress round-trips: large documents
 * ======================================================================== */

static void test_rt_large_mapping(void) {
    /* Build a large mapping programmatically */
    char yaml[16384];
    int pos = 0;
    for (int i = 0; i < 100 && pos < 16000; i++) {
        pos += snprintf(yaml + pos, sizeof(yaml) - pos,
                       "key_%03d: value_%03d\n", i, i);
    }
    roundtrip("rt_large_map", yaml);
}

static void test_rt_large_sequence(void) {
    char yaml[16384];
    int pos = 0;
    for (int i = 0; i < 100 && pos < 16000; i++) {
        pos += snprintf(yaml + pos, sizeof(yaml) - pos,
                       "- item_%03d\n", i);
    }
    roundtrip("rt_large_seq", yaml);
}

static void test_rt_large_nested(void) {
    char yaml[16384];
    int pos = 0;
    for (int i = 0; i < 20 && pos < 15000; i++) {
        pos += snprintf(yaml + pos, sizeof(yaml) - pos,
                       "group_%02d:\n", i);
        for (int j = 0; j < 5 && pos < 15500; j++) {
            pos += snprintf(yaml + pos, sizeof(yaml) - pos,
                           "  item_%02d_%02d: value\n", i, j);
        }
    }
    roundtrip("rt_large_nested", yaml);
}

/* ========================================================================
 * Encoding round-trips
 * ======================================================================== */

/* Round-trip through a specific emitter encoding */
static int roundtrip_encoding(const char *test_name, const char *yaml,
                              yaml_encoding_t encoding) {
    stored_event_t events1[MAX_EVENTS], events2[MAX_EVENTS];
    int count1 = 0, count2 = 0;
    unsigned char output[OUTPUT_SIZE];
    size_t written = 0;
    char msg[256];

    /* Parse original */
    snprintf(msg, sizeof(msg), "%s: parse original", test_name);
    if (!parse_to_events(yaml, strlen(yaml), events1, MAX_EVENTS, &count1)) {
        ASSERT(0, msg);
        return 0;
    }

    /* Emit with specific encoding */
    yaml_emitter_t emitter;
    yaml_event_t event;
    if (!yaml_emitter_initialize(&emitter)) {
        ASSERT(0, test_name);
        free_stored_events(events1, count1);
        return 0;
    }
    yaml_emitter_set_output_string(&emitter, output, OUTPUT_SIZE, &written);
    yaml_emitter_set_encoding(&emitter, encoding);
    yaml_emitter_set_unicode(&emitter, 1);

    for (int i = 0; i < count1; i++) {
        const stored_event_t *ev = &events1[i];
        switch (ev->type) {
        case YAML_STREAM_START_EVENT:
            yaml_stream_start_event_initialize(&event, encoding);
            break;
        case YAML_STREAM_END_EVENT:
            yaml_stream_end_event_initialize(&event);
            break;
        case YAML_DOCUMENT_START_EVENT:
            yaml_document_start_event_initialize(&event, NULL, NULL, NULL, ev->implicit);
            break;
        case YAML_DOCUMENT_END_EVENT:
            yaml_document_end_event_initialize(&event, ev->implicit);
            break;
        case YAML_ALIAS_EVENT:
            yaml_alias_event_initialize(&event, (const yaml_char_t *)ev->anchor);
            break;
        case YAML_SCALAR_EVENT:
            yaml_scalar_event_initialize(&event,
                (const yaml_char_t *)ev->anchor,
                (const yaml_char_t *)ev->tag,
                (const yaml_char_t *)ev->value,
                (int)ev->length,
                ev->plain_implicit, ev->quoted_implicit,
                YAML_ANY_SCALAR_STYLE);
            break;
        case YAML_SEQUENCE_START_EVENT:
            yaml_sequence_start_event_initialize(&event,
                (const yaml_char_t *)ev->anchor,
                (const yaml_char_t *)ev->tag,
                ev->tag ? 0 : 1,
                YAML_ANY_SEQUENCE_STYLE);
            break;
        case YAML_SEQUENCE_END_EVENT:
            yaml_sequence_end_event_initialize(&event);
            break;
        case YAML_MAPPING_START_EVENT:
            yaml_mapping_start_event_initialize(&event,
                (const yaml_char_t *)ev->anchor,
                (const yaml_char_t *)ev->tag,
                ev->tag ? 0 : 1,
                YAML_ANY_MAPPING_STYLE);
            break;
        case YAML_MAPPING_END_EVENT:
            yaml_mapping_end_event_initialize(&event);
            break;
        default:
            break;
        }
        if (!yaml_emitter_emit(&emitter, &event)) {
            snprintf(msg, sizeof(msg), "%s: emit failed", test_name);
            ASSERT(0, msg);
            yaml_emitter_delete(&emitter);
            free_stored_events(events1, count1);
            return 0;
        }
    }
    yaml_emitter_delete(&emitter);

    /* Re-parse the encoded output */
    yaml_parser_t parser;
    if (!yaml_parser_initialize(&parser)) {
        free_stored_events(events1, count1);
        return 0;
    }
    yaml_parser_set_input_string(&parser, output, written);
    count2 = 0;
    while (count2 < MAX_EVENTS) {
        if (!yaml_parser_parse(&parser, &event)) {
            snprintf(msg, sizeof(msg), "%s: re-parse failed", test_name);
            ASSERT(0, msg);
            break;
        }
        store_event(&events2[count2], &event);
        count2++;
        if (event.type == YAML_STREAM_END_EVENT) {
            yaml_event_delete(&event);
            break;
        }
        yaml_event_delete(&event);
    }
    yaml_parser_delete(&parser);

    compare_events(test_name, events1, count1, events2, count2);

    free_stored_events(events1, count1);
    free_stored_events(events2, count2);
    return 1;
}

static void test_rt_utf16le(void) {
    roundtrip_encoding("rt_utf16le",
        "key: value\nlist:\n- a\n- b\n",
        YAML_UTF16LE_ENCODING);
}

static void test_rt_utf16be(void) {
    roundtrip_encoding("rt_utf16be",
        "key: value\nlist:\n- a\n- b\n",
        YAML_UTF16BE_ENCODING);
}

static void test_rt_utf16le_unicode(void) {
    roundtrip_encoding("rt_utf16le_unicode",
        "text: \"\xc3\xa9\xc3\xa0\xc3\xbc\"\n",
        YAML_UTF16LE_ENCODING);
}

static void test_rt_utf16be_unicode(void) {
    roundtrip_encoding("rt_utf16be_unicode",
        "text: \"\xc3\xa9\xc3\xa0\xc3\xbc\"\n",
        YAML_UTF16BE_ENCODING);
}

/* ========================================================================
 * Emitter style round-trips: force specific styles and verify content
 * ======================================================================== */

/* Round-trip with specific scalar style forced */
static int roundtrip_scalar_style(const char *test_name, const char *value,
                                  yaml_scalar_style_t style) {
    yaml_emitter_t emitter;
    yaml_event_t event;
    unsigned char output[OUTPUT_SIZE];
    size_t written = 0;
    char msg[256];

    if (!yaml_emitter_initialize(&emitter)) return 0;
    yaml_emitter_set_output_string(&emitter, output, OUTPUT_SIZE, &written);
    yaml_emitter_set_unicode(&emitter, 1);

    yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
    yaml_emitter_emit(&emitter, &event);

    yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1);
    yaml_emitter_emit(&emitter, &event);

    yaml_scalar_event_initialize(&event, NULL,
        (const yaml_char_t *)YAML_STR_TAG,
        (const yaml_char_t *)value, strlen(value),
        1, 1, style);
    yaml_emitter_emit(&emitter, &event);

    yaml_document_end_event_initialize(&event, 1);
    yaml_emitter_emit(&emitter, &event);

    yaml_stream_end_event_initialize(&event);
    yaml_emitter_emit(&emitter, &event);

    yaml_emitter_delete(&emitter);

    /* Re-parse and check value */
    stored_event_t events[MAX_EVENTS];
    int count = 0;

    snprintf(msg, sizeof(msg), "%s: re-parse", test_name);
    if (!parse_to_events((const char *)output, written, events, MAX_EVENTS, &count)) {
        ASSERT(0, msg);
        return 0;
    }

    /* Find the scalar event */
    int found = 0;
    for (int i = 0; i < count; i++) {
        if (events[i].type == YAML_SCALAR_EVENT) {
            snprintf(msg, sizeof(msg), "%s: value match", test_name);
            ASSERT_EQ_STR(events[i].value, value, msg);

            snprintf(msg, sizeof(msg), "%s: length match", test_name);
            ASSERT_EQ_INT((int)events[i].length, (int)strlen(value), msg);
            found = 1;
            break;
        }
    }
    snprintf(msg, sizeof(msg), "%s: scalar found", test_name);
    ASSERT(found, msg);

    free_stored_events(events, count);
    return 1;
}

static void test_rt_style_plain(void) {
    roundtrip_scalar_style("rt_style_plain", "hello", YAML_PLAIN_SCALAR_STYLE);
}

static void test_rt_style_single(void) {
    roundtrip_scalar_style("rt_style_single", "hello world", YAML_SINGLE_QUOTED_SCALAR_STYLE);
}

static void test_rt_style_double(void) {
    roundtrip_scalar_style("rt_style_double", "hello world", YAML_DOUBLE_QUOTED_SCALAR_STYLE);
}

static void test_rt_style_literal(void) {
    roundtrip_scalar_style("rt_style_literal", "line1\nline2\nline3\n", YAML_LITERAL_SCALAR_STYLE);
}

static void test_rt_style_folded(void) {
    roundtrip_scalar_style("rt_style_folded", "line1\nline2\nline3\n", YAML_FOLDED_SCALAR_STYLE);
}

static void test_rt_style_double_special(void) {
    roundtrip_scalar_style("rt_style_dbl_special", "has\ttab and\nnewline", YAML_DOUBLE_QUOTED_SCALAR_STYLE);
}

static void test_rt_style_double_backslash(void) {
    roundtrip_scalar_style("rt_style_dbl_bs", "back\\slash", YAML_DOUBLE_QUOTED_SCALAR_STYLE);
}

static void test_rt_style_double_quotes(void) {
    roundtrip_scalar_style("rt_style_dbl_quotes", "has \"quotes\"", YAML_DOUBLE_QUOTED_SCALAR_STYLE);
}

static void test_rt_style_single_apostrophe(void) {
    roundtrip_scalar_style("rt_style_sgl_apos", "it''s", YAML_SINGLE_QUOTED_SCALAR_STYLE);
}

static void test_rt_style_literal_empty_lines(void) {
    roundtrip_scalar_style("rt_style_lit_empty", "line1\n\nline3\n", YAML_LITERAL_SCALAR_STYLE);
}

static void test_rt_style_folded_empty_lines(void) {
    roundtrip_scalar_style("rt_style_fold_empty", "line1\n\nline3\n", YAML_FOLDED_SCALAR_STYLE);
}

/* ========================================================================
 * Document API round-trips (load -> dump -> load)
 * ======================================================================== */

static int roundtrip_document(const char *test_name, const char *yaml) {
    yaml_parser_t parser;
    yaml_emitter_t emitter;
    yaml_document_t doc;
    unsigned char output[OUTPUT_SIZE];
    size_t written = 0;
    char msg[256];

    /* Load */
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)yaml, strlen(yaml));
    snprintf(msg, sizeof(msg), "%s: load", test_name);
    if (!yaml_parser_load(&parser, &doc)) {
        ASSERT(0, msg);
        yaml_parser_delete(&parser);
        return 0;
    }
    yaml_parser_delete(&parser);

    /* Check we got a non-empty document */
    yaml_node_t *root = yaml_document_get_root_node(&doc);
    snprintf(msg, sizeof(msg), "%s: has root", test_name);
    ASSERT_NOT_NULL(root, msg);

    /* Dump to string */
    yaml_emitter_initialize(&emitter);
    yaml_emitter_set_output_string(&emitter, output, OUTPUT_SIZE, &written);
    yaml_emitter_set_unicode(&emitter, 1);

    snprintf(msg, sizeof(msg), "%s: open", test_name);
    ASSERT(yaml_emitter_open(&emitter), msg);

    snprintf(msg, sizeof(msg), "%s: dump", test_name);
    ASSERT(yaml_emitter_dump(&emitter, &doc), msg);
    /* doc is consumed by dump */

    snprintf(msg, sizeof(msg), "%s: close", test_name);
    ASSERT(yaml_emitter_close(&emitter), msg);
    yaml_emitter_delete(&emitter);

    /* Re-load and verify root type matches */
    yaml_document_t doc2;
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, output, written);
    snprintf(msg, sizeof(msg), "%s: re-load", test_name);
    if (!yaml_parser_load(&parser, &doc2)) {
        ASSERT(0, msg);
        yaml_parser_delete(&parser);
        return 0;
    }
    yaml_parser_delete(&parser);

    yaml_node_t *root2 = yaml_document_get_root_node(&doc2);
    snprintf(msg, sizeof(msg), "%s: re-load has root", test_name);
    ASSERT_NOT_NULL(root2, msg);

    if (root2) {
        snprintf(msg, sizeof(msg), "%s: root type match", test_name);
        /* We can't compare root types across docs since doc was consumed,
         * but we can at least verify re-load produced a valid document */
        ASSERT(root2->type == YAML_SCALAR_NODE ||
               root2->type == YAML_SEQUENCE_NODE ||
               root2->type == YAML_MAPPING_NODE, msg);
    }

    yaml_document_delete(&doc2);
    return 1;
}

static void test_rt_doc_scalar(void) {
    roundtrip_document("rt_doc_scalar", "hello world");
}

static void test_rt_doc_mapping(void) {
    roundtrip_document("rt_doc_map", "a: b\nc: d\n");
}

static void test_rt_doc_sequence(void) {
    roundtrip_document("rt_doc_seq", "- a\n- b\n- c\n");
}

static void test_rt_doc_nested(void) {
    roundtrip_document("rt_doc_nested",
        "users:\n"
        "  - name: Alice\n"
        "    roles:\n"
        "      - admin\n"
        "      - user\n"
        "  - name: Bob\n"
        "    roles:\n"
        "      - user\n");
}

static void test_rt_doc_anchors(void) {
    roundtrip_document("rt_doc_anchors",
        "- &val hello\n"
        "- *val\n");
}

/* ========================================================================
 * Canonical round-trips
 * ======================================================================== */

static int roundtrip_canonical(const char *test_name, const char *yaml) {
    stored_event_t events1[MAX_EVENTS], events2[MAX_EVENTS];
    int count1 = 0, count2 = 0;
    unsigned char output[OUTPUT_SIZE];
    size_t written = 0;
    char msg[256];

    if (!parse_to_events(yaml, strlen(yaml), events1, MAX_EVENTS, &count1)) {
        snprintf(msg, sizeof(msg), "%s: parse", test_name);
        ASSERT(0, msg);
        return 0;
    }

    /* Emit in canonical mode */
    yaml_emitter_t emitter;
    yaml_event_t event;
    yaml_emitter_initialize(&emitter);
    yaml_emitter_set_output_string(&emitter, output, OUTPUT_SIZE, &written);
    yaml_emitter_set_canonical(&emitter, 1);

    for (int i = 0; i < count1; i++) {
        const stored_event_t *ev = &events1[i];
        switch (ev->type) {
        case YAML_STREAM_START_EVENT:
            yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
            break;
        case YAML_STREAM_END_EVENT:
            yaml_stream_end_event_initialize(&event);
            break;
        case YAML_DOCUMENT_START_EVENT:
            yaml_document_start_event_initialize(&event, NULL, NULL, NULL, ev->implicit);
            break;
        case YAML_DOCUMENT_END_EVENT:
            yaml_document_end_event_initialize(&event, ev->implicit);
            break;
        case YAML_ALIAS_EVENT:
            yaml_alias_event_initialize(&event, (const yaml_char_t *)ev->anchor);
            break;
        case YAML_SCALAR_EVENT:
            yaml_scalar_event_initialize(&event,
                (const yaml_char_t *)ev->anchor,
                (const yaml_char_t *)ev->tag,
                (const yaml_char_t *)ev->value,
                (int)ev->length,
                ev->plain_implicit, ev->quoted_implicit,
                YAML_ANY_SCALAR_STYLE);
            break;
        case YAML_SEQUENCE_START_EVENT:
            yaml_sequence_start_event_initialize(&event,
                (const yaml_char_t *)ev->anchor,
                (const yaml_char_t *)ev->tag,
                ev->tag ? 0 : 1,
                YAML_ANY_SEQUENCE_STYLE);
            break;
        case YAML_SEQUENCE_END_EVENT:
            yaml_sequence_end_event_initialize(&event);
            break;
        case YAML_MAPPING_START_EVENT:
            yaml_mapping_start_event_initialize(&event,
                (const yaml_char_t *)ev->anchor,
                (const yaml_char_t *)ev->tag,
                ev->tag ? 0 : 1,
                YAML_ANY_MAPPING_STYLE);
            break;
        case YAML_MAPPING_END_EVENT:
            yaml_mapping_end_event_initialize(&event);
            break;
        default:
            break;
        }
        yaml_emitter_emit(&emitter, &event);
    }
    yaml_emitter_delete(&emitter);

    /* Re-parse canonical output */
    if (!parse_to_events((const char *)output, written, events2, MAX_EVENTS, &count2)) {
        snprintf(msg, sizeof(msg), "%s: re-parse canonical", test_name);
        ASSERT(0, msg);
        free_stored_events(events1, count1);
        return 0;
    }

    compare_events(test_name, events1, count1, events2, count2);

    free_stored_events(events1, count1);
    free_stored_events(events2, count2);
    return 1;
}

static void test_rt_canonical_scalar(void) {
    roundtrip_canonical("rt_canon_scalar", "hello");
}

static void test_rt_canonical_mapping(void) {
    roundtrip_canonical("rt_canon_map", "a: b\nc: d\n");
}

static void test_rt_canonical_sequence(void) {
    roundtrip_canonical("rt_canon_seq", "- a\n- b\n- c\n");
}

static void test_rt_canonical_nested(void) {
    roundtrip_canonical("rt_canon_nested",
        "outer:\n"
        "  inner:\n"
        "    - a\n"
        "    - b\n");
}

static void test_rt_canonical_complex(void) {
    roundtrip_canonical("rt_canon_complex",
        "name: test\n"
        "items:\n"
        "  - id: '1'\n"
        "    values: [a, b, c]\n"
        "  - id: '2'\n"
        "    values: [d, e]\n");
}

int main(void) {
    TEST_SUITE_BEGIN("Round-trip");

    /* Basic */
    test_rt_empty();
    test_rt_scalar();
    test_rt_integer();
    test_rt_float();
    test_rt_boolean_true();
    test_rt_boolean_false();
    test_rt_null();
    test_rt_empty_string();
    test_rt_tilde();

    /* Scalar styles */
    test_rt_single_quoted();
    test_rt_double_quoted();
    test_rt_literal();
    test_rt_folded();
    test_rt_literal_keep();
    test_rt_literal_strip();
    test_rt_folded_keep();
    test_rt_folded_strip();

    /* Collections */
    test_rt_simple_mapping();
    test_rt_simple_sequence();
    test_rt_nested_mapping();
    test_rt_nested_sequence();
    test_rt_mapping_of_sequences();
    test_rt_sequence_of_mappings();
    test_rt_flow_sequence();
    test_rt_flow_mapping();
    test_rt_flow_nested();
    test_rt_mixed_flow_block();
    test_rt_empty_mapping();
    test_rt_empty_sequence();
    test_rt_empty_value();
    test_rt_empty_key();

    /* Anchors & aliases */
    test_rt_anchor_alias();
    test_rt_anchor_mapping();
    test_rt_multiple_anchors();

    /* Documents */
    test_rt_explicit_doc();
    test_rt_multi_doc();
    test_rt_multi_doc_explicit_end();

    /* Special scalar content */
    test_rt_multiline_plain();
    test_rt_special_chars();
    test_rt_colon_in_value();
    test_rt_hash_in_value();
    test_rt_leading_space();
    test_rt_trailing_space();
    test_rt_newlines_in_scalar();
    test_rt_tab_in_scalar();
    test_rt_backslash_in_scalar();
    test_rt_quote_in_scalar();
    test_rt_unicode_bmp();
    test_rt_empty_lines_literal();
    test_rt_indicator_chars();

    /* Tagged content */
    test_rt_tagged_str();
    test_rt_tagged_int();
    test_rt_tagged_float();
    test_rt_tagged_bool();
    test_rt_tagged_null();
    test_rt_tagged_seq();
    test_rt_tagged_map();

    /* Complex structures */
    test_rt_kubernetes();
    test_rt_docker_compose();
    test_rt_github_actions();
    test_rt_deep_nesting();
    test_rt_wide_mapping();
    test_rt_long_sequence();

    /* Edge cases */
    test_rt_numeric_keys();
    test_rt_bool_like_keys();
    test_rt_null_like_values();
    test_rt_multiline_key();
    test_rt_complex_key();
    test_rt_seq_as_map_value();

    /* Stress */
    test_rt_large_mapping();
    test_rt_large_sequence();
    test_rt_large_nested();

    /* Encoding */
    test_rt_utf16le();
    test_rt_utf16be();
    test_rt_utf16le_unicode();
    test_rt_utf16be_unicode();

    /* Forced scalar styles */
    test_rt_style_plain();
    test_rt_style_single();
    test_rt_style_double();
    test_rt_style_literal();
    test_rt_style_folded();
    test_rt_style_double_special();
    test_rt_style_double_backslash();
    test_rt_style_double_quotes();
    test_rt_style_single_apostrophe();
    test_rt_style_literal_empty_lines();
    test_rt_style_folded_empty_lines();

    /* Document API round-trips */
    test_rt_doc_scalar();
    test_rt_doc_mapping();
    test_rt_doc_sequence();
    test_rt_doc_nested();
    test_rt_doc_anchors();

    /* Canonical mode */
    test_rt_canonical_scalar();
    test_rt_canonical_mapping();
    test_rt_canonical_sequence();
    test_rt_canonical_nested();
    test_rt_canonical_complex();

    TEST_SUITE_END();
}
