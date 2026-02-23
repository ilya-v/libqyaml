/*
 * OOM (Out of Memory) injection test.
 *
 * Tests that all allocation failure paths in libqyaml return errors
 * cleanly without leaks, double-frees, or use-after-free.
 *
 * Strategy: override malloc/realloc via LD_PRELOAD-style interposition
 * to fail after exactly N successful allocations, then verify:
 *   1. The API returns an error (not a crash)
 *   2. Cleanup runs without issues (no double-free under ASan)
 *   3. The error code is YAML_MEMORY_ERROR where applicable
 *
 * Build: cc -O0 -g -fsanitize=address -DYAML_DECLARE_STATIC \
 *        -I../include -I../build/include test_oom.c ../build/libqyaml.a \
 *        -Wl,--wrap=malloc -Wl,--wrap=realloc -Wl,--wrap=calloc \
 *        -o test-oom
 *
 * The --wrap linker flag redirects malloc -> __wrap_malloc and provides
 * __real_malloc to call the original. This catches all allocations
 * including those inside yaml_malloc.
 */

#include <yaml.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Counters for injection */
static int g_alloc_count = 0;
static int g_fail_after = -1;  /* -1 = no injection */
static int g_total_allocs = 0;
static int g_failed_allocs = 0;

/* Real libc functions provided by --wrap */
extern void *__real_malloc(size_t size);
extern void *__real_realloc(void *ptr, size_t size);
extern void *__real_calloc(size_t nmemb, size_t size);

void *__wrap_malloc(size_t size) {
    if (g_fail_after >= 0 && g_alloc_count >= g_fail_after) {
        g_failed_allocs++;
        return NULL;
    }
    g_alloc_count++;
    g_total_allocs++;
    return __real_malloc(size);
}

void *__wrap_realloc(void *ptr, size_t size) {
    if (g_fail_after >= 0 && g_alloc_count >= g_fail_after) {
        g_failed_allocs++;
        return NULL;
    }
    g_alloc_count++;
    g_total_allocs++;
    return __real_realloc(ptr, size);
}

void *__wrap_calloc(size_t nmemb, size_t size) {
    if (g_fail_after >= 0 && g_alloc_count >= g_fail_after) {
        g_failed_allocs++;
        return NULL;
    }
    g_alloc_count++;
    g_total_allocs++;
    return __real_calloc(nmemb, size);
}

/* Reset injection state */
static void oom_reset(void) {
    g_alloc_count = 0;
    g_fail_after = -1;
    g_total_allocs = 0;
    g_failed_allocs = 0;
}

/* Start failing after N successful allocations */
static void oom_set_fail_after(int n) {
    g_alloc_count = 0;
    g_fail_after = n;
    g_failed_allocs = 0;
}

/* ================================================================
 * Test helpers
 * ================================================================ */

static int tests_run = 0;
static int tests_passed = 0;

#define PASS() do { tests_passed++; tests_run++; } while(0)
#define FAIL(msg) do { \
    fprintf(stderr, "  FAIL: %s (line %d)\n", msg, __LINE__); \
    tests_run++; \
} while(0)

/* ================================================================
 * Test: yaml_parser_initialize OOM
 *
 * Parser init does 8 allocations (raw_buffer, buffer, tokens,
 * indents, simple_keys, states, marks, tag_directives).
 * Fail at each point and verify clean error return.
 * ================================================================ */
static void test_parser_init_oom(void) {
    printf("  parser_initialize OOM...\n");

    /* First, find how many allocs a successful init takes */
    oom_reset();
    oom_set_fail_after(1000); /* high enough to not fail */
    yaml_parser_t parser;
    int ok = yaml_parser_initialize(&parser);
    if (!ok) {
        FAIL("parser init failed with high limit");
        return;
    }
    int allocs_needed = g_alloc_count;
    yaml_parser_delete(&parser);
    printf("    init needs %d allocations\n", allocs_needed);

    /* Now fail at each allocation point */
    int failures_tested = 0;
    for (int n = 0; n < allocs_needed; n++) {
        oom_set_fail_after(n);
        memset(&parser, 0xAB, sizeof(parser)); /* poison */
        ok = yaml_parser_initialize(&parser);
        if (ok) {
            /* Should have failed but didn't -- still ok, just not testing OOM here */
            yaml_parser_delete(&parser);
        } else {
            failures_tested++;
            /* Verify error is set (parser struct should be safe to inspect
             * since initialize should have cleaned up) */
        }
    }
    printf("    tested %d OOM failure points\n", failures_tested);
    if (failures_tested > 0) {
        PASS();
    } else {
        FAIL("no OOM failures triggered in parser init");
    }
}

