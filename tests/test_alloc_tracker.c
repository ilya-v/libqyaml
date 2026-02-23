/*
 * Allocation-Tracking Allocator Test (Requirement 4.6)
 *
 * Records every malloc/realloc/calloc/free and verifies at teardown that:
 *   1. Every allocation was freed exactly once (no leaks)
 *   2. No pointer was freed more than once (no double-frees)
 *   3. No free was called on a pointer not returned by malloc/realloc/calloc
 *
 * Uses --wrap linker flags to intercept all allocation calls.
 *
 * Build: cc -O0 -g -DYAML_DECLARE_STATIC \
 *        -I../include test_alloc_tracker.c ../build/libqyaml.a \
 *        -Wl,--wrap=malloc -Wl,--wrap=realloc -Wl,--wrap=calloc \
 *        -Wl,--wrap=free -o test-alloc-tracker
 */

#include <yaml.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ================================================================
 * Allocation tracker: hash table of all live allocations
 * ================================================================ */

#define TRACKER_BUCKETS 4096
#define TRACKER_BUCKET_MASK (TRACKER_BUCKETS - 1)

typedef struct alloc_entry {
    void *ptr;
    size_t size;
    struct alloc_entry *next;
} alloc_entry_t;

static alloc_entry_t *tracker_buckets[TRACKER_BUCKETS];
static int tracker_total_allocs = 0;
static int tracker_total_frees = 0;
static int tracker_live_allocs = 0;
static int tracker_peak_allocs = 0;
static int tracker_double_frees = 0;
static int tracker_invalid_frees = 0;
static int tracker_enabled = 0;

/* Real libc functions provided by --wrap */
extern void *__real_malloc(size_t size);
extern void *__real_realloc(void *ptr, size_t size);
extern void *__real_calloc(size_t nmemb, size_t size);
extern void  __real_free(void *ptr);
extern char *__real_strdup(const char *s);

static unsigned hash_ptr(void *ptr) {
    unsigned long h = (unsigned long)ptr;
    h = (h >> 4) ^ (h >> 16);
    return h & TRACKER_BUCKET_MASK;
}

static void tracker_record_alloc(void *ptr, size_t size) {
    if (!tracker_enabled || !ptr) return;
    unsigned bucket = hash_ptr(ptr);
    alloc_entry_t *entry = __real_malloc(sizeof(alloc_entry_t));
    if (!entry) return; /* internal OOM - best effort */
    entry->ptr = ptr;
    entry->size = size;
    entry->next = tracker_buckets[bucket];
    tracker_buckets[bucket] = entry;
    tracker_total_allocs++;
    tracker_live_allocs++;
    if (tracker_live_allocs > tracker_peak_allocs)
        tracker_peak_allocs = tracker_live_allocs;
}

/* Returns 1 if found and removed, 0 if not found */
static int tracker_record_free(void *ptr) {
    if (!tracker_enabled || !ptr) return 1; /* free(NULL) is always valid */
    unsigned bucket = hash_ptr(ptr);
    alloc_entry_t **prev = &tracker_buckets[bucket];
    alloc_entry_t *cur = tracker_buckets[bucket];
    while (cur) {
        if (cur->ptr == ptr) {
            *prev = cur->next;
            __real_free(cur);
            tracker_total_frees++;
            tracker_live_allocs--;
            return 1;
        }
        prev = &cur->next;
        cur = cur->next;
    }
    return 0; /* not found - invalid or double free */
}

static void tracker_reset(void) {
    /* Free all tracking entries */
    for (int i = 0; i < TRACKER_BUCKETS; i++) {
        alloc_entry_t *cur = tracker_buckets[i];
        while (cur) {
            alloc_entry_t *next = cur->next;
            __real_free(cur);
            cur = next;
        }
        tracker_buckets[i] = NULL;
    }
    tracker_total_allocs = 0;
    tracker_total_frees = 0;
    tracker_live_allocs = 0;
    tracker_peak_allocs = 0;
    tracker_double_frees = 0;
    tracker_invalid_frees = 0;
}

static void tracker_start(void) {
    tracker_reset();
    tracker_enabled = 1;
}

static void tracker_stop(void) {
    tracker_enabled = 0;
}

/* ================================================================
 * Wrapped allocation functions
 * ================================================================ */

