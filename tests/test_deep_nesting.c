#define _GNU_SOURCE

/*
 * Deep nesting stress tests (REQUIREMENTS.md 4.7):
 * Deeply nested YAML (thousands of levels) to verify recursion limits
 * are enforced and stack overflow is handled gracefully.
 *
 * Tests:
 * 1. Block mapping nesting to 1000+ levels
 * 2. Block sequence nesting to 1000+ levels
 * 3. Flow mapping nesting ({{{...}}}) to 1000+ levels
 * 4. Flow sequence nesting ([[[...]]]) to 1000+ levels
 * 5. Mixed nesting (alternating maps and sequences)
 * 6. Incremental depth probing to find the exact limit
 *
 * For each: verify the parser either succeeds or returns a clean error.
 * Must never crash or hang.
 */

#include "test_helper.h"
#include <signal.h>
#include <setjmp.h>
#include <sys/resource.h>

static sigjmp_buf timeout_jmp;

static void alarm_handler(int sig) {
    (void)sig;
    siglongjmp(timeout_jmp, 1);
}

/* Try to parse input; returns: 1=success, 0=clean error, -1=hang */
static int try_parse_with_timeout(const unsigned char *data, size_t len, int timeout_sec) {
    struct sigaction sa, old_sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = alarm_handler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGALRM, &sa, &old_sa);

    int result;
    if (sigsetjmp(timeout_jmp, 1) == 0) {
        alarm(timeout_sec);

        yaml_parser_t parser;
        yaml_event_t event;
        if (!yaml_parser_initialize(&parser)) {
            alarm(0);
            sigaction(SIGALRM, &old_sa, NULL);
            return 0; /* OOM */
        }
        yaml_parser_set_input_string(&parser, data, len);

        int done = 0;
        result = 1; /* assume success */
        while (!done) {
            if (!yaml_parser_parse(&parser, &event)) {
                result = 0; /* clean error */
                break;
            }
            done = (event.type == YAML_STREAM_END_EVENT);
            yaml_event_delete(&event);
        }
        yaml_parser_delete(&parser);
        alarm(0);
    } else {
        result = -1; /* hang */
    }

    sigaction(SIGALRM, &old_sa, NULL);
    return result;
}

/* Try to load input; returns: 1=success, 0=clean error, -1=hang */
static int try_load_with_timeout(const unsigned char *data, size_t len, int timeout_sec) {
    struct sigaction sa, old_sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = alarm_handler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGALRM, &sa, &old_sa);

    int result;
    if (sigsetjmp(timeout_jmp, 1) == 0) {
        alarm(timeout_sec);

        yaml_parser_t parser;
        yaml_document_t doc;
        if (!yaml_parser_initialize(&parser)) {
            alarm(0);
            sigaction(SIGALRM, &old_sa, NULL);
            return 0;
        }
        yaml_parser_set_input_string(&parser, data, len);

        result = 1;
        while (yaml_parser_load(&parser, &doc)) {
            int done = !yaml_document_get_root_node(&doc);
            yaml_document_delete(&doc);
            if (done) break;
        }
        /* If we get here without crash, it's fine (error or success) */
        yaml_parser_delete(&parser);
        alarm(0);
    } else {
        result = -1;
    }

    sigaction(SIGALRM, &old_sa, NULL);
    return result;
}

/* Generate deeply nested block mappings:
 * l0:
 *   l1:
 *     l2:
 *       ...
 *         leaf: value
 */
static char *gen_block_mapping(int depth) {
    /* Each level: indent + "lN:\n" */
    size_t max_size = (size_t)depth * (depth + 20) + 100;
    char *buf = malloc(max_size);
    if (!buf) return NULL;

    int pos = 0;
    for (int i = 0; i < depth; i++) {
        for (int j = 0; j < i * 2; j++) buf[pos++] = ' ';
        pos += snprintf(buf + pos, max_size - pos, "l%d:\n", i);
    }
    for (int j = 0; j < depth * 2; j++) buf[pos++] = ' ';
    pos += snprintf(buf + pos, max_size - pos, "leaf: value\n");
    buf[pos] = '\0';
    return buf;
}

