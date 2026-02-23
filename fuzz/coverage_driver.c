/*
 * Coverage driver: runs all files from a directory through all 3 API levels
 * (scanner, parser, loader) to measure code coverage.
 *
 * Build with coverage flags:
 *   clang -fprofile-instr-generate -fcoverage-mapping -g -I../include \
 *         coverage_driver.c -L../build-cov -lqyaml -o coverage_driver
 *
 * Run:
 *   LLVM_PROFILE_FILE=cov.profraw ./coverage_driver <corpus_dir>
 *   llvm-profdata merge -sparse cov.profraw -o cov.profdata
 *   llvm-cov report ../build-cov/libqyaml.a -instr-profile=cov.profdata
 */

#include <yaml.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

static void run_scan(const unsigned char *data, size_t size) {
    yaml_parser_t p;
    if (!yaml_parser_initialize(&p)) return;
    yaml_parser_set_input_string(&p, data, size);
    yaml_token_t tok;
    while (1) {
        if (!yaml_parser_scan(&p, &tok)) break;
        int done = (tok.type == YAML_STREAM_END_TOKEN);
        yaml_token_delete(&tok);
        if (done) break;
    }
    yaml_parser_delete(&p);
}

static void run_parse(const unsigned char *data, size_t size) {
    yaml_parser_t p;
    if (!yaml_parser_initialize(&p)) return;
    yaml_parser_set_input_string(&p, data, size);
    yaml_event_t evt;
    while (1) {
        if (!yaml_parser_parse(&p, &evt)) break;
        int done = (evt.type == YAML_STREAM_END_EVENT);
        yaml_event_delete(&evt);
        if (done) break;
    }
    yaml_parser_delete(&p);
}

static void run_load(const unsigned char *data, size_t size) {
    yaml_parser_t p;
    if (!yaml_parser_initialize(&p)) return;
    yaml_parser_set_input_string(&p, data, size);
    yaml_document_t doc;
    while (1) {
        if (!yaml_parser_load(&p, &doc)) break;
        yaml_node_t *root = yaml_document_get_root_node(&doc);
        if (!root) { yaml_document_delete(&doc); break; }
        yaml_document_delete(&doc);
    }
    yaml_parser_delete(&p);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <corpus_dir> [corpus_dir2 ...]\n", argv[0]);
        return 1;
    }

    int total = 0, errors = 0;

    for (int a = 1; a < argc; a++) {
        DIR *d = opendir(argv[a]);
        if (!d) { perror(argv[a]); continue; }
        struct dirent *e;
        while ((e = readdir(d)) != NULL) {
            if (e->d_name[0] == '.') continue;
            char path[4096];
            snprintf(path, sizeof(path), "%s/%s", argv[a], e->d_name);
            FILE *f = fopen(path, "rb");
            if (!f) continue;
            fseek(f, 0, SEEK_END);
            size_t sz = ftell(f);
            fseek(f, 0, SEEK_SET);
            unsigned char *buf = malloc(sz);
            if (!buf) { fclose(f); continue; }
            fread(buf, 1, sz, f);
            fclose(f);

            run_scan(buf, sz);
            run_parse(buf, sz);
            run_load(buf, sz);
            total++;
            free(buf);
        }
        closedir(d);
    }

    fprintf(stderr, "Processed %d files\n", total);
    return 0;
}