void *__wrap_malloc(size_t size) {
    void *ptr = __real_malloc(size);
    tracker_record_alloc(ptr, size);
    return ptr;
}

void *__wrap_realloc(void *old_ptr, size_t size) {
    if (size == 0 && old_ptr) {
        /* realloc(ptr, 0) is equivalent to free(ptr) on some systems */
        if (tracker_enabled) {
            if (!tracker_record_free(old_ptr)) {
                tracker_invalid_frees++;
            }
        }
        __real_free(old_ptr);
        return NULL;
    }
    if (!old_ptr) {
        /* realloc(NULL, size) is equivalent to malloc(size) */
        void *ptr = __real_malloc(size);
        tracker_record_alloc(ptr, size);
        return ptr;
    }
    /* Normal realloc: remove old tracking, call real realloc, add new tracking */
    if (tracker_enabled) {
        if (!tracker_record_free(old_ptr)) {
            tracker_invalid_frees++;
        }
    }
    void *new_ptr = __real_realloc(old_ptr, size);
    if (new_ptr) {
        tracker_record_alloc(new_ptr, size);
    }
    /* If realloc failed, old_ptr is still valid but we already removed tracking.
     * Re-add it. */
    if (!new_ptr && old_ptr) {
        tracker_record_alloc(old_ptr, 0); /* size unknown but track the pointer */
    }
    return new_ptr;
}

void *__wrap_calloc(size_t nmemb, size_t size) {
    void *ptr = __real_calloc(nmemb, size);
    tracker_record_alloc(ptr, nmemb * size);
    return ptr;
}

void __wrap_free(void *ptr) {
    if (ptr && tracker_enabled) {
        if (!tracker_record_free(ptr)) {
            /* This pointer was never allocated or was already freed */
            tracker_double_frees++;
            fprintf(stderr, "  WARNING: invalid/double free of %p\n", ptr);
        }
    }
    __real_free(ptr);
}

char *__wrap_strdup(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s) + 1;
    char *dup = (char *)__real_malloc(len);
    if (dup) {
        memcpy(dup, s, len);
        tracker_record_alloc(dup, len);
    }
    return dup;
}

/* ================================================================
 * Test framework
 * ================================================================ */

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;
static int total_leaks = 0;

#define ASSERT(cond, msg) do { \
    tests_run++; \
    if (!(cond)) { \
        fprintf(stderr, "  FAIL [%s:%d]: %s\n", __FILE__, __LINE__, msg); \
        tests_failed++; \
    } else { \
        tests_passed++; \
    } \
} while(0)

/* Check tracker state and report. Returns 0 if clean, 1 if errors. */
static int tracker_check(const char *test_name) {
    tracker_stop();
    int errors = 0;
    if (tracker_live_allocs != 0) {
        fprintf(stderr, "  LEAK: %s: %d allocation(s) not freed "
                "(total allocs=%d, frees=%d)\n",
                test_name, tracker_live_allocs,
                tracker_total_allocs, tracker_total_frees);
        total_leaks += tracker_live_allocs;
        errors++;
    }
    if (tracker_double_frees != 0) {
        fprintf(stderr, "  DOUBLE-FREE: %s: %d double/invalid free(s)\n",
                test_name, tracker_double_frees);
        errors++;
    }
    if (tracker_invalid_frees != 0) {
        fprintf(stderr, "  INVALID-FREE: %s: %d free(s) of untracked pointer(s)\n",
                test_name, tracker_invalid_frees);
        errors++;
    }
    return errors;
}

/* ================================================================
 * Test cases
 * ================================================================ */

static void test_parser_init_delete(void) {
    printf("  parser init/delete...\n");
    tracker_start();
    yaml_parser_t parser;
    int ok = yaml_parser_initialize(&parser);
    ASSERT(ok, "parser init should succeed");
    yaml_parser_delete(&parser);
    int errs = tracker_check("parser_init_delete");
    ASSERT(errs == 0, "parser init/delete should have zero leaks and zero double-frees");
    printf("    allocs=%d, frees=%d, peak=%d\n",
           tracker_total_allocs, tracker_total_frees, tracker_peak_allocs);
}