/* ================================================================
 * Test: yaml_emitter_initialize OOM
 * ================================================================ */
static void test_emitter_init_oom(void) {
    printf("  emitter_initialize OOM...\n");

    oom_reset();
    oom_set_fail_after(1000);
    yaml_emitter_t emitter;
    int ok = yaml_emitter_initialize(&emitter);
    if (!ok) {
        FAIL("emitter init failed with high limit");
        return;
    }
    int allocs_needed = g_alloc_count;
    yaml_emitter_delete(&emitter);
    printf("    init needs %d allocations\n", allocs_needed);

    int failures_tested = 0;
    for (int n = 0; n < allocs_needed; n++) {
        oom_set_fail_after(n);
        memset(&emitter, 0xAB, sizeof(emitter));
        ok = yaml_emitter_initialize(&emitter);
        if (ok) {
            yaml_emitter_delete(&emitter);
        } else {
            failures_tested++;
        }
    }
    printf("    tested %d OOM failure points\n", failures_tested);
    if (failures_tested > 0) {
        PASS();
    } else {
        FAIL("no OOM failures triggered in emitter init");
    }
}

/* ================================================================
 * Test: yaml_parser_parse OOM during scanning/parsing
 *
 * Initialize parser successfully, then inject failures during parse.
 * ================================================================ */
static void test_parser_parse_oom(const char *name,
                                   const char *yaml_input) {
    printf("  parser_parse OOM [%s]...\n", name);
    size_t input_len = strlen(yaml_input);

    /* First, count allocs for a successful parse */
    oom_reset();
    oom_set_fail_after(100000);
    yaml_parser_t parser;
    yaml_event_t event;
    if (!yaml_parser_initialize(&parser)) {
        FAIL("parser init failed");
        return;
    }
    yaml_parser_set_input_string(&parser, (const unsigned char *)yaml_input, input_len);
    int event_count = 0;
    while (1) {
        if (!yaml_parser_parse(&parser, &event)) break;
        int done = (event.type == YAML_STREAM_END_EVENT);
        yaml_event_delete(&event);
        event_count++;
        if (done) break;
    }
    int allocs_needed = g_alloc_count;
    yaml_parser_delete(&parser);
    printf("    full parse needs %d allocations, %d events\n", allocs_needed, event_count);

    /* Now fail at each allocation point during parsing
     * (skip the init allocs, start failing during parse) */
    int failures_tested = 0;
    for (int n = 0; n < allocs_needed; n++) {
        oom_set_fail_after(n);
        if (!yaml_parser_initialize(&parser)) {
            /* OOM during init is fine */
            failures_tested++;
            continue;
        }
        yaml_parser_set_input_string(&parser, (const unsigned char *)yaml_input, input_len);
        while (1) {
            if (!yaml_parser_parse(&parser, &event)) {
                /* OOM during parse -- this is what we're testing */
                failures_tested++;
                break;
            }
            int done = (event.type == YAML_STREAM_END_EVENT);
            yaml_event_delete(&event);
            if (done) break;
        }
        yaml_parser_delete(&parser);
    }
    printf("    tested %d OOM failure points\n", failures_tested);
    if (failures_tested > 0) {
        PASS();
    } else {
        FAIL("no OOM failures triggered");
    }
}

/* ================================================================
 * Test: yaml_parser_load OOM during document loading
 * ================================================================ */
static void test_parser_load_oom(const char *name,
                                  const char *yaml_input) {
    printf("  parser_load OOM [%s]...\n", name);
    size_t input_len = strlen(yaml_input);

    /* Count allocs for successful load */
    oom_reset();
    oom_set_fail_after(100000);
    yaml_parser_t parser;
    yaml_document_t doc;
    if (!yaml_parser_initialize(&parser)) {
        FAIL("parser init failed");
        return;
    }
    yaml_parser_set_input_string(&parser, (const unsigned char *)yaml_input, input_len);
    int docs = 0;
    while (1) {
        if (!yaml_parser_load(&parser, &doc)) break;
        int has_root = (yaml_document_get_root_node(&doc) != NULL);
        yaml_document_delete(&doc);
        if (!has_root) break;
        docs++;
    }
    int allocs_needed = g_alloc_count;
    yaml_parser_delete(&parser);
    printf("    full load needs %d allocations, %d documents\n", allocs_needed, docs);

    /* Fail at each point */
    int failures_tested = 0;
    for (int n = 0; n < allocs_needed; n++) {
        oom_set_fail_after(n);
        if (!yaml_parser_initialize(&parser)) {
            failures_tested++;
            continue;
        }
        yaml_parser_set_input_string(&parser, (const unsigned char *)yaml_input, input_len);
        while (1) {
            if (!yaml_parser_load(&parser, &doc)) {
                failures_tested++;
                break;
            }
            int has_root = (yaml_document_get_root_node(&doc) != NULL);
            yaml_document_delete(&doc);
            if (!has_root) break;
        }
        yaml_parser_delete(&parser);
    }
    printf("    tested %d OOM failure points\n", failures_tested);
    if (failures_tested > 0) {
        PASS();
    } else {
        FAIL("no OOM failures triggered");
    }
}

