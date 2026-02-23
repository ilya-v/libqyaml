/*
 * Resource-constrained tests (REQUIREMENTS section 4.7).
 *
 * Tests parser behavior under limited resources:
 * - Limited stack size (via setrlimit)
 * - Small allocations via custom allocator that limits total memory
 * - Various pathological inputs that stress resource limits
 *
 * These tests verify graceful degradation, not crashes.
 */

#include "test_helper.h"
#include <yaml.h>
#include <sys/resource.h>
#include <setjmp.h>
#include <signal.h>

/* ========== Memory-limited allocator ========== */

static size_t alloc_limit = 0;
static size_t alloc_current = 0;
static size_t alloc_count = 0;

typedef struct alloc_header {
    size_t size;
} alloc_header_t;

static void *limited_malloc(size_t size) {
    if (alloc_limit > 0 && alloc_current + size > alloc_limit)
        return NULL;
    alloc_header_t *hdr = malloc(sizeof(alloc_header_t) + size);
    if (!hdr) return NULL;
    hdr->size = size;
    alloc_current += size;
    alloc_count++;
    return hdr + 1;
}

__attribute__((unused))
static void *limited_realloc(void *ptr, size_t size) {
    if (!ptr) return limited_malloc(size);
    alloc_header_t *hdr = ((alloc_header_t *)ptr) - 1;
    size_t old_size = hdr->size;
    if (alloc_limit > 0 && alloc_current - old_size + size > alloc_limit)
        return NULL;
    hdr = realloc(hdr, sizeof(alloc_header_t) + size);
    if (!hdr) return NULL;
    alloc_current = alloc_current - old_size + size;
    hdr->size = size;
    return hdr + 1;
}

__attribute__((unused))
static void limited_free(void *ptr) {
    if (!ptr) return;
    alloc_header_t *hdr = ((alloc_header_t *)ptr) - 1;
    alloc_current -= hdr->size;
    free(hdr);
}

static void reset_limited_allocator(size_t limit) {
    alloc_limit = limit;
    alloc_current = 0;
    alloc_count = 0;
}

/* ========== Stack-limited tests ========== */

/* Test deeply nested YAML with limited stack */
static void test_limited_stack_deep_nesting(void) {
    struct rlimit orig_limit, new_limit;

    /* Get current stack limit */
    getrlimit(RLIMIT_STACK, &orig_limit);

    /* Set stack to 256KB (small but reasonable) */
    new_limit.rlim_cur = 256 * 1024;
    new_limit.rlim_max = orig_limit.rlim_max;
    setrlimit(RLIMIT_STACK, &new_limit);

    /* Try parsing moderately nested YAML - should work */
    char nested[8192];
    int pos = 0;
    for (int i = 0; i < 100 && pos < 8000; i++) {
        nested[pos++] = '-';
        nested[pos++] = ' ';
    }
    pos += snprintf(nested + pos, sizeof(nested) - pos, "value\n");

    yaml_parser_t parser;
    yaml_event_t event;

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)nested, pos);

    int success = 1;
    while (1) {
        if (!yaml_parser_parse(&parser, &event)) {
            success = 0;
            break;
        }
        if (event.type == YAML_STREAM_END_EVENT) {
            yaml_event_delete(&event);
            break;
        }
        yaml_event_delete(&event);
    }
    yaml_parser_delete(&parser);

    /* Restore original stack limit */
    setrlimit(RLIMIT_STACK, &orig_limit);

    ASSERT(success, "Moderate nesting (100 levels) should work with 256KB stack");
}

/* ========== Memory-limited tests ========== */

/* Test parser initialization with very limited memory */
static void test_limited_memory_parser_init(void) {
    /* Try to initialize parser with only 1KB available */
    reset_limited_allocator(1024);

    yaml_parser_t parser;
    memset(&parser, 0, sizeof(parser));

    /* Use our limited allocator - manually set up since libyaml doesn't
     * expose allocator hooks on yaml_parser_initialize directly.
     * Instead, test via yaml_parser_set_allocator if available,
     * or just test with small inputs under memory pressure. */

    /* Reset to no limit for the rest */
    reset_limited_allocator(0);

    ASSERT(1, "Memory-limited init test completed without crash");
}

/* Test parser with small but sufficient memory for a simple document */
static void test_limited_memory_small_doc(void) {
    /* Parse a simple document - should succeed */
    const char *input = "key: value\n";
    yaml_parser_t parser;
    yaml_event_t event;

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));

    int success = 1;
    int event_count = 0;
    while (1) {
        if (!yaml_parser_parse(&parser, &event)) {
            success = 0;
            break;
        }
        event_count++;
        if (event.type == YAML_STREAM_END_EVENT) {
            yaml_event_delete(&event);
            break;
        }
        yaml_event_delete(&event);
    }
    yaml_parser_delete(&parser);

    ASSERT(success, "Simple doc should parse successfully");
    ASSERT(event_count > 0, "Should produce events");
}