static void test_emitter_init_delete(void) {
    printf("  emitter init/delete...\n");
    tracker_start();
    yaml_emitter_t emitter;
    int ok = yaml_emitter_initialize(&emitter);
    ASSERT(ok, "emitter init should succeed");
    yaml_emitter_delete(&emitter);
    int errs = tracker_check("emitter_init_delete");
    ASSERT(errs == 0, "emitter init/delete should have zero leaks and zero double-frees");
    printf("    allocs=%d, frees=%d, peak=%d\n",
           tracker_total_allocs, tracker_total_frees, tracker_peak_allocs);
}

static void test_document_init_delete(void) {
    printf("  document init/delete...\n");
    tracker_start();
    yaml_document_t doc;
    int ok = yaml_document_initialize(&doc, NULL, NULL, NULL, 1, 1);
    ASSERT(ok, "doc init should succeed");
    yaml_document_delete(&doc);
    int errs = tracker_check("document_init_delete");
    ASSERT(errs == 0, "document init/delete should have zero leaks and zero double-frees");
    printf("    allocs=%d, frees=%d, peak=%d\n",
           tracker_total_allocs, tracker_total_frees, tracker_peak_allocs);
}

static void test_document_double_delete(void) {
    printf("  document double delete (idempotency)...\n");
    tracker_start();
    yaml_document_t doc;
    int ok = yaml_document_initialize(&doc, NULL, NULL, NULL, 1, 1);
    ASSERT(ok, "doc init should succeed");
    yaml_document_delete(&doc);
    /* Second delete should be safe (idempotent) */
    yaml_document_delete(&doc);
    int errs = tracker_check("document_double_delete");
    ASSERT(errs == 0, "double document delete should not cause double-free");
}

static void test_parse_string(const char *name, const char *yaml_input) {
    printf("  parse [%s]...\n", name);
    tracker_start();
    yaml_parser_t parser;
    yaml_event_t event;
    int ok = yaml_parser_initialize(&parser);
    ASSERT(ok, "parser init should succeed");
    yaml_parser_set_input_string(&parser,
        (const unsigned char *)yaml_input, strlen(yaml_input));
    int events = 0;
    while (1) {
        ok = yaml_parser_parse(&parser, &event);
        if (!ok) break;
        int done = (event.type == YAML_STREAM_END_EVENT);
        yaml_event_delete(&event);
        events++;
        if (done) break;
    }
    yaml_parser_delete(&parser);
    int errs = tracker_check(name);
    ASSERT(errs == 0, "parse should have zero leaks and zero double-frees");
    printf("    events=%d, allocs=%d, frees=%d, peak=%d\n",
           events, tracker_total_allocs, tracker_total_frees, tracker_peak_allocs);
}

static void test_scan_string(const char *name, const char *yaml_input) {
    printf("  scan [%s]...\n", name);
    tracker_start();
    yaml_parser_t parser;
    yaml_token_t token;
    int ok = yaml_parser_initialize(&parser);
    ASSERT(ok, "parser init should succeed");
    yaml_parser_set_input_string(&parser,
        (const unsigned char *)yaml_input, strlen(yaml_input));
    int tokens = 0;
    while (1) {
        ok = yaml_parser_scan(&parser, &token);
        if (!ok) break;
        int done = (token.type == YAML_STREAM_END_TOKEN);
        yaml_token_delete(&token);
        tokens++;
        if (done) break;
    }
    yaml_parser_delete(&parser);
    int errs = tracker_check(name);
    ASSERT(errs == 0, "scan should have zero leaks and zero double-frees");
    printf("    tokens=%d, allocs=%d, frees=%d, peak=%d\n",
           tokens, tracker_total_allocs, tracker_total_frees, tracker_peak_allocs);
}

static void test_load_string(const char *name, const char *yaml_input) {
    printf("  load [%s]...\n", name);
    tracker_start();
    yaml_parser_t parser;
    yaml_document_t doc;
    int ok = yaml_parser_initialize(&parser);
    ASSERT(ok, "parser init should succeed");
    yaml_parser_set_input_string(&parser,
        (const unsigned char *)yaml_input, strlen(yaml_input));
    int docs = 0;
    while (1) {
        ok = yaml_parser_load(&parser, &doc);
        if (!ok) break;
        int has_root = (yaml_document_get_root_node(&doc) != NULL);
        yaml_document_delete(&doc);
        if (!has_root) break;
        docs++;
    }
    yaml_parser_delete(&parser);
    int errs = tracker_check(name);
    ASSERT(errs == 0, "load should have zero leaks and zero double-frees");
    printf("    docs=%d, allocs=%d, frees=%d, peak=%d\n",
           docs, tracker_total_allocs, tracker_total_frees, tracker_peak_allocs);
}