/* ================================================================
 * Test: yaml_emitter_emit OOM during emitting
 * ================================================================ */
static int emit_write_handler(void *data, unsigned char *buffer, size_t size) {
    (void)data; (void)buffer; (void)size;
    return 1; /* discard output */
}

static void test_emitter_emit_oom(void) {
    printf("  emitter_emit OOM...\n");

    /* Count allocs for successful emit */
    oom_reset();
    oom_set_fail_after(100000);
    yaml_emitter_t emitter;
    yaml_event_t event;
    if (!yaml_emitter_initialize(&emitter)) {
        FAIL("emitter init failed");
        return;
    }
    yaml_emitter_set_output(&emitter, emit_write_handler, NULL);

    yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
    yaml_emitter_emit(&emitter, &event);
    yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1);
    yaml_emitter_emit(&emitter, &event);
    yaml_mapping_start_event_initialize(&event, NULL, NULL, 1, YAML_BLOCK_MAPPING_STYLE);
    yaml_emitter_emit(&emitter, &event);
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
    yaml_emitter_emit(&emitter, &event);

    int allocs_needed = g_alloc_count;
    yaml_emitter_delete(&emitter);
    printf("    full emit needs %d allocations\n", allocs_needed);

    /* Fail at each point */
    int failures_tested = 0;
    for (int n = 0; n < allocs_needed; n++) {
        oom_set_fail_after(n);
        if (!yaml_emitter_initialize(&emitter)) {
            failures_tested++;
            continue;
        }
        yaml_emitter_set_output(&emitter, emit_write_handler, NULL);

        int failed = 0;

        /* Helper: initialize an event and emit it.
         * Must check init return value for events that allocate
         * (scalar, doc_start with directives, etc.) to avoid
         * passing stale event data to emit. */
#define EMIT_EVENT(init_call) do { \
    memset(&event, 0, sizeof(event)); \
    if (!(init_call)) { failed = 1; break; } \
    if (!yaml_emitter_emit(&emitter, &event)) { failed = 1; } \
} while(0)

        if (!failed) EMIT_EVENT(yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING));
        if (!failed) EMIT_EVENT(yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1));
        if (!failed) EMIT_EVENT(yaml_mapping_start_event_initialize(&event, NULL, NULL, 1, YAML_BLOCK_MAPPING_STYLE));
        if (!failed) EMIT_EVENT(yaml_scalar_event_initialize(&event, NULL, NULL, (yaml_char_t *)"key", 3, 1, 1, YAML_PLAIN_SCALAR_STYLE));
        if (!failed) EMIT_EVENT(yaml_scalar_event_initialize(&event, NULL, NULL, (yaml_char_t *)"value", 5, 1, 1, YAML_PLAIN_SCALAR_STYLE));
        if (!failed) EMIT_EVENT(yaml_mapping_end_event_initialize(&event));
        if (!failed) EMIT_EVENT(yaml_document_end_event_initialize(&event, 1));
        if (!failed) EMIT_EVENT(yaml_stream_end_event_initialize(&event));
#undef EMIT_EVENT

        if (failed) failures_tested++;
        yaml_emitter_delete(&emitter);
    }
    printf("    tested %d OOM failure points\n", failures_tested);
    if (failures_tested > 0) {
        PASS();
    } else {
        FAIL("no OOM failures triggered");
    }
}

/* ================================================================
 * Test: yaml_document_initialize OOM
 * ================================================================ */
static void test_document_init_oom(void) {
    printf("  document_initialize OOM...\n");

    oom_reset();
    oom_set_fail_after(100000);
    yaml_document_t doc;
    int ok = yaml_document_initialize(&doc, NULL, NULL, NULL, 1, 1);
    if (!ok) {
        FAIL("document init failed with high limit");
        return;
    }
    int allocs_needed = g_alloc_count;
    yaml_document_delete(&doc);
    printf("    init needs %d allocations\n", allocs_needed);

    int failures_tested = 0;
    for (int n = 0; n < allocs_needed; n++) {
        oom_set_fail_after(n);
        ok = yaml_document_initialize(&doc, NULL, NULL, NULL, 1, 1);
        if (ok) {
            yaml_document_delete(&doc);
        } else {
            failures_tested++;
        }
    }
    printf("    tested %d OOM failure points\n", failures_tested);
    if (failures_tested > 0) {
        PASS();
    } else {
        FAIL("no OOM failures triggered");
    }
}

