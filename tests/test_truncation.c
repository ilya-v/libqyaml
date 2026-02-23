#define _GNU_SOURCE

/*
 * Input truncation testing (REQUIREMENTS.md 4.7):
 * Feed every valid input truncated at every byte offset.
 * The parser must return an error cleanly, never crash.
 *
 * Strategy: for each test input, iterate len from 0 to strlen(input)-1,
 * feed len bytes to scanner, parser, and loader. Each must either:
 *   - succeed (partial input happened to be valid)
 *   - return an error (expected for truncated input)
 * It must NEVER crash, hang, or leak.
 *
 * We also test with a tight timeout via setjmp/longjmp to catch hangs.
 */

#include "test_helper.h"
#include <signal.h>
#include <setjmp.h>

static sigjmp_buf timeout_jmp;
static volatile sig_atomic_t alarm_fired;

static void alarm_handler(int sig) {
    (void)sig;
    alarm_fired = 1;
    siglongjmp(timeout_jmp, 1);
}

/* Test truncated input through the scanner API */
static int try_scan_truncated(const unsigned char *data, size_t len) {
    yaml_parser_t parser;
    yaml_token_t token;

    if (!yaml_parser_initialize(&parser)) return 1; /* OOM is ok */
    yaml_parser_set_input_string(&parser, data, len);

    int done = 0;
    while (!done) {
        if (!yaml_parser_scan(&parser, &token)) {
            /* Error is expected for truncated input */
            yaml_parser_delete(&parser);
            return 1;
        }
        done = (token.type == YAML_STREAM_END_TOKEN);
        yaml_token_delete(&token);
    }

    yaml_parser_delete(&parser);
    return 1;
}

/* Test truncated input through the parser (event) API */
static int try_parse_truncated(const unsigned char *data, size_t len) {
    yaml_parser_t parser;
    yaml_event_t event;

    if (!yaml_parser_initialize(&parser)) return 1;
    yaml_parser_set_input_string(&parser, data, len);

    int done = 0;
    while (!done) {
        if (!yaml_parser_parse(&parser, &event)) {
            yaml_parser_delete(&parser);
            return 1;
        }
        done = (event.type == YAML_STREAM_END_EVENT);
        yaml_event_delete(&event);
    }

    yaml_parser_delete(&parser);
    return 1;
}

/* Test truncated input through the loader (document) API */
static int try_load_truncated(const unsigned char *data, size_t len) {
    yaml_parser_t parser;
    yaml_document_t document;

    if (!yaml_parser_initialize(&parser)) return 1;
    yaml_parser_set_input_string(&parser, data, len);

    while (yaml_parser_load(&parser, &document)) {
        int done = !yaml_document_get_root_node(&document);
        yaml_document_delete(&document);
        if (done) break;
    }
    /* Error or clean finish -- both acceptable */

    yaml_parser_delete(&parser);
    return 1;
}

/*
 * Run truncation test on a single input across all three API layers.
 * Returns number of failures (crashes caught by timeout).
 */
static int test_truncation_single(const char *name, const char *input) {
    size_t full_len = strlen(input);
    int failures = 0;
    struct sigaction sa, old_sa;

    /* Set up alarm handler for hang detection (2 second timeout per truncation) */
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = alarm_handler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGALRM, &sa, &old_sa);

    for (size_t trunc_len = 0; trunc_len < full_len; trunc_len++) {
        const unsigned char *data = (const unsigned char *)input;

        /* Scanner */
        alarm_fired = 0;
        if (sigsetjmp(timeout_jmp, 1) == 0) {
            alarm(2);
            try_scan_truncated(data, trunc_len);
            alarm(0);
        } else {
            fprintf(stderr, "  HANG: %s scan truncated at %zu/%zu\n",
                    name, trunc_len, full_len);
            failures++;
        }

        /* Parser */
        alarm_fired = 0;
        if (sigsetjmp(timeout_jmp, 1) == 0) {
            alarm(2);
            try_parse_truncated(data, trunc_len);
            alarm(0);
        } else {
            fprintf(stderr, "  HANG: %s parse truncated at %zu/%zu\n",
                    name, trunc_len, full_len);
            failures++;
        }

        /* Loader */
        alarm_fired = 0;
        if (sigsetjmp(timeout_jmp, 1) == 0) {
            alarm(2);
            try_load_truncated(data, trunc_len);
            alarm(0);
        } else {
            fprintf(stderr, "  HANG: %s load truncated at %zu/%zu\n",
                    name, trunc_len, full_len);
            failures++;
        }
    }

    /* Restore old signal handler */
    sigaction(SIGALRM, &old_sa, NULL);

    /* One assertion per input: all truncations handled cleanly */
    char msg[256];
    snprintf(msg, sizeof(msg), "%s: %zu truncation points x 3 APIs, %d hangs",
             name, full_len, failures);
    ASSERT(failures == 0, msg);

    return failures;
}

/* --- Test inputs: representative YAML covering all major constructs --- */

static const char *yaml_simple_scalar = "hello world";

static const char *yaml_mapping =
    "key1: value1\n"
    "key2: value2\n"
    "key3: value3\n";

static const char *yaml_nested_mapping =
    "parent:\n"
    "  child1: val1\n"
    "  child2: val2\n"
    "  nested:\n"
    "    deep: value\n";

static const char *yaml_sequence =
    "- item1\n"
    "- item2\n"
    "- item3\n";

static const char *yaml_nested_sequence =
    "- - nested1\n"
    "  - nested2\n"
    "- - nested3\n";

static const char *yaml_flow_mapping = "{key1: val1, key2: val2, key3: val3}";