static int emit_write_handler(void *data, unsigned char *buffer, size_t size) {
    (void)data; (void)buffer; (void)size;
    return 1;
}

static void test_emit_simple(void) {
    printf("  emit simple document...\n");
    tracker_start();
    yaml_emitter_t emitter;
    yaml_event_t event;
    int ok = yaml_emitter_initialize(&emitter);
    ASSERT(ok, "emitter init should succeed");
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
    yaml_emitter_delete(&emitter);

    int errs = tracker_check("emit_simple");
    ASSERT(errs == 0, "emit should have zero leaks and zero double-frees");
    printf("    allocs=%d, frees=%d, peak=%d\n",
           tracker_total_allocs, tracker_total_frees, tracker_peak_allocs);
}

static void test_document_build_and_dump(void) {
    printf("  document build and dump...\n");
    tracker_start();

    /* Build a document */
    yaml_document_t doc;
    int ok = yaml_document_initialize(&doc, NULL, NULL, NULL, 1, 1);
    ASSERT(ok, "doc init should succeed");
    int root = yaml_document_add_mapping(&doc, NULL, YAML_BLOCK_MAPPING_STYLE);
    int key1 = yaml_document_add_scalar(&doc, NULL, (yaml_char_t *)"name", 4, YAML_PLAIN_SCALAR_STYLE);
    int val1 = yaml_document_add_scalar(&doc, NULL, (yaml_char_t *)"test", 4, YAML_PLAIN_SCALAR_STYLE);
    yaml_document_append_mapping_pair(&doc, root, key1, val1);
    int key2 = yaml_document_add_scalar(&doc, NULL, (yaml_char_t *)"items", 5, YAML_PLAIN_SCALAR_STYLE);
    int seq = yaml_document_add_sequence(&doc, NULL, YAML_BLOCK_SEQUENCE_STYLE);
    yaml_document_append_mapping_pair(&doc, root, key2, seq);
    int item1 = yaml_document_add_scalar(&doc, NULL, (yaml_char_t *)"a", 1, YAML_PLAIN_SCALAR_STYLE);
    int item2 = yaml_document_add_scalar(&doc, NULL, (yaml_char_t *)"b", 1, YAML_PLAIN_SCALAR_STYLE);
    yaml_document_append_sequence_item(&doc, seq, item1);
    yaml_document_append_sequence_item(&doc, seq, item2);

    /* Dump via emitter */
    yaml_emitter_t emitter;
    ok = yaml_emitter_initialize(&emitter);
    ASSERT(ok, "emitter init should succeed");
    yaml_emitter_set_output(&emitter, emit_write_handler, NULL);
    yaml_emitter_open(&emitter);
    yaml_emitter_dump(&emitter, &doc);
    yaml_emitter_close(&emitter);
    yaml_emitter_delete(&emitter);
    /* doc is consumed by dump, but delete should be safe */
    yaml_document_delete(&doc);

    int errs = tracker_check("document_build_and_dump");
    ASSERT(errs == 0, "document build+dump should have zero leaks and zero double-frees");
    printf("    allocs=%d, frees=%d, peak=%d\n",
           tracker_total_allocs, tracker_total_frees, tracker_peak_allocs);
}

static void test_roundtrip(const char *name, const char *yaml_input) {
    printf("  roundtrip [%s]...\n", name);
    tracker_start();

    /* Parse to document */
    yaml_parser_t parser;
    yaml_document_t doc;
    int ok = yaml_parser_initialize(&parser);
    ASSERT(ok, "parser init should succeed");
    yaml_parser_set_input_string(&parser,
        (const unsigned char *)yaml_input, strlen(yaml_input));
    ok = yaml_parser_load(&parser, &doc);
    if (!ok) {
        yaml_parser_delete(&parser);
        tracker_check(name);
        ASSERT(0, "load failed");
        return;
    }
    yaml_parser_delete(&parser);

    /* Emit back */
    yaml_emitter_t emitter;
    ok = yaml_emitter_initialize(&emitter);
    ASSERT(ok, "emitter init should succeed");
    yaml_emitter_set_output(&emitter, emit_write_handler, NULL);
    yaml_emitter_open(&emitter);
    yaml_emitter_dump(&emitter, &doc);
    yaml_emitter_close(&emitter);
    yaml_emitter_delete(&emitter);
    yaml_document_delete(&doc);

    /* Load remaining empty doc to drain parser state */
    int errs = tracker_check(name);
    ASSERT(errs == 0, "roundtrip should have zero leaks and zero double-frees");
    printf("    allocs=%d, frees=%d, peak=%d\n",
           tracker_total_allocs, tracker_total_frees, tracker_peak_allocs);
}

