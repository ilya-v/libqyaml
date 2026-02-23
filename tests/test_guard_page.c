/*
 * Guard-Page Allocator Test (Requirement 4.6)
 *
 * Implements a custom guard-page allocator using mmap/mprotect.
 * Every allocation is placed right-aligned against an inaccessible guard page,
 * so any buffer overrun (even 1 byte) immediately triggers a segfault.
 *
 * Strategy:
 *   - For each malloc(N), allocate ceil((N + sizeof(header)) / PAGE_SIZE) + 1 pages
 *   - The last page is marked PROT_NONE (guard page)
 *   - The allocation is placed at the end of the usable region, right-aligned
 *     to the guard page boundary
 *   - A header before the allocation stores the mmap base and total size for free()
 *   - On free(), the entire mmap region is munmapped
 *
 * This catches:
 *   - Buffer overruns (write past end of allocation)
 *   - Read past end of allocation
 *   - Use-after-free (munmap makes the entire region inaccessible)
 *
 * Uses --wrap linker flags to intercept malloc/realloc/calloc/free/strdup.
 *
 * Note: This allocator is SLOW and uses a LOT of virtual memory. It is only
 * suitable for correctness testing, not performance testing.
 *
 * Build:
 *   cc -O0 -g -DYAML_DECLARE_STATIC -I../include test_guard_page.c \
 *      ../build/libqyaml.a \
 *      -Wl,--wrap=malloc -Wl,--wrap=realloc -Wl,--wrap=calloc \
 *      -Wl,--wrap=free -Wl,--wrap=strdup \
 *      -o test-guard-page
 */

#include <yaml.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <signal.h>
#include <setjmp.h>

/* Real libc functions provided by --wrap */
extern void *__real_malloc(size_t size);
extern void *__real_realloc(void *ptr, size_t size);
extern void *__real_calloc(size_t nmemb, size_t size);
extern void  __real_free(void *ptr);
extern char *__real_strdup(const char *s);

static long page_size;
static int guard_enabled = 0;
static int total_allocs = 0;
static int total_frees = 0;

/* Header placed before each allocation to track the mmap region */
typedef struct {
    void  *mmap_base;    /* Start of mmap'd region */
    size_t mmap_size;    /* Total mmap'd size */
    size_t alloc_size;   /* Requested allocation size */
    unsigned int magic;  /* Magic number for validation */
} guard_header_t;

#define GUARD_MAGIC 0xDEADBEEF

/* Round up to next multiple of page_size */
static size_t round_up_page(size_t n) {
    return (n + (size_t)page_size - 1) & ~((size_t)page_size - 1);
}

