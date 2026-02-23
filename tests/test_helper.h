#ifndef TEST_HELPER_H
#define TEST_HELPER_H

#include <yaml.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

#define ASSERT_EQ_INT(a, b, msg) do { \
    int _a = (a), _b = (b); \
    tests_run++; \
    if (_a != _b) { \
        fprintf(stderr, "  FAIL [%s:%d]: %s (expected %d, got %d)\n", \
                __FILE__, __LINE__, msg, _b, _a); \
        tests_failed++; \
    } else { \
        tests_passed++; \
    } \
} while(0)

#define ASSERT_EQ_STR(a, b, msg) do { \
    const char *_a = (const char *)(a), *_b = (const char *)(b); \
    tests_run++; \
    if (_a == NULL && _b == NULL) { \
        tests_passed++; \
    } else if (_a == NULL || _b == NULL || strcmp(_a, _b) != 0) { \
        fprintf(stderr, "  FAIL [%s:%d]: %s (expected \"%s\", got \"%s\")\n", \
                __FILE__, __LINE__, msg, _b ? _b : "(null)", _a ? _a : "(null)"); \
        tests_failed++; \
    } else { \
        tests_passed++; \
    } \
} while(0)

#define ASSERT_NOT_NULL(ptr, msg) do { \
    tests_run++; \
    if ((ptr) == NULL) { \
        fprintf(stderr, "  FAIL [%s:%d]: %s (expected non-null)\n", \
                __FILE__, __LINE__, msg); \
        tests_failed++; \
    } else { \
        tests_passed++; \
    } \
} while(0)

#define ASSERT_NULL(ptr, msg) do { \
    tests_run++; \
    if ((ptr) != NULL) { \
        fprintf(stderr, "  FAIL [%s:%d]: %s (expected null)\n", \
                __FILE__, __LINE__, msg); \
        tests_failed++; \
    } else { \
        tests_passed++; \
    } \
} while(0)

#define TEST_SUITE_BEGIN(name) \
    printf("Running test suite: %s\n", name);

#define TEST_SUITE_END() do { \
    printf("Results: %d/%d passed", tests_passed, tests_run); \
    if (tests_failed > 0) printf(", %d FAILED", tests_failed); \
    printf("\n"); \
    return tests_failed > 0 ? 1 : 0; \
} while(0)

/* Helper: parse a YAML string and collect all events */
__attribute__((unused))
static int parse_string_events(const char *input, yaml_event_type_t *event_types,
                                int max_events, int *event_count) {
    yaml_parser_t parser;
    yaml_event_t event;
    int count = 0;

    if (!yaml_parser_initialize(&parser)) return 0;
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));

    while (count < max_events) {
        if (!yaml_parser_parse(&parser, &event)) {
            yaml_parser_delete(&parser);
            *event_count = count;
            return 0;
        }
        event_types[count] = event.type;
        count++;
        if (event.type == YAML_STREAM_END_EVENT) {
            yaml_event_delete(&event);
            break;
        }
        yaml_event_delete(&event);
    }

    yaml_parser_delete(&parser);
    *event_count = count;
    return 1;
}

/* Helper: parse a YAML string and collect all tokens */
__attribute__((unused))
static int scan_string_tokens(const char *input, yaml_token_type_t *token_types,
                               int max_tokens, int *token_count) {
    yaml_parser_t parser;
    yaml_token_t token;
    int count = 0;

    if (!yaml_parser_initialize(&parser)) return 0;
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));

    while (count < max_tokens) {
        if (!yaml_parser_scan(&parser, &token)) {
            yaml_parser_delete(&parser);
            *token_count = count;
            return 0;
        }
        token_types[count] = token.type;
        count++;
        if (token.type == YAML_STREAM_END_TOKEN) {
            yaml_token_delete(&token);
            break;
        }
        yaml_token_delete(&token);
    }

    yaml_parser_delete(&parser);
    *token_count = count;
    return 1;
}

#endif /* TEST_HELPER_H */