static void test_error_path(void) {
    printf("  error path (invalid YAML)...\n");
    tracker_start();
    const char *bad_yaml = ":\n  - - :\n    :\n[{";
    yaml_parser_t parser;
    yaml_event_t event;
    int ok = yaml_parser_initialize(&parser);
    ASSERT(ok, "parser init should succeed");
    yaml_parser_set_input_string(&parser,
        (const unsigned char *)bad_yaml, strlen(bad_yaml));
    while (1) {
        ok = yaml_parser_parse(&parser, &event);
        if (!ok) break;
        int done = (event.type == YAML_STREAM_END_EVENT);
        yaml_event_delete(&event);
        if (done) break;
    }
    yaml_parser_delete(&parser);
    int errs = tracker_check("error_path");
    ASSERT(errs == 0, "error path should have zero leaks and zero double-frees");
    printf("    allocs=%d, frees=%d\n",
           tracker_total_allocs, tracker_total_frees);
}

static void test_load_error_path(void) {
    printf("  load error path (invalid YAML)...\n");
    tracker_start();
    const char *bad_yaml = "foo: [bar\n  baz: {\nqux";
    yaml_parser_t parser;
    yaml_document_t doc;
    int ok = yaml_parser_initialize(&parser);
    ASSERT(ok, "parser init should succeed");
    yaml_parser_set_input_string(&parser,
        (const unsigned char *)bad_yaml, strlen(bad_yaml));
    ok = yaml_parser_load(&parser, &doc);
    if (!ok) {
        /* Error path -- doc should still be safe to delete */
        yaml_document_delete(&doc);
    } else {
        yaml_document_delete(&doc);
    }
    yaml_parser_delete(&parser);
    int errs = tracker_check("load_error_path");
    ASSERT(errs == 0, "load error path should have zero leaks and zero double-frees");
    printf("    allocs=%d, frees=%d\n",
           tracker_total_allocs, tracker_total_frees);
}

static void test_event_init_delete(void) {
    printf("  event init/delete cycles...\n");
    tracker_start();

    yaml_event_t event;

    /* Scalar with anchor and tag */
    yaml_scalar_event_initialize(&event, (yaml_char_t *)"anchor",
        (yaml_char_t *)"!tag", (yaml_char_t *)"value", 5, 0, 0,
        YAML_PLAIN_SCALAR_STYLE);
    yaml_event_delete(&event);

    /* Document start with version and tags */
    yaml_tag_directive_t tags[] = {
        { (yaml_char_t *)"!e!", (yaml_char_t *)"tag:example.com,2000:" },
    };
    yaml_version_directive_t version = { 1, 1 };
    yaml_document_start_event_initialize(&event, &version, tags, tags + 1, 0);
    yaml_event_delete(&event);

    /* Sequence with anchor */
    yaml_sequence_start_event_initialize(&event, (yaml_char_t *)"seq_anchor",
        (yaml_char_t *)"!seq_tag", 0, YAML_BLOCK_SEQUENCE_STYLE);
    yaml_event_delete(&event);

    /* Mapping with anchor */
    yaml_mapping_start_event_initialize(&event, (yaml_char_t *)"map_anchor",
        (yaml_char_t *)"!map_tag", 0, YAML_BLOCK_MAPPING_STYLE);
    yaml_event_delete(&event);

    /* Alias */
    yaml_alias_event_initialize(&event, (yaml_char_t *)"alias_target");
    yaml_event_delete(&event);

    int errs = tracker_check("event_init_delete");
    ASSERT(errs == 0, "event init/delete should have zero leaks and zero double-frees");
    printf("    allocs=%d, frees=%d\n",
           tracker_total_allocs, tracker_total_frees);
}