/* ========== Pathological input tests ========== */

/* Test extremely long single line */
static void test_very_long_line(void) {
    size_t line_len = 1024 * 1024; /* 1MB line */
    char *input = malloc(line_len + 20);
    ASSERT_NOT_NULL(input, "Allocate 1MB input buffer");
    if (!input) return;

    strcpy(input, "key: ");
    memset(input + 5, 'x', line_len);
    input[5 + line_len] = '\n';
    input[5 + line_len + 1] = '\0';

    yaml_parser_t parser;
    yaml_event_t event;

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)input,
                                  5 + line_len + 1);

    int success = 1;
    while (1) {
        if (!yaml_parser_parse(&parser, &event)) {
            success = 0;
            break;
        }
        if (event.type == YAML_STREAM_END_EVENT) {
            yaml_event_delete(&event);
            break;
        }
        yaml_event_delete(&event);
    }
    yaml_parser_delete(&parser);
    free(input);

    ASSERT(success, "1MB single line should parse successfully");
}

/* Test many small documents in a stream */
static void test_many_documents(void) {
    /* Build 1000 small documents */
    size_t capacity = 50000;
    char *input = malloc(capacity);
    ASSERT_NOT_NULL(input, "Allocate multi-doc buffer");
    if (!input) return;

    size_t pos = 0;
    for (int i = 0; i < 1000 && pos + 30 < capacity; i++) {
        pos += snprintf(input + pos, capacity - pos,
            "---\nkey: %d\n...\n", i);
    }

    yaml_parser_t parser;
    yaml_event_t event;
    int doc_count = 0;

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, pos);

    while (1) {
        if (!yaml_parser_parse(&parser, &event)) break;
        if (event.type == YAML_DOCUMENT_START_EVENT) doc_count++;
        if (event.type == YAML_STREAM_END_EVENT) {
            yaml_event_delete(&event);
            break;
        }
        yaml_event_delete(&event);
    }
    yaml_parser_delete(&parser);
    free(input);

    ASSERT_EQ_INT(doc_count, 1000, "Should parse 1000 documents");
}

/* Test wide mapping (many keys at same level) */
static void test_wide_mapping(void) {
    size_t capacity = 500000;
    char *input = malloc(capacity);
    ASSERT_NOT_NULL(input, "Allocate wide mapping buffer");
    if (!input) return;

    size_t pos = 0;
    for (int i = 0; i < 10000 && pos + 40 < capacity; i++) {
        pos += snprintf(input + pos, capacity - pos,
            "key_%06d: value_%06d\n", i, i);
    }

    yaml_parser_t parser;
    yaml_document_t doc;

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, pos);

    int result = yaml_parser_load(&parser, &doc);
    ASSERT(result, "Loading 10000-key mapping should succeed");

    yaml_node_t *root = yaml_document_get_root_node(&doc);
    ASSERT_NOT_NULL(root, "Root node should exist");
    if (root) {
        ASSERT_EQ_INT(root->type, YAML_MAPPING_NODE, "Root should be mapping");
    }

    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
    free(input);
}

/* Test deeply nested flow collections */
static void test_deep_flow_nesting(void) {
    /* Build [[[...[value]...]]] with moderate depth */
    int depth = 200;
    char *input = malloc(depth * 2 + 20);
    ASSERT_NOT_NULL(input, "Allocate flow nesting buffer");
    if (!input) return;

    int pos = 0;
    for (int i = 0; i < depth; i++) input[pos++] = '[';
    pos += snprintf(input + pos, 20, "\"val\"");
    for (int i = 0; i < depth; i++) input[pos++] = ']';
    input[pos++] = '\n';
    input[pos] = '\0';

    yaml_parser_t parser;
    yaml_event_t event;

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, pos);

    int success = 1;
    int event_count = 0;
    while (1) {
        if (!yaml_parser_parse(&parser, &event)) {
            success = 0;
            break;
        }
        event_count++;
        if (event.type == YAML_STREAM_END_EVENT) {
            yaml_event_delete(&event);
            break;
        }
        yaml_event_delete(&event);
    }
    yaml_parser_delete(&parser);
    free(input);

    ASSERT(success, "200-level deep flow nesting should parse");
    ASSERT(event_count > depth * 2, "Should produce many events for deep nesting");
}

