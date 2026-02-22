#include "test_helper.h"

/*
 * Differential tests: parse YAML inputs with our library and verify
 * the events match expected output. These tests use known YAML inputs
 * and verify we produce the correct sequence of events.
 */

/* Helper: parse yaml and verify exact event sequence */
static void verify_events(const char *name, const char *yaml,
                           const yaml_event_type_t *expected, int expected_count) {
    yaml_event_type_t events[128];
    int count;
    int ok = parse_string_events(yaml, events, 128, &count);
    ASSERT(ok, name);
    ASSERT_EQ_INT(count, expected_count, name);
    for (int i = 0; i < count && i < expected_count; i++) {
        char msg[256];
        snprintf(msg, sizeof(msg), "%s: event[%d]", name, i);
        ASSERT_EQ_INT(events[i], expected[i], msg);
    }
}

/* Test: empty stream */
static void test_empty(void) {
    yaml_event_type_t expected[] = {
        YAML_STREAM_START_EVENT,
        YAML_STREAM_END_EVENT
    };
    verify_events("empty", "", expected, 2);
}

/* Test: single scalar document */
static void test_single_scalar(void) {
    yaml_event_type_t expected[] = {
        YAML_STREAM_START_EVENT,
        YAML_DOCUMENT_START_EVENT,
        YAML_SCALAR_EVENT,
        YAML_DOCUMENT_END_EVENT,
        YAML_STREAM_END_EVENT
    };
    verify_events("single_scalar", "hello", expected, 5);
}

/* Test: simple mapping */
static void test_simple_mapping(void) {
    yaml_event_type_t expected[] = {
        YAML_STREAM_START_EVENT,
        YAML_DOCUMENT_START_EVENT,
        YAML_MAPPING_START_EVENT,
        YAML_SCALAR_EVENT,  /* key */
        YAML_SCALAR_EVENT,  /* value */
        YAML_MAPPING_END_EVENT,
        YAML_DOCUMENT_END_EVENT,
        YAML_STREAM_END_EVENT
    };
    verify_events("simple_mapping", "key: value\n", expected, 8);
}

/* Test: simple sequence */
static void test_simple_sequence(void) {
    yaml_event_type_t expected[] = {
        YAML_STREAM_START_EVENT,
        YAML_DOCUMENT_START_EVENT,
        YAML_SEQUENCE_START_EVENT,
        YAML_SCALAR_EVENT,
        YAML_SCALAR_EVENT,
        YAML_SEQUENCE_END_EVENT,
        YAML_DOCUMENT_END_EVENT,
        YAML_STREAM_END_EVENT
    };
    verify_events("simple_sequence", "- a\n- b\n", expected, 8);
}

/* Test: flow sequence */
static void test_flow_sequence(void) {
    yaml_event_type_t expected[] = {
        YAML_STREAM_START_EVENT,
        YAML_DOCUMENT_START_EVENT,
        YAML_SEQUENCE_START_EVENT,
        YAML_SCALAR_EVENT,
        YAML_SCALAR_EVENT,
        YAML_SCALAR_EVENT,
        YAML_SEQUENCE_END_EVENT,
        YAML_DOCUMENT_END_EVENT,
        YAML_STREAM_END_EVENT
    };
    verify_events("flow_sequence", "[a, b, c]", expected, 9);
}

/* Test: flow mapping */
static void test_flow_mapping(void) {
    yaml_event_type_t expected[] = {
        YAML_STREAM_START_EVENT,
        YAML_DOCUMENT_START_EVENT,
        YAML_MAPPING_START_EVENT,
        YAML_SCALAR_EVENT,
        YAML_SCALAR_EVENT,
        YAML_MAPPING_END_EVENT,
        YAML_DOCUMENT_END_EVENT,
        YAML_STREAM_END_EVENT
    };
    verify_events("flow_mapping", "{a: b}", expected, 8);
}

/* Test: mapping with multiple keys */
static void test_multi_key_mapping(void) {
    yaml_event_type_t expected[] = {
        YAML_STREAM_START_EVENT,
        YAML_DOCUMENT_START_EVENT,
        YAML_MAPPING_START_EVENT,
        YAML_SCALAR_EVENT,  /* key1 */
        YAML_SCALAR_EVENT,  /* val1 */
        YAML_SCALAR_EVENT,  /* key2 */
        YAML_SCALAR_EVENT,  /* val2 */
        YAML_SCALAR_EVENT,  /* key3 */
        YAML_SCALAR_EVENT,  /* val3 */
        YAML_MAPPING_END_EVENT,
        YAML_DOCUMENT_END_EVENT,
        YAML_STREAM_END_EVENT
    };
    verify_events("multi_key", "key1: val1\nkey2: val2\nkey3: val3\n", expected, 12);
}