static const char *yaml_flow_sequence = "[item1, item2, item3, item4]";

static const char *yaml_nested_flow = "{a: [1, 2, {b: [3, 4]}], c: {d: e}}";

static const char *yaml_double_quoted =
    "key: \"double \\\"quoted\\\" with \\n escapes\"\n";

static const char *yaml_single_quoted =
    "key: 'single ''quoted'' value'\n";

static const char *yaml_literal_block =
    "key: |\n"
    "  line1\n"
    "  line2\n"
    "  line3\n";

static const char *yaml_folded_block =
    "key: >\n"
    "  folded\n"
    "  text\n"
    "  here\n";

static const char *yaml_anchor_alias =
    "anchor: &ref value\n"
    "alias: *ref\n";

static const char *yaml_multi_doc =
    "---\n"
    "doc1: value1\n"
    "...\n"
    "---\n"
    "doc2: value2\n"
    "...\n";

static const char *yaml_tag =
    "tagged: !custom value\n"
    "typed: !!str 42\n";

static const char *yaml_complex_key =
    "? - complex\n"
    "  - key\n"
    ": value\n";

static const char *yaml_directive =
    "%YAML 1.1\n"
    "%TAG !e! tag:example.com,2000:\n"
    "---\n"
    "key: value\n";

static const char *yaml_comment_heavy =
    "# header comment\n"
    "key1: value1 # inline comment\n"
    "# block comment\n"
    "# continued\n"
    "key2: value2\n";

static const char *yaml_empty_values =
    "key1:\n"
    "key2: ~\n"
    "key3: null\n"
    "key4: \"\"\n";

static const char *yaml_mixed_indent =
    "level0:\n"
    "  level1a:\n"
    "    level2: deep\n"
    "  level1b: shallow\n";

static const char *yaml_unicode =
    "emoji: \xf0\x9f\x98\x80\n"     /* U+1F600 grinning face */
    "cjk: \xe4\xb8\xad\xe6\x96\x87\n" /* Chinese characters */
    "accent: caf\xc3\xa9\n";         /* cafe with accent */

static const char *yaml_kubernetes_like =
    "apiVersion: v1\n"
    "kind: Pod\n"
    "metadata:\n"
    "  name: my-pod\n"
    "  labels:\n"
    "    app: web\n"
    "spec:\n"
    "  containers:\n"
    "  - name: nginx\n"
    "    image: nginx:latest\n"
    "    ports:\n"
    "    - containerPort: 80\n";

static const char *yaml_backslash_escapes =
    "escapes: \"\\0\\a\\b\\t\\n\\v\\f\\r\\e\\ \\\"\\\\\"\n";

static const char *yaml_multiline_plain =
    "key: this is a\n"
    "  multiline plain\n"
    "  scalar value\n";

static const char *yaml_block_seq_in_map =
    "mapping:\n"
    "  key:\n"
    "  - item1\n"
    "  - item2\n"
    "  other: val\n";

/* BOM prefix */
static const char *yaml_bom =
    "\xef\xbb\xbf"  /* UTF-8 BOM */
    "key: value\n";

/* Large-ish input to stress buffer boundaries */
static char yaml_large_mapping[4096];
static void init_large_mapping(void) {
    int pos = 0;
    for (int i = 0; i < 100 && pos < 4000; i++) {
        pos += snprintf(yaml_large_mapping + pos,
                        sizeof(yaml_large_mapping) - pos,
                        "key_%03d: value_%03d\n", i, i);
    }
}

int main(void) {
    TEST_SUITE_BEGIN("Input Truncation");

    init_large_mapping();

    printf("  Testing truncation at every byte offset...\n");

    test_truncation_single("simple_scalar", yaml_simple_scalar);
    test_truncation_single("mapping", yaml_mapping);
    test_truncation_single("nested_mapping", yaml_nested_mapping);
    test_truncation_single("sequence", yaml_sequence);
    test_truncation_single("nested_sequence", yaml_nested_sequence);
    test_truncation_single("flow_mapping", yaml_flow_mapping);
    test_truncation_single("flow_sequence", yaml_flow_sequence);
    test_truncation_single("nested_flow", yaml_nested_flow);
    test_truncation_single("double_quoted", yaml_double_quoted);
    test_truncation_single("single_quoted", yaml_single_quoted);
    test_truncation_single("literal_block", yaml_literal_block);
    test_truncation_single("folded_block", yaml_folded_block);
    test_truncation_single("anchor_alias", yaml_anchor_alias);
    test_truncation_single("multi_doc", yaml_multi_doc);
    test_truncation_single("tag", yaml_tag);
    test_truncation_single("complex_key", yaml_complex_key);
    test_truncation_single("directive", yaml_directive);
    test_truncation_single("comment_heavy", yaml_comment_heavy);
    test_truncation_single("empty_values", yaml_empty_values);
    test_truncation_single("mixed_indent", yaml_mixed_indent);
    test_truncation_single("unicode", yaml_unicode);
    test_truncation_single("kubernetes_like", yaml_kubernetes_like);
    test_truncation_single("backslash_escapes", yaml_backslash_escapes);
    test_truncation_single("multiline_plain", yaml_multiline_plain);
    test_truncation_single("block_seq_in_map", yaml_block_seq_in_map);
    test_truncation_single("bom", yaml_bom);
    test_truncation_single("large_mapping", yaml_large_mapping);

    printf("  Total truncation points tested: ~%d per input x 3 APIs x 27 inputs\n",
           (int)strlen(yaml_kubernetes_like)); /* representative size */

    TEST_SUITE_END();
}