static void test_token_delete_idempotent(void) {
    printf("  token delete idempotency...\n");
    tracker_start();
    yaml_parser_t parser;
    yaml_token_t token;
    const char *input = "key: value\n";
    int ok = yaml_parser_initialize(&parser);
    ASSERT(ok, "parser init should succeed");
    yaml_parser_set_input_string(&parser,
        (const unsigned char *)input, strlen(input));

    /* Get first token, delete it twice */
    ok = yaml_parser_scan(&parser, &token);
    ASSERT(ok, "scan should succeed");
    yaml_token_delete(&token);
    yaml_token_delete(&token); /* Should be safe (idempotent) */

    /* Drain remaining tokens */
    while (1) {
        ok = yaml_parser_scan(&parser, &token);
        if (!ok) break;
        int done = (token.type == YAML_STREAM_END_TOKEN);
        yaml_token_delete(&token);
        if (done) break;
    }
    yaml_parser_delete(&parser);
    int errs = tracker_check("token_delete_idempotent");
    ASSERT(errs == 0, "token double delete should be safe");
    printf("    allocs=%d, frees=%d\n",
           tracker_total_allocs, tracker_total_frees);
}

/* ================================================================
 * YAML inputs
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
    "double: \"escape\\n\\t\\\"chars\"\n";

static const char *yaml_nested =
    "root:\n"
    "  level1:\n"
    "    level2:\n"
    "      level3:\n"
    "        - item1\n"
    "        - item2\n"
    "        - nested:\n"
    "            deep: value\n";

/* ================================================================ */

int main(void) {
    printf("Allocation Tracker Tests\n");
    printf("========================\n\n");

    /* Init/delete symmetry */
    test_parser_init_delete();
    test_emitter_init_delete();
    test_document_init_delete();
    test_document_double_delete();
    test_event_init_delete();
    test_token_delete_idempotent();

    printf("\n--- Scan tests ---\n");
    test_scan_string("simple", yaml_simple);
    test_scan_string("complex", yaml_complex);
    test_scan_string("anchors", yaml_anchors);
    test_scan_string("flow", yaml_flow);
    test_scan_string("multiline", yaml_multiline);
    test_scan_string("tags", yaml_tags);
    test_scan_string("multi_doc", yaml_multi_doc);
    test_scan_string("quoted", yaml_quoted);
    test_scan_string("nested", yaml_nested);

    printf("\n--- Parse tests ---\n");
    test_parse_string("simple", yaml_simple);
    test_parse_string("complex", yaml_complex);
    test_parse_string("anchors", yaml_anchors);
    test_parse_string("flow", yaml_flow);
    test_parse_string("multiline", yaml_multiline);
    test_parse_string("tags", yaml_tags);
    test_parse_string("multi_doc", yaml_multi_doc);
    test_parse_string("quoted", yaml_quoted);
    test_parse_string("nested", yaml_nested);

    printf("\n--- Load tests ---\n");
    test_load_string("simple", yaml_simple);
    test_load_string("complex", yaml_complex);
    test_load_string("anchors", yaml_anchors);
    test_load_string("flow", yaml_flow);
    test_load_string("multiline", yaml_multiline);
    test_load_string("tags", yaml_tags);
    test_load_string("multi_doc", yaml_multi_doc);
    test_load_string("quoted", yaml_quoted);
    test_load_string("nested", yaml_nested);

    printf("\n--- Emit tests ---\n");
    test_emit_simple();
    test_document_build_and_dump();

    printf("\n--- Roundtrip tests ---\n");
    test_roundtrip("simple", yaml_simple);
    test_roundtrip("complex", yaml_complex);
    test_roundtrip("anchors", yaml_anchors);
    test_roundtrip("flow", yaml_flow);
    test_roundtrip("multiline", yaml_multiline);
    test_roundtrip("nested", yaml_nested);

    printf("\n--- Error path tests ---\n");
    test_error_path();
    test_load_error_path();

    printf("\n========================\n");
    printf("Results: %d/%d passed", tests_passed, tests_run);
    if (tests_failed > 0) printf(", %d FAILED", tests_failed);
    printf("\n");
    if (total_leaks > 0)
        printf("TOTAL LEAKS DETECTED: %d\n", total_leaks);

    return tests_failed > 0 ? 1 : 0;
}