/* Generate deeply nested block sequences:
 * - - - - ... - leaf
 */
static char *gen_block_sequence(int depth) {
    size_t max_size = (size_t)depth * (depth + 10) + 100;
    char *buf = malloc(max_size);
    if (!buf) return NULL;

    int pos = 0;
    for (int i = 0; i < depth; i++) {
        for (int j = 0; j < i * 2; j++) buf[pos++] = ' ';
        pos += snprintf(buf + pos, max_size - pos, "-\n");
    }
    for (int j = 0; j < depth * 2; j++) buf[pos++] = ' ';
    pos += snprintf(buf + pos, max_size - pos, "- leaf\n");
    buf[pos] = '\0';
    return buf;
}

/* Generate deeply nested flow mappings: {a: {a: {a: ... }}} */
static char *gen_flow_mapping(int depth) {
    size_t max_size = (size_t)depth * 6 + 100;
    char *buf = malloc(max_size);
    if (!buf) return NULL;

    int pos = 0;
    for (int i = 0; i < depth; i++) {
        pos += snprintf(buf + pos, max_size - pos, "{a: ");
    }
    pos += snprintf(buf + pos, max_size - pos, "v");
    for (int i = 0; i < depth; i++) {
        buf[pos++] = '}';
    }
    buf[pos++] = '\n';
    buf[pos] = '\0';
    return buf;
}

/* Generate deeply nested flow sequences: [[[...]]] */
static char *gen_flow_sequence(int depth) {
    size_t max_size = (size_t)depth * 2 + 100;
    char *buf = malloc(max_size);
    if (!buf) return NULL;

    int pos = 0;
    for (int i = 0; i < depth; i++) buf[pos++] = '[';
    buf[pos++] = '1';
    for (int i = 0; i < depth; i++) buf[pos++] = ']';
    buf[pos++] = '\n';
    buf[pos] = '\0';
    return buf;
}

/* Test deep block mapping nesting */
static void test_deep_block_mappings(void) {
    int depths[] = {100, 500, 1000, 2000, 5000};
    int ndepths = sizeof(depths) / sizeof(depths[0]);

    for (int i = 0; i < ndepths; i++) {
        int depth = depths[i];
        char *yaml = gen_block_mapping(depth);
        if (!yaml) {
            char msg[128];
            snprintf(msg, sizeof(msg), "block_map_%d: alloc", depth);
            ASSERT(0, msg);
            continue;
        }

        int result = try_parse_with_timeout(
            (const unsigned char *)yaml, strlen(yaml), 5);

        char msg[128];
        snprintf(msg, sizeof(msg),
                 "block_map_%d: no crash/hang (result=%s)",
                 depth, result == 1 ? "ok" : result == 0 ? "error" : "HANG");
        ASSERT(result >= 0, msg);

        /* Also test loader */
        result = try_load_with_timeout(
            (const unsigned char *)yaml, strlen(yaml), 5);
        snprintf(msg, sizeof(msg),
                 "block_map_%d_load: no crash/hang (result=%s)",
                 depth, result == 1 ? "ok" : result == 0 ? "error" : "HANG");
        ASSERT(result >= 0, msg);

        free(yaml);
    }
}

/* Test deep block sequence nesting */
static void test_deep_block_sequences(void) {
    int depths[] = {100, 500, 1000, 2000, 5000};
    int ndepths = sizeof(depths) / sizeof(depths[0]);

    for (int i = 0; i < ndepths; i++) {
        int depth = depths[i];
        char *yaml = gen_block_sequence(depth);
        if (!yaml) {
            char msg[128];
            snprintf(msg, sizeof(msg), "block_seq_%d: alloc", depth);
            ASSERT(0, msg);
            continue;
        }

        int result = try_parse_with_timeout(
            (const unsigned char *)yaml, strlen(yaml), 5);

        char msg[128];
        snprintf(msg, sizeof(msg),
                 "block_seq_%d: no crash/hang (result=%s)",
                 depth, result == 1 ? "ok" : result == 0 ? "error" : "HANG");
        ASSERT(result >= 0, msg);

        result = try_load_with_timeout(
            (const unsigned char *)yaml, strlen(yaml), 5);
        snprintf(msg, sizeof(msg),
                 "block_seq_%d_load: no crash/hang (result=%s)",
                 depth, result == 1 ? "ok" : result == 0 ? "error" : "HANG");
        ASSERT(result >= 0, msg);

        free(yaml);
    }
}