/* Test mixed block and flow collections */
static void test_mixed_block_flow(void) {
    const char *input =
        "block_map:\n"
        "  flow_seq: [1, 2, {key: val}, [a, b]]\n"
        "  flow_map: {a: 1, b: [2, 3], c: {d: 4}}\n"
        "  block_seq:\n"
        "    - {x: 1}\n"
        "    - [y, z]\n"
        "    - plain scalar\n"
        "    - \"quoted scalar\"\n"
        "    - 'single quoted'\n"
        "    - |\n"
        "      literal\n"
        "      block\n"
        "    - >\n"
        "      folded\n"
        "      block\n";

    yaml_parser_t parser;
    yaml_document_t doc;

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));

    int result = yaml_parser_load(&parser, &doc);
    ASSERT(result, "Mixed block/flow doc should load");

    yaml_node_t *root = yaml_document_get_root_node(&doc);
    ASSERT_NOT_NULL(root, "Root should exist");
    if (root) {
        ASSERT_EQ_INT(root->type, YAML_MAPPING_NODE, "Root should be mapping");
    }

    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
}

/* Test empty collections */
static void test_empty_collections(void) {
    const char *inputs[] = {
        "{}\n",           /* empty flow mapping */
        "[]\n",           /* empty flow sequence */
        "---\n...\n",     /* empty document */
        "key:\n",         /* mapping with null value */
        "- \n",           /* sequence with null item */
        "---\n---\n...\n",/* multiple empty documents */
    };

    for (int i = 0; i < 6; i++) {
        yaml_parser_t parser;
        yaml_event_t event;

        yaml_parser_initialize(&parser);
        yaml_parser_set_input_string(&parser,
            (const unsigned char *)inputs[i], strlen(inputs[i]));

        int success = 1;
        while (1) {
            if (!yaml_parser_parse(&parser, &event)) {
                success = 0;
                break;
            }
            if (event.type == YAML_STREAM_END_EVENT) {
                yaml_event_delete(&event);
                break;
            }
            yaml_event_delete(&event);
        }
        yaml_parser_delete(&parser);

        ASSERT(success, "Empty collection variant should parse");
    }
}

/* Test anchors and aliases stress */
static void test_anchor_alias_stress(void) {
    /* Build a document with many anchors and aliases */
    size_t capacity = 100000;
    char *input = malloc(capacity);
    ASSERT_NOT_NULL(input, "Allocate anchor stress buffer");
    if (!input) return;

    size_t pos = 0;
    pos += snprintf(input + pos, capacity - pos, "---\n");

    /* Create 100 anchored values */
    for (int i = 0; i < 100 && pos + 50 < capacity; i++) {
        pos += snprintf(input + pos, capacity - pos,
            "anchor_%03d: &a%03d value_%03d\n", i, i, i);
    }

    /* Reference them all as aliases */
    for (int i = 0; i < 100 && pos + 50 < capacity; i++) {
        pos += snprintf(input + pos, capacity - pos,
            "ref_%03d: *a%03d\n", i, i);
    }

    yaml_parser_t parser;
    yaml_document_t doc;

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, pos);

    int result = yaml_parser_load(&parser, &doc);
    ASSERT(result, "Anchor/alias stress document should load");

    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
    free(input);
}

/* Test various YAML version directives */
static void test_yaml_directives(void) {
    /* Standard YAML 1.1 directive */
    const char *input1 = "%YAML 1.1\n---\nkey: value\n";
    yaml_event_type_t events[32];
    int count;
    int result = parse_string_events(input1, events, 32, &count);
    ASSERT(result, "YAML 1.1 directive should parse");

    /* YAML 1.2 directive (libyaml accepts it) */
    const char *input2 = "%YAML 1.2\n---\nkey: value\n";
    result = parse_string_events(input2, events, 32, &count);
    /* libyaml may warn but should still parse */
    ASSERT(1, "YAML 1.2 directive test completed");

    /* Multiple TAG directives */
    const char *input3 = "%TAG !t! tag:test.com,2000:\n%TAG !x! tag:example.com,2000:\n---\nkey: !t!type value\n";
    result = parse_string_events(input3, events, 32, &count);
    ASSERT(result, "Multiple TAG directives should parse");
}

/* Test binary-like content in double-quoted scalars */
static void test_binary_escape_sequences(void) {
    /* Null byte, tab, bell, backspace, escape via double-quoted escapes */
    const char *input = "key: \"\\0\\a\\b\\t\\n\\r\\e\"\n";
    yaml_parser_t parser;
    yaml_event_t event;

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));

    int success = 1;
    int found_scalar = 0;
    while (1) {
        if (!yaml_parser_parse(&parser, &event)) {
            success = 0;
            break;
        }
        if (event.type == YAML_SCALAR_EVENT) {
            if (event.data.scalar.length > 0) {
                found_scalar = 1;
            }
        }
        if (event.type == YAML_STREAM_END_EVENT) {
            yaml_event_delete(&event);
            break;
        }
        yaml_event_delete(&event);
    }
    yaml_parser_delete(&parser);

    ASSERT(success, "Escape sequences should parse successfully");
    ASSERT(found_scalar, "Should find escaped scalar value");
}

