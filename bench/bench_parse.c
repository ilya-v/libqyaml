#include <yaml.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double get_time(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

/* Generate a typical Kubernetes-like YAML document */
static char *generate_k8s_yaml(int services, size_t *out_size) {
    size_t capacity = 1024 * services;
    char *buf = malloc(capacity);
    size_t pos = 0;

    pos += snprintf(buf + pos, capacity - pos, "apiVersion: v1\nkind: ServiceList\nitems:\n");

    for (int i = 0; i < services; i++) {
        pos += snprintf(buf + pos, capacity - pos,
            "  - apiVersion: v1\n"
            "    kind: Service\n"
            "    metadata:\n"
            "      name: service-%d\n"
            "      namespace: default\n"
            "      labels:\n"
            "        app: myapp-%d\n"
            "        version: v1.0.%d\n"
            "        tier: backend\n"
            "      annotations:\n"
            "        description: \"Service number %d\"\n"
            "    spec:\n"
            "      type: ClusterIP\n"
            "      selector:\n"
            "        app: myapp-%d\n"
            "      ports:\n"
            "        - name: http\n"
            "          port: %d\n"
            "          targetPort: 8080\n"
            "          protocol: TCP\n"
            "        - name: https\n"
            "          port: %d\n"
            "          targetPort: 8443\n"
            "          protocol: TCP\n",
            i, i, i, i, i, 8000 + i, 8100 + i);
    }

    *out_size = pos;
    return buf;
}

/* Generate a simple mapping-heavy document */
static char *generate_mapping_yaml(int keys, size_t *out_size) {
    size_t capacity = 64 * keys;
    char *buf = malloc(capacity);
    size_t pos = 0;

    for (int i = 0; i < keys; i++) {
        pos += snprintf(buf + pos, capacity - pos,
            "key_%06d: value_%06d\n", i, i);
    }

    *out_size = pos;
    return buf;
}

/* Generate a flow sequence document */
static char *generate_flow_yaml(int items, size_t *out_size) {
    size_t capacity = 16 * items + 4;
    char *buf = malloc(capacity);
    size_t pos = 0;

    buf[pos++] = '[';
    for (int i = 0; i < items; i++) {
        if (i > 0) { buf[pos++] = ','; buf[pos++] = ' '; }
        pos += snprintf(buf + pos, capacity - pos, "%d", i);
    }
    buf[pos++] = ']';
    buf[pos++] = '\n';
    buf[pos] = '\0';

    *out_size = pos;
    return buf;
}

typedef void (*bench_fn)(const unsigned char *data, size_t size);

static void bench_scan(const unsigned char *data, size_t size) {
    yaml_parser_t parser;
    yaml_token_t token;

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, data, size);

    while (1) {
        yaml_parser_scan(&parser, &token);
        if (token.type == YAML_STREAM_END_TOKEN) {
            yaml_token_delete(&token);
            break;
        }
        yaml_token_delete(&token);
    }
    yaml_parser_delete(&parser);
}

static void bench_parse(const unsigned char *data, size_t size) {
    yaml_parser_t parser;
    yaml_event_t event;

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, data, size);

    while (1) {
        yaml_parser_parse(&parser, &event);
        if (event.type == YAML_STREAM_END_EVENT) {
            yaml_event_delete(&event);
            break;
        }
        yaml_event_delete(&event);
    }
    yaml_parser_delete(&parser);
}

static void bench_load(const unsigned char *data, size_t size) {
    yaml_parser_t parser;
    yaml_document_t doc;

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, data, size);

    while (1) {
        yaml_parser_load(&parser, &doc);
        yaml_node_t *root = yaml_document_get_root_node(&doc);
        yaml_document_delete(&doc);
        if (!root) break;
    }
    yaml_parser_delete(&parser);
}

static void run_benchmark(const char *name, const unsigned char *data, size_t size,
                           bench_fn fn, int iterations) {
    /* Warmup */
    for (int i = 0; i < 3; i++) {
        fn(data, size);
    }

    double start = get_time();
    for (int i = 0; i < iterations; i++) {
        fn(data, size);
    }
    double elapsed = get_time() - start;

    double throughput_mb = (double)size * iterations / elapsed / (1024.0 * 1024.0);
    double latency_us = elapsed * 1e6 / iterations;

    printf("  %-30s %6.1f MB/s  %8.1f us/iter  (%d iters, %.2f KB input)\n",
           name, throughput_mb, latency_us, iterations, size / 1024.0);
}

int main(void) {
    printf("libqyaml Benchmark\n");
    printf("==================\n\n");

    /* K8s-like YAML */
    {
        size_t size;
        char *yaml = generate_k8s_yaml(100, &size);
        printf("Kubernetes-like YAML (%.1f KB):\n", size / 1024.0);
        run_benchmark("scan", (const unsigned char *)yaml, size, bench_scan, 1000);
        run_benchmark("parse (events)", (const unsigned char *)yaml, size, bench_parse, 1000);
        run_benchmark("load (document)", (const unsigned char *)yaml, size, bench_load, 1000);
        printf("\n");
        free(yaml);
    }

    /* Mapping-heavy YAML */
    {
        size_t size;
        char *yaml = generate_mapping_yaml(10000, &size);
        printf("Mapping-heavy YAML (%.1f KB):\n", size / 1024.0);
        run_benchmark("scan", (const unsigned char *)yaml, size, bench_scan, 100);
        run_benchmark("parse (events)", (const unsigned char *)yaml, size, bench_parse, 100);
        run_benchmark("load (document)", (const unsigned char *)yaml, size, bench_load, 100);
        printf("\n");
        free(yaml);
    }

    /* Flow collection */
    {
        size_t size;
        char *yaml = generate_flow_yaml(10000, &size);
        printf("Flow sequence (%.1f KB):\n", size / 1024.0);
        run_benchmark("scan", (const unsigned char *)yaml, size, bench_scan, 100);
        run_benchmark("parse (events)", (const unsigned char *)yaml, size, bench_parse, 100);
        run_benchmark("load (document)", (const unsigned char *)yaml, size, bench_load, 100);
        printf("\n");
        free(yaml);
    }

    /* Small document repeated */
    {
        const char *yaml = "key: value\nlist:\n  - a\n  - b\n  - c\n";
        size_t size = strlen(yaml);
        printf("Small document (%zu bytes):\n", size);
        run_benchmark("scan", (const unsigned char *)yaml, size, bench_scan, 100000);
        run_benchmark("parse (events)", (const unsigned char *)yaml, size, bench_parse, 100000);
        run_benchmark("load (document)", (const unsigned char *)yaml, size, bench_load, 100000);
        printf("\n");
    }

    return 0;
}