/* Test: nested mapping */
static void test_nested_mapping(void) {
    yaml_event_type_t expected[] = {
        YAML_STREAM_START_EVENT,
        YAML_DOCUMENT_START_EVENT,
        YAML_MAPPING_START_EVENT,
        YAML_SCALAR_EVENT,           /* outer */
        YAML_MAPPING_START_EVENT,
        YAML_SCALAR_EVENT,           /* inner */
        YAML_SCALAR_EVENT,           /* value */
        YAML_MAPPING_END_EVENT,
        YAML_MAPPING_END_EVENT,
        YAML_DOCUMENT_END_EVENT,
        YAML_STREAM_END_EVENT
    };
    verify_events("nested_mapping", "outer:\n  inner: value\n", expected, 11);
}

/* Test: sequence in mapping */
static void test_seq_in_mapping(void) {
    yaml_event_type_t expected[] = {
        YAML_STREAM_START_EVENT,
        YAML_DOCUMENT_START_EVENT,
        YAML_MAPPING_START_EVENT,
        YAML_SCALAR_EVENT,           /* items */
        YAML_SEQUENCE_START_EVENT,
        YAML_SCALAR_EVENT,           /* a */
        YAML_SCALAR_EVENT,           /* b */
        YAML_SEQUENCE_END_EVENT,
        YAML_MAPPING_END_EVENT,
        YAML_DOCUMENT_END_EVENT,
        YAML_STREAM_END_EVENT
    };
    verify_events("seq_in_map", "items:\n  - a\n  - b\n", expected, 11);
}

/* Test: mapping in sequence */
static void test_map_in_sequence(void) {
    yaml_event_type_t expected[] = {
        YAML_STREAM_START_EVENT,
        YAML_DOCUMENT_START_EVENT,
        YAML_SEQUENCE_START_EVENT,
        YAML_MAPPING_START_EVENT,
        YAML_SCALAR_EVENT,
        YAML_SCALAR_EVENT,
        YAML_MAPPING_END_EVENT,
        YAML_SEQUENCE_END_EVENT,
        YAML_DOCUMENT_END_EVENT,
        YAML_STREAM_END_EVENT
    };
    verify_events("map_in_seq", "- key: val\n", expected, 10);
}

/* Test: explicit document */
static void test_explicit_doc(void) {
    yaml_event_type_t expected[] = {
        YAML_STREAM_START_EVENT,
        YAML_DOCUMENT_START_EVENT,
        YAML_SCALAR_EVENT,
        YAML_DOCUMENT_END_EVENT,
        YAML_STREAM_END_EVENT
    };
    verify_events("explicit_doc", "---\nhello\n...\n", expected, 5);
}

/* Test: multiple explicit documents */
static void test_multi_doc(void) {
    yaml_event_type_t expected[] = {
        YAML_STREAM_START_EVENT,
        YAML_DOCUMENT_START_EVENT,
        YAML_SCALAR_EVENT,
        YAML_DOCUMENT_END_EVENT,
        YAML_DOCUMENT_START_EVENT,
        YAML_SCALAR_EVENT,
        YAML_DOCUMENT_END_EVENT,
        YAML_STREAM_END_EVENT
    };
    verify_events("multi_doc", "---\na\n---\nb\n...\n", expected, 8);
}

/* Test: anchor and alias */
static void test_anchor_alias(void) {
    yaml_event_type_t expected[] = {
        YAML_STREAM_START_EVENT,
        YAML_DOCUMENT_START_EVENT,
        YAML_SEQUENCE_START_EVENT,
        YAML_SCALAR_EVENT,       /* &a hello */
        YAML_ALIAS_EVENT,        /* *a */
        YAML_SEQUENCE_END_EVENT,
        YAML_DOCUMENT_END_EVENT,
        YAML_STREAM_END_EVENT
    };
    verify_events("anchor_alias", "- &a hello\n- *a\n", expected, 8);
}

/* Test: empty mapping value */
static void test_empty_value(void) {
    yaml_event_type_t expected[] = {
        YAML_STREAM_START_EVENT,
        YAML_DOCUMENT_START_EVENT,
        YAML_MAPPING_START_EVENT,
        YAML_SCALAR_EVENT,       /* key */
        YAML_SCALAR_EVENT,       /* empty value */
        YAML_MAPPING_END_EVENT,
        YAML_DOCUMENT_END_EVENT,
        YAML_STREAM_END_EVENT
    };
    verify_events("empty_value", "key:\n", expected, 8);
}