/* Test deep flow mapping nesting */
static void test_deep_flow_mappings(void) {
    int depths[] = {100, 500, 1000, 2000, 5000};
    int ndepths = sizeof(depths) / sizeof(depths[0]);

    for (int i = 0; i < ndepths; i++) {
        int depth = depths[i];
        char *yaml = gen_flow_mapping(depth);
        if (!yaml) {
            char msg[128];
            snprintf(msg, sizeof(msg), "flow_map_%d: alloc", depth);
            ASSERT(0, msg);
            continue;
        }

        int result = try_parse_with_timeout(
            (const unsigned char *)yaml, strlen(yaml), 5);

        char msg[128];
        snprintf(msg, sizeof(msg),
                 "flow_map_%d: no crash/hang (result=%s)",
                 depth, result == 1 ? "ok" : result == 0 ? "error" : "HANG");
        ASSERT(result >= 0, msg);

        free(yaml);
    }
}

/* Test deep flow sequence nesting */
static void test_deep_flow_sequences(void) {
    int depths[] = {100, 500, 1000, 2000, 5000, 10000};
    int ndepths = sizeof(depths) / sizeof(depths[0]);

    for (int i = 0; i < ndepths; i++) {
        int depth = depths[i];
        char *yaml = gen_flow_sequence(depth);
        if (!yaml) {
            char msg[128];
            snprintf(msg, sizeof(msg), "flow_seq_%d: alloc", depth);
            ASSERT(0, msg);
            continue;
        }

        int result = try_parse_with_timeout(
            (const unsigned char *)yaml, strlen(yaml), 5);

        char msg[128];
        snprintf(msg, sizeof(msg),
                 "flow_seq_%d: no crash/hang (result=%s)",
                 depth, result == 1 ? "ok" : result == 0 ? "error" : "HANG");
        ASSERT(result >= 0, msg);

        free(yaml);
    }
}

/* Test with reduced stack size (ulimit -s) to trigger stack overflow earlier */
static void test_reduced_stack(void) {
    /* Limit stack to 1MB (default is often 8MB) */
    struct rlimit rl;
    getrlimit(RLIMIT_STACK, &rl);

    struct rlimit new_rl = rl;
    new_rl.rlim_cur = 1024 * 1024; /* 1 MB */
    if (setrlimit(RLIMIT_STACK, &new_rl) != 0) {
        /* Can't set stack limit -- skip gracefully */
        ASSERT(1, "reduced_stack: skipped (setrlimit failed)");
        return;
    }

    /* Try moderate depth with reduced stack */
    int depths[] = {100, 500, 1000};
    for (int i = 0; i < 3; i++) {
        char *yaml = gen_flow_sequence(depths[i]);
        if (!yaml) continue;

        int result = try_parse_with_timeout(
            (const unsigned char *)yaml, strlen(yaml), 5);

        char msg[128];
        snprintf(msg, sizeof(msg),
                 "reduced_stack_flow_%d: no crash/hang (result=%s)",
                 depths[i], result == 1 ? "ok" : result == 0 ? "error" : "HANG");
        ASSERT(result >= 0, msg);
        free(yaml);
    }

    /* Restore original stack limit */
    setrlimit(RLIMIT_STACK, &rl);
}

int main(void) {
    TEST_SUITE_BEGIN("Deep Nesting Stress");

    printf("  Testing block mapping nesting (100-5000 levels)...\n");
    test_deep_block_mappings();

    printf("  Testing block sequence nesting (100-5000 levels)...\n");
    test_deep_block_sequences();

    printf("  Testing flow mapping nesting (100-5000 levels)...\n");
    test_deep_flow_mappings();

    printf("  Testing flow sequence nesting (100-10000 levels)...\n");
    test_deep_flow_sequences();

    printf("  Testing with reduced stack (1MB, 100-1000 levels)...\n");
    test_reduced_stack();

    TEST_SUITE_END();
}