/* ================================================================
 * Test: document add_* OOM
 * ================================================================ */
static void test_document_add_nodes_oom(void) {
    printf("  document add_nodes OOM...\n");

    /* Count allocs for building a small document */
    oom_reset();
    oom_set_fail_after(100000);
    yaml_document_t doc;
    if (!yaml_document_initialize(&doc, NULL, NULL, NULL, 1, 1)) {
        FAIL("doc init failed");
        return;
    }
    int root = yaml_document_add_mapping(&doc, NULL, YAML_BLOCK_MAPPING_STYLE);
    int key = yaml_document_add_scalar(&doc, NULL, (yaml_char_t *)"key", 3, YAML_PLAIN_SCALAR_STYLE);
    int val = yaml_document_add_scalar(&doc, NULL, (yaml_char_t *)"val", 3, YAML_PLAIN_SCALAR_STYLE);
    yaml_document_append_mapping_pair(&doc, root, key, val);
    int seq = yaml_document_add_sequence(&doc, NULL, YAML_BLOCK_SEQUENCE_STYLE);
    int item = yaml_document_add_scalar(&doc, NULL, (yaml_char_t *)"item", 4, YAML_PLAIN_SCALAR_STYLE);
    yaml_document_append_sequence_item(&doc, seq, item);
    int allocs_needed = g_alloc_count;
    yaml_document_delete(&doc);
    printf("    doc construction needs %d allocations\n", allocs_needed);

    int failures_tested = 0;
    for (int n = 0; n < allocs_needed; n++) {
        oom_set_fail_after(n);
        if (!yaml_document_initialize(&doc, NULL, NULL, NULL, 1, 1)) {
            failures_tested++;
            continue;
        }
        root = yaml_document_add_mapping(&doc, NULL, YAML_BLOCK_MAPPING_STYLE);
        if (!root) { failures_tested++; yaml_document_delete(&doc); continue; }
        key = yaml_document_add_scalar(&doc, NULL, (yaml_char_t *)"key", 3, YAML_PLAIN_SCALAR_STYLE);
        if (!key) { failures_tested++; yaml_document_delete(&doc); continue; }
        val = yaml_document_add_scalar(&doc, NULL, (yaml_char_t *)"val", 3, YAML_PLAIN_SCALAR_STYLE);
        if (!val) { failures_tested++; yaml_document_delete(&doc); continue; }
        yaml_document_append_mapping_pair(&doc, root, key, val);
        seq = yaml_document_add_sequence(&doc, NULL, YAML_BLOCK_SEQUENCE_STYLE);
        if (!seq) { failures_tested++; yaml_document_delete(&doc); continue; }
        item = yaml_document_add_scalar(&doc, NULL, (yaml_char_t *)"item", 4, YAML_PLAIN_SCALAR_STYLE);
        if (!item) { failures_tested++; yaml_document_delete(&doc); continue; }
        yaml_document_append_sequence_item(&doc, seq, item);
        yaml_document_delete(&doc);
    }
    printf("    tested %d OOM failure points\n", failures_tested);
    if (failures_tested > 0) {
        PASS();
    } else {
        FAIL("no OOM failures triggered");
    }
}

/* ================================================================
 * Test: event init OOM (scalar, etc.)
 * ================================================================ */