/* Test: nested flow collections */
static void test_nested_flow(void) {
    yaml_event_type_t expected[] = {
        YAML_STREAM_START_EVENT,
        YAML_DOCUMENT_START_EVENT,
        YAML_SEQUENCE_START_EVENT,
        YAML_SEQUENCE_START_EVENT,
        YAML_SCALAR_EVENT,
        YAML_SEQUENCE_END_EVENT,
        YAML_SEQUENCE_START_EVENT,
        YAML_SCALAR_EVENT,
        YAML_SEQUENCE_END_EVENT,
        YAML_SEQUENCE_END_EVENT,
        YAML_DOCUMENT_END_EVENT,
        YAML_STREAM_END_EVENT
    };
    verify_events("nested_flow", "[[1], [2]]", expected, 12);
}

/* Test: tagged scalar */
static void test_tagged_scalar(void) {
    yaml_event_type_t expected[] = {
        YAML_STREAM_START_EVENT,
        YAML_DOCUMENT_START_EVENT,
        YAML_SCALAR_EVENT,       /* !!str hello */
        YAML_DOCUMENT_END_EVENT,
        YAML_STREAM_END_EVENT
    };
    verify_events("tagged_scalar", "!!str hello", expected, 5);
}

/* Test: complex yaml structure (Kubernetes-like) */
static void test_complex_structure(void) {
    const char *yaml =
        "apiVersion: v1\n"
        "kind: Service\n"
        "metadata:\n"
        "  name: my-service\n"
        "  labels:\n"
        "    app: my-app\n"
        "spec:\n"
        "  selector:\n"
        "    app: my-app\n"
        "  ports:\n"
        "    - port: 80\n"
        "      targetPort: 8080\n";

    yaml_event_type_t events[128];
    int count;
    ASSERT(parse_string_events(yaml, events, 128, &count), "complex: parse");

    /* Count events by type */
    int scalars = 0, map_start = 0, map_end = 0, seq_start = 0, seq_end = 0;
    for (int i = 0; i < count; i++) {
        switch (events[i]) {
            case YAML_SCALAR_EVENT: scalars++; break;
            case YAML_MAPPING_START_EVENT: map_start++; break;
            case YAML_MAPPING_END_EVENT: map_end++; break;
            case YAML_SEQUENCE_START_EVENT: seq_start++; break;
            case YAML_SEQUENCE_END_EVENT: seq_end++; break;
            default: break;
        }
    }

    /* apiVersion, v1, kind, Service, metadata, name, my-service, labels, app, my-app,
       spec, selector, app, my-app, ports, port, 80, targetPort, 8080 = 19 scalars */
    ASSERT_EQ_INT(scalars, 19, "complex: 19 scalars");
    ASSERT_EQ_INT(map_start, map_end, "complex: balanced maps");
    ASSERT_EQ_INT(seq_start, seq_end, "complex: balanced seqs");
    ASSERT(map_start >= 4, "complex: 4+ mapping levels");
    ASSERT_EQ_INT(seq_start, 1, "complex: 1 sequence");
}

/* Test: docker-compose-like structure */
static void test_docker_compose(void) {
    const char *yaml =
        "version: '3'\n"
        "services:\n"
        "  web:\n"
        "    image: nginx\n"
        "    ports:\n"
        "      - '80:80'\n"
        "      - '443:443'\n"
        "    volumes:\n"
        "      - ./html:/usr/share/nginx/html\n"
        "  db:\n"
        "    image: postgres\n"
        "    environment:\n"
        "      POSTGRES_DB: mydb\n"
        "      POSTGRES_USER: user\n";

    yaml_event_type_t events[128];
    int count;
    ASSERT(parse_string_events(yaml, events, 128, &count), "compose: parse");
    ASSERT(count > 20, "compose: many events");
}

/* Test: github actions-like structure */
static void test_github_actions(void) {
    const char *yaml =
        "name: CI\n"
        "on:\n"
        "  push:\n"
        "    branches: [main]\n"
        "  pull_request:\n"
        "    branches: [main]\n"
        "jobs:\n"
        "  build:\n"
        "    runs-on: ubuntu-latest\n"
        "    steps:\n"
        "      - uses: actions/checkout@v2\n"
        "      - name: Build\n"
        "        run: make\n";

    yaml_event_type_t events[128];
    int count;
    ASSERT(parse_string_events(yaml, events, 128, &count), "actions: parse");
    ASSERT(count > 20, "actions: many events");
}

int main(void) {
    TEST_SUITE_BEGIN("Differential");

    test_empty();
    test_single_scalar();
    test_simple_mapping();
    test_simple_sequence();
    test_flow_sequence();
    test_flow_mapping();
    test_multi_key_mapping();
    test_nested_mapping();
    test_seq_in_mapping();
    test_map_in_sequence();
    test_explicit_doc();
    test_multi_doc();
    test_anchor_alias();
    test_empty_value();
    test_nested_flow();
    test_tagged_scalar();
    test_complex_structure();
    test_docker_compose();
    test_github_actions();

    TEST_SUITE_END();
}