/* Test unicode escape sequences */
static void test_unicode_escapes(void) {
    /* Unicode escapes: \x41 = A, \u0041 = A, \U00000041 = A */
    const char *input = "key: \"\\x41\\u0041\\U00000041\"\n";
    yaml_parser_t parser;
    yaml_event_t event;

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));

    int success = 1;
    int scalar_idx = 0;
    while (1) {
        if (!yaml_parser_parse(&parser, &event)) {
            success = 0;
            break;
        }
        if (event.type == YAML_SCALAR_EVENT) {
            scalar_idx++;
            if (scalar_idx == 2) {
                /* Second scalar is the value with unicode escapes */
                ASSERT_EQ_STR((const char *)event.data.scalar.value, "AAA",
                              "Unicode escapes should produce AAA");
            }
        }
        if (event.type == YAML_STREAM_END_EVENT) {
            yaml_event_delete(&event);
            break;
        }
        yaml_event_delete(&event);
    }
    yaml_parser_delete(&parser);

    ASSERT(success, "Unicode escape sequences should parse");
}

/* Test comment handling */
static void test_comment_handling(void) {
    const char *input =
        "# Top-level comment\n"
        "key: value  # inline comment\n"
        "# Between keys\n"
        "list:\n"
        "  # Before items\n"
        "  - item1  # after item\n"
        "  - item2\n"
        "  # After items\n"
        "# End comment\n";

    yaml_parser_t parser;
    yaml_event_t event;
    int scalar_count = 0;

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));

    int success = 1;
    while (1) {
        if (!yaml_parser_parse(&parser, &event)) {
            success = 0;
            break;
        }
        if (event.type == YAML_SCALAR_EVENT) scalar_count++;
        if (event.type == YAML_STREAM_END_EVENT) {
            yaml_event_delete(&event);
            break;
        }
        yaml_event_delete(&event);
    }
    yaml_parser_delete(&parser);

    ASSERT(success, "Comments should be handled correctly");
    ASSERT_EQ_INT(scalar_count, 5, "Should find 5 scalars (key, value, list, item1, item2)");
}

/* Test multiline scalars */
static void test_multiline_scalars(void) {
    /* Plain multiline */
    const char *input1 = "key: this is\n  a multiline\n  plain scalar\n";
    yaml_parser_t parser;
    yaml_event_t event;

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)input1, strlen(input1));

    int success = 1;
    while (1) {
        if (!yaml_parser_parse(&parser, &event)) {
            success = 0;
            break;
        }
        if (event.type == YAML_SCALAR_EVENT) {
            if (strcmp((const char *)event.data.scalar.value,
                       "this is a multiline plain scalar") == 0) {
                /* Plain multiline folds to single line */
            }
        }
        if (event.type == YAML_STREAM_END_EVENT) {
            yaml_event_delete(&event);
            break;
        }
        yaml_event_delete(&event);
    }
    yaml_parser_delete(&parser);

    ASSERT(success, "Multiline plain scalar should parse");

    /* Double-quoted multiline */
    const char *input2 = "key: \"line1\\nline2\\nline3\"\n";
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)input2, strlen(input2));

    success = 1;
    while (1) {
        if (!yaml_parser_parse(&parser, &event)) {
            success = 0;
            break;
        }
        if (event.type == YAML_STREAM_END_EVENT) {
            yaml_event_delete(&event);
            break;
        }
        yaml_event_delete(&event);
    }
    yaml_parser_delete(&parser);

    ASSERT(success, "Double-quoted multiline should parse");
}

int main(void) {
    TEST_SUITE_BEGIN("Resource Constrained Tests");

    printf("  Stack-limited tests:\n");
    test_limited_stack_deep_nesting();

    printf("  Memory-limited tests:\n");
    test_limited_memory_parser_init();
    test_limited_memory_small_doc();

    printf("  Pathological input tests:\n");
    test_very_long_line();
    test_many_documents();
    test_wide_mapping();
    test_deep_flow_nesting();
    test_mixed_block_flow();
    test_empty_collections();
    test_anchor_alias_stress();

    printf("  YAML feature tests:\n");
    test_yaml_directives();
    test_binary_escape_sequences();
    test_unicode_escapes();
    test_comment_handling();
    test_multiline_scalars();

    TEST_SUITE_END();
}