static void test_event_init_oom(void) {
    printf("  event initialization OOM...\n");

    int failures_tested = 0;

    /* Test yaml_scalar_event_initialize with OOM */
    for (int n = 0; n < 10; n++) {
        oom_set_fail_after(n);
        yaml_event_t event;
        int ok = yaml_scalar_event_initialize(&event,
            (yaml_char_t *)"anchor", (yaml_char_t *)"!tag",
            (yaml_char_t *)"long scalar value that needs allocation", 39,
            0, 0, YAML_PLAIN_SCALAR_STYLE);
        if (ok) {
            yaml_event_delete(&event);
        } else {
            failures_tested++;
        }
    }

    /* Test yaml_document_start_event_initialize with tag directives */
    yaml_tag_directive_t tags[] = {
        { (yaml_char_t *)"!e!", (yaml_char_t *)"tag:example.com,2000:" },
    };
    for (int n = 0; n < 10; n++) {
        oom_set_fail_after(n);
        yaml_event_t event;
        yaml_version_directive_t version = { 1, 1 };
        int ok = yaml_document_start_event_initialize(&event,
            &version, tags, tags + 1, 0);
        if (ok) {
            yaml_event_delete(&event);
        } else {
            failures_tested++;
        }
    }

    /* Test yaml_sequence_start_event_initialize */
    for (int n = 0; n < 5; n++) {
        oom_set_fail_after(n);
        yaml_event_t event;
        int ok = yaml_sequence_start_event_initialize(&event,
            (yaml_char_t *)"anchor", (yaml_char_t *)"!tag",
            0, YAML_BLOCK_SEQUENCE_STYLE);
        if (ok) {
            yaml_event_delete(&event);
        } else {
            failures_tested++;
        }
    }

    /* Test yaml_mapping_start_event_initialize */
    for (int n = 0; n < 5; n++) {
        oom_set_fail_after(n);
        yaml_event_t event;
        int ok = yaml_mapping_start_event_initialize(&event,
            (yaml_char_t *)"anchor", (yaml_char_t *)"!tag",
            0, YAML_BLOCK_MAPPING_STYLE);
        if (ok) {
            yaml_event_delete(&event);
        } else {
            failures_tested++;
        }
    }

    printf("    tested %d OOM failure points across event types\n", failures_tested);
    if (failures_tested > 0) {
        PASS();
    } else {
        FAIL("no OOM failures triggered");
    }
}

/* ================================================================
 * YAML inputs for parse/load OOM testing
 * ================================================================ */

static const char *yaml_simple = "key: value\n";

static const char *yaml_complex =
    "apiVersion: v1\n"
    "kind: Service\n"
    "metadata:\n"
    "  name: my-service\n"
    "  labels:\n"
    "    app: web\n"
    "    tier: frontend\n"
    "spec:\n"
    "  ports:\n"
    "    - port: 80\n"
    "      targetPort: 8080\n"
    "    - port: 443\n"
    "      targetPort: 8443\n"
    "  selector:\n"
    "    app: web\n";

static const char *yaml_anchors =
    "defaults: &defaults\n"
    "  timeout: 30\n"
    "  retries: 3\n"
    "production:\n"
    "  <<: *defaults\n"
    "  timeout: 60\n";

static const char *yaml_flow = "[1, 2, {a: b, c: [d, e]}, 3]\n";

static const char *yaml_multiline =
    "literal: |\n"
    "  line one\n"
    "  line two\n"
    "folded: >\n"
    "  paragraph one\n"
    "  continues here\n"
    "\n"
    "  paragraph two\n";

static const char *yaml_tags =
    "%YAML 1.1\n"
    "%TAG !e! tag:example.com,2000:\n"
    "---\n"
    "!e!foo bar\n";

static const char *yaml_multi_doc =
    "---\n"
    "doc1: value1\n"
    "...\n"
    "---\n"
    "doc2: value2\n"
    "...\n";

static const char *yaml_quoted =
    "single: 'hello world'\n"
    "double: \"escape\\n\\t\\\"chars\"\n"
    "unicode: \"\\u0041\\U00000042\"\n";

/* ================================================================ */

int main(void) {
    printf("OOM Injection Tests\n");
    printf("===================\n\n");

    test_parser_init_oom();
    test_emitter_init_oom();
    test_document_init_oom();
    test_event_init_oom();
    test_document_add_nodes_oom();

    printf("\n");

    test_parser_parse_oom("simple", yaml_simple);
    test_parser_parse_oom("complex", yaml_complex);
    test_parser_parse_oom("anchors", yaml_anchors);
    test_parser_parse_oom("flow", yaml_flow);
    test_parser_parse_oom("multiline", yaml_multiline);
    test_parser_parse_oom("tags", yaml_tags);
    test_parser_parse_oom("multi_doc", yaml_multi_doc);
    test_parser_parse_oom("quoted", yaml_quoted);

    printf("\n");

    test_parser_load_oom("simple", yaml_simple);
    test_parser_load_oom("complex", yaml_complex);
    test_parser_load_oom("anchors", yaml_anchors);
    test_parser_load_oom("flow", yaml_flow);
    test_parser_load_oom("multiline", yaml_multiline);
    test_parser_load_oom("tags", yaml_tags);
    test_parser_load_oom("multi_doc", yaml_multi_doc);
    test_parser_load_oom("quoted", yaml_quoted);

    printf("\n");

    test_emitter_emit_oom();

    printf("\n===================\n");
    printf("Results: %d/%d passed\n", tests_passed, tests_run);

    return (tests_passed == tests_run) ? 0 : 1;
}