static void *guard_alloc(size_t size) {
    if (size == 0) size = 1; /* Avoid zero-size edge case */

    /* We need space for the header + the allocation, plus one guard page.
     * Place the allocation right-aligned to the guard page. */
    size_t needed = sizeof(guard_header_t) + size;
    size_t data_pages = round_up_page(needed);
    size_t total_size = data_pages + (size_t)page_size; /* +1 guard page */

    void *base = mmap(NULL, total_size, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (base == MAP_FAILED) return NULL;

    /* Make the last page a guard page (inaccessible) */
    void *guard = (char *)base + data_pages;
    if (mprotect(guard, (size_t)page_size, PROT_NONE) != 0) {
        munmap(base, total_size);
        return NULL;
    }

    /* Place allocation right-aligned to the guard page boundary.
     * The allocation ends exactly at the guard page start. */
    void *alloc_end = guard;
    void *alloc_start = (char *)alloc_end - size;

    /* Place header just before the allocation */
    guard_header_t *header = (guard_header_t *)((char *)alloc_start - sizeof(guard_header_t));

    /* Verify header fits in the mapped region */
    if ((void *)header < base) {
        munmap(base, total_size);
        return NULL;
    }

    header->mmap_base = base;
    header->mmap_size = total_size;
    header->alloc_size = size;
    header->magic = GUARD_MAGIC;

    total_allocs++;
    return alloc_start;
}

static void guard_free(void *ptr) {
    if (!ptr) return;

    guard_header_t *header = (guard_header_t *)((char *)ptr - sizeof(guard_header_t));

    if (header->magic != GUARD_MAGIC) {
        fprintf(stderr, "GUARD-PAGE: free() called on invalid pointer %p "
                "(bad magic: 0x%x)\n", ptr, header->magic);
        return;
    }

    /* Invalidate magic to detect double-free */
    header->magic = 0;

    /* Unmap the entire region -- any subsequent access to this
     * allocation (use-after-free) will segfault */
    munmap(header->mmap_base, header->mmap_size);
    total_frees++;
}

static size_t guard_get_size(void *ptr) {
    if (!ptr) return 0;
    guard_header_t *header = (guard_header_t *)((char *)ptr - sizeof(guard_header_t));
    if (header->magic != GUARD_MAGIC) return 0;
    return header->alloc_size;
}

/* ================================================================
 * Wrapped allocation functions
 * ================================================================ */

void *__wrap_malloc(size_t size) {
    if (!guard_enabled) return __real_malloc(size);
    return guard_alloc(size);
}

void *__wrap_calloc(size_t nmemb, size_t size) {
    if (!guard_enabled) return __real_calloc(nmemb, size);
    size_t total = nmemb * size;
    void *ptr = guard_alloc(total);
    if (ptr) memset(ptr, 0, total);
    return ptr;
}

void *__wrap_realloc(void *old_ptr, size_t new_size) {
    if (!guard_enabled) return __real_realloc(old_ptr, new_size);

    if (!old_ptr) return guard_alloc(new_size);
    if (new_size == 0) {
        guard_free(old_ptr);
        return NULL;
    }

    size_t old_size = guard_get_size(old_ptr);
    void *new_ptr = guard_alloc(new_size);
    if (!new_ptr) return NULL;

    size_t copy_size = old_size < new_size ? old_size : new_size;
    memcpy(new_ptr, old_ptr, copy_size);
    guard_free(old_ptr);
    return new_ptr;
}

void __wrap_free(void *ptr) {
    if (!guard_enabled) { __real_free(ptr); return; }
    guard_free(ptr);
}

char *__wrap_strdup(const char *s) {
    if (!guard_enabled) return __real_strdup(s);
    if (!s) return NULL;
    size_t len = strlen(s) + 1;
    char *dup = (char *)guard_alloc(len);
    if (dup) memcpy(dup, s, len);
    return dup;
}

/* ================================================================
 * Test framework
 * ================================================================ */

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT(cond, msg) do { \
    tests_run++; \
    if (!(cond)) { \
        fprintf(stderr, "  FAIL [%s:%d]: %s\n", __FILE__, __LINE__, msg); \
        tests_failed++; \
    } else { \
        tests_passed++; \
    } \
} while(0)

static void guard_start(void) {
    total_allocs = 0;
    total_frees = 0;
    guard_enabled = 1;
}

static void guard_stop(void) {
    guard_enabled = 0;
}

/* ================================================================
 * Test cases: run various YAML operations under guard-page allocator.
 * If there are any buffer overruns, the process will segfault.
 * Surviving all tests = no overruns detected.
 * ================================================================ */

static void test_parser_init_delete(void) {
    printf("  parser init/delete...\n");
    guard_start();
    yaml_parser_t parser;
    int ok = yaml_parser_initialize(&parser);
    ASSERT(ok, "parser init should succeed");
    yaml_parser_delete(&parser);
    guard_stop();
    printf("    allocs=%d, frees=%d\n", total_allocs, total_frees);
    ASSERT(total_allocs == total_frees, "all allocations freed");
}

static void test_emitter_init_delete(void) {
    printf("  emitter init/delete...\n");
    guard_start();
    yaml_emitter_t emitter;
    int ok = yaml_emitter_initialize(&emitter);
    ASSERT(ok, "emitter init should succeed");
    yaml_emitter_delete(&emitter);
    guard_stop();
    printf("    allocs=%d, frees=%d\n", total_allocs, total_frees);
    ASSERT(total_allocs == total_frees, "all allocations freed");
}

static void test_scan_string(const char *name, const char *yaml_input) {
    printf("  scan [%s]...\n", name);
    guard_start();
    yaml_parser_t parser;
    yaml_token_t token;
    int ok = yaml_parser_initialize(&parser);
    ASSERT(ok, "parser init");
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
    guard_stop();
    printf("    tokens=%d, allocs=%d, frees=%d\n", tokens, total_allocs, total_frees);
    ASSERT(total_allocs == total_frees, "all allocations freed");
}

static void test_parse_string(const char *name, const char *yaml_input) {
    printf("  parse [%s]...\n", name);
    guard_start();
    yaml_parser_t parser;
    yaml_event_t event;
    int ok = yaml_parser_initialize(&parser);
    ASSERT(ok, "parser init");
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
    guard_stop();
    printf("    events=%d, allocs=%d, frees=%d\n", events, total_allocs, total_frees);
    ASSERT(total_allocs == total_frees, "all allocations freed");
}

static void test_load_string(const char *name, const char *yaml_input) {
    printf("  load [%s]...\n", name);
    guard_start();
    yaml_parser_t parser;
    yaml_document_t doc;
    int ok = yaml_parser_initialize(&parser);
    ASSERT(ok, "parser init");
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
    guard_stop();
    printf("    docs=%d, allocs=%d, frees=%d\n", docs, total_allocs, total_frees);
    ASSERT(total_allocs == total_frees, "all allocations freed");
}

static int emit_write_handler(void *data, unsigned char *buffer, size_t size) {
    (void)data; (void)buffer; (void)size;
    return 1;
}

static void test_emit_simple(void) {
    printf("  emit simple document...\n");
    guard_start();
    yaml_emitter_t emitter;
    yaml_event_t event;
    int ok = yaml_emitter_initialize(&emitter);
    ASSERT(ok, "emitter init");
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

    guard_stop();
    printf("    allocs=%d, frees=%d\n", total_allocs, total_frees);
    ASSERT(total_allocs == total_frees, "all allocations freed");
}

static void test_roundtrip(const char *name, const char *yaml_input) {
    printf("  roundtrip [%s]...\n", name);
    guard_start();

    yaml_parser_t parser;
    yaml_document_t doc;
    int ok = yaml_parser_initialize(&parser);
    ASSERT(ok, "parser init");
    yaml_parser_set_input_string(&parser,
        (const unsigned char *)yaml_input, strlen(yaml_input));
    ok = yaml_parser_load(&parser, &doc);
    if (!ok) {
        yaml_parser_delete(&parser);
        guard_stop();
        ASSERT(0, "load failed");
        return;
    }
    yaml_parser_delete(&parser);

    yaml_emitter_t emitter;
    ok = yaml_emitter_initialize(&emitter);
    ASSERT(ok, "emitter init");
    yaml_emitter_set_output(&emitter, emit_write_handler, NULL);
    yaml_emitter_open(&emitter);
    yaml_emitter_dump(&emitter, &doc);
    yaml_emitter_close(&emitter);
    yaml_emitter_delete(&emitter);
    yaml_document_delete(&doc);

    guard_stop();
    printf("    allocs=%d, frees=%d\n", total_allocs, total_frees);
    ASSERT(total_allocs == total_frees, "all allocations freed");
}

static void test_error_path(void) {
    printf("  error path (invalid YAML)...\n");
    guard_start();
    const char *bad_yaml = ":\n  - - :\n    :\n[{";
    yaml_parser_t parser;
    yaml_event_t event;
    int ok = yaml_parser_initialize(&parser);
    ASSERT(ok, "parser init");
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
    guard_stop();
    printf("    allocs=%d, frees=%d\n", total_allocs, total_frees);
    ASSERT(total_allocs == total_frees, "all allocations freed");
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
    page_size = sysconf(_SC_PAGESIZE);
    if (page_size <= 0) page_size = 4096;

    printf("Guard-Page Allocator Tests\n");
    printf("==========================\n");
    printf("Page size: %ld bytes\n\n", page_size);
    printf("Any buffer overrun will cause an immediate segfault.\n");
    printf("Surviving all tests = no overruns detected.\n\n");

    /* Init/delete */
    test_parser_init_delete();
    test_emitter_init_delete();

    /* Scan */
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

    /* Parse */
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

    /* Load */
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

    /* Emit */
    printf("\n--- Emit tests ---\n");
    test_emit_simple();

    /* Roundtrip */
    printf("\n--- Roundtrip tests ---\n");
    test_roundtrip("simple", yaml_simple);
    test_roundtrip("complex", yaml_complex);
    test_roundtrip("anchors", yaml_anchors);
    test_roundtrip("flow", yaml_flow);
    test_roundtrip("multiline", yaml_multiline);
    test_roundtrip("nested", yaml_nested);

    /* Error paths */
    printf("\n--- Error path tests ---\n");
    test_error_path();

    printf("\n==========================\n");
    printf("Results: %d/%d passed", tests_passed, tests_run);
    if (tests_failed > 0) printf(", %d FAILED", tests_failed);
    printf("\n");
    printf("NO BUFFER OVERRUNS DETECTED (process survived all tests)\n");

    return tests_failed > 0 ? 1 : 0;
}
