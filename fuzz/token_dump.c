/*
 * Token dump comparison tool: side-by-side token streams from libqyaml
 * and reference libyaml for a given YAML input.
 *
 * Build:
 *   clang -O1 -g -I../include token_dump.c -L../build -lqyaml -ldl -o token_dump
 * Run:
 *   ./token_dump <input_file>
 *   ./token_dump -              # read from stdin
 */

#include <yaml.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Reference libyaml function pointers (loaded via dlopen) */
typedef int (*fn_parser_initialize)(yaml_parser_t *);
typedef void (*fn_parser_delete)(yaml_parser_t *);
typedef void (*fn_parser_set_input_string)(yaml_parser_t *,
    const unsigned char *, size_t);
typedef int (*fn_parser_scan)(yaml_parser_t *, yaml_token_t *);
typedef void (*fn_token_delete)(yaml_token_t *);

static fn_parser_initialize       ref_init;
static fn_parser_delete           ref_delete;
static fn_parser_set_input_string ref_set_input;
static fn_parser_scan             ref_scan;
static fn_token_delete            ref_tok_delete;

static const char *token_names[] = {
    [YAML_NO_TOKEN]                  = "NO_TOKEN",
    [YAML_STREAM_START_TOKEN]        = "STREAM_START",
    [YAML_STREAM_END_TOKEN]          = "STREAM_END",
    [YAML_VERSION_DIRECTIVE_TOKEN]   = "VERSION_DIRECTIVE",
    [YAML_TAG_DIRECTIVE_TOKEN]       = "TAG_DIRECTIVE",
    [YAML_DOCUMENT_START_TOKEN]      = "DOCUMENT_START",
    [YAML_DOCUMENT_END_TOKEN]        = "DOCUMENT_END",
    [YAML_BLOCK_SEQUENCE_START_TOKEN]= "BLOCK_SEQ_START",
    [YAML_BLOCK_MAPPING_START_TOKEN] = "BLOCK_MAP_START",
    [YAML_BLOCK_END_TOKEN]           = "BLOCK_END",
    [YAML_FLOW_SEQUENCE_START_TOKEN] = "FLOW_SEQ_START",
    [YAML_FLOW_SEQUENCE_END_TOKEN]   = "FLOW_SEQ_END",
    [YAML_FLOW_MAPPING_START_TOKEN]  = "FLOW_MAP_START",
    [YAML_FLOW_MAPPING_END_TOKEN]    = "FLOW_MAP_END",
    [YAML_BLOCK_ENTRY_TOKEN]         = "BLOCK_ENTRY",
    [YAML_FLOW_ENTRY_TOKEN]          = "FLOW_ENTRY",
    [YAML_KEY_TOKEN]                 = "KEY",
    [YAML_VALUE_TOKEN]               = "VALUE",
    [YAML_ALIAS_TOKEN]               = "ALIAS",
    [YAML_ANCHOR_TOKEN]              = "ANCHOR",
    [YAML_TAG_TOKEN]                 = "TAG",
    [YAML_SCALAR_TOKEN]              = "SCALAR",
};

static const char *error_names[] = {
    [YAML_NO_ERROR]       = "NO_ERROR",
    [YAML_MEMORY_ERROR]   = "MEMORY_ERROR",
    [YAML_READER_ERROR]   = "READER_ERROR",
    [YAML_SCANNER_ERROR]  = "SCANNER_ERROR",
    [YAML_PARSER_ERROR]   = "PARSER_ERROR",
    [YAML_COMPOSER_ERROR]  = "COMPOSER_ERROR",
    [YAML_WRITER_ERROR]   = "WRITER_ERROR",
    [YAML_EMITTER_ERROR]  = "EMITTER_ERROR",
};

static const char *tok_name(int type) {
    if (type >= 0 && type <= YAML_SCALAR_TOKEN)
        return token_names[type] ? token_names[type] : "UNKNOWN";
    return "UNKNOWN";
}

static const char *err_name(int type) {
    if (type >= 0 && type <= YAML_EMITTER_ERROR)
        return error_names[type] ? error_names[type] : "UNKNOWN";
    return "UNKNOWN";
}

/* Print escaped string, max `max_chars` visible characters */
static void print_escaped(const unsigned char *s, size_t len, size_t max_chars) {
    size_t printed = 0;
    for (size_t i = 0; i < len && printed < max_chars; i++) {
        unsigned char c = s[i];
        if (c == '\n')      { printf("\\n"); printed += 2; }
        else if (c == '\t') { printf("\\t"); printed += 2; }
        else if (c == '\r') { printf("\\r"); printed += 2; }
        else if (c == '\\') { printf("\\\\"); printed += 2; }
        else if (c >= 32 && c < 127) { putchar(c); printed++; }
        else { printf("\\x%02x", c); printed += 4; }
    }
    if (len > max_chars) printf("...");
}

/* Print token details */
static void print_token(const char *prefix, yaml_token_t *tok,
                         unsigned long line, unsigned long col) {
    printf("%s: %s", prefix, tok_name(tok->type));

    switch (tok->type) {
    case YAML_SCALAR_TOKEN:
        printf(" \"");
        print_escaped(tok->data.scalar.value, tok->data.scalar.length, 60);
        printf("\" (len=%zu)", tok->data.scalar.length);
        break;
    case YAML_ANCHOR_TOKEN:
        printf(" &%s", tok->data.anchor.value ? (char *)tok->data.anchor.value : "(null)");
        break;
    case YAML_ALIAS_TOKEN:
        printf(" *%s", tok->data.alias.value ? (char *)tok->data.alias.value : "(null)");
        break;
    case YAML_TAG_TOKEN:
        printf(" %s%s",
               tok->data.tag.handle ? (char *)tok->data.tag.handle : "",
               tok->data.tag.suffix ? (char *)tok->data.tag.suffix : "");
        break;
    default:
        break;
    }

    printf(" @%lu:%lu", line, col);
}

static void init_ref_lib(void) {
    void *h = dlopen("libyaml-0.so.2", RTLD_LAZY);
    if (!h) {
        fprintf(stderr, "FATAL: cannot load libyaml: %s\n", dlerror());
        exit(1);
    }
    ref_init      = (fn_parser_initialize)dlsym(h, "yaml_parser_initialize");
    ref_delete    = (fn_parser_delete)dlsym(h, "yaml_parser_delete");
    ref_set_input = (fn_parser_set_input_string)dlsym(h, "yaml_parser_set_input_string");
    ref_scan      = (fn_parser_scan)dlsym(h, "yaml_parser_scan");
    ref_tok_delete= (fn_token_delete)dlsym(h, "yaml_token_delete");

    if (!ref_init || !ref_delete || !ref_set_input || !ref_scan || !ref_tok_delete) {
        fprintf(stderr, "FATAL: missing libyaml symbols\n");
        exit(1);
    }
}

static unsigned char *read_file(const char *path, size_t *out_size) {
    FILE *f;
    if (strcmp(path, "-") == 0) {
        f = stdin;
    } else {
        f = fopen(path, "rb");
        if (!f) { perror(path); return NULL; }
    }

    size_t cap = 4096, len = 0;
    unsigned char *buf = malloc(cap);
    if (!buf) { perror("malloc"); return NULL; }

    size_t n;
    while ((n = fread(buf + len, 1, cap - len, f)) > 0) {
        len += n;
        if (len == cap) {
            cap *= 2;
            unsigned char *tmp = realloc(buf, cap);
            if (!tmp) { free(buf); perror("realloc"); return NULL; }
            buf = tmp;
        }
    }

    if (f != stdin) fclose(f);
    *out_size = len;
    return buf;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <input_file | ->\n", argv[0]);
        return 1;
    }

    init_ref_lib();

    size_t size;
    unsigned char *data = read_file(argv[1], &size);
    if (!data) return 1;

    printf("Input: %zu bytes\n", size);
    printf("Raw: \"");
    print_escaped(data, size, 200);
    printf("\"\n\n");

    yaml_parser_t our, ref;
    if (!yaml_parser_initialize(&our)) {
        fprintf(stderr, "Failed to initialize our parser\n");
        free(data);
        return 1;
    }
    if (!ref_init(&ref)) {
        fprintf(stderr, "Failed to initialize ref parser\n");
        yaml_parser_delete(&our);
        free(data);
        return 1;
    }

    yaml_parser_set_input_string(&our, data, size);
    ref_set_input(&ref, data, size);

    int tok_num = 0;
    int divergences = 0;
    int error_divergences = 0;

    while (1) {
        yaml_token_t ot, rt;
        int ook = yaml_parser_scan(&our, &ot);
        int rok = ref_scan(&ref, &rt);

        tok_num++;

        if (ook != rok) {
            /* One succeeded, other failed */
            printf("[%d] ", tok_num);
            if (ook) {
                print_token("OUR", &ot,
                    (unsigned long)ot.start_mark.line,
                    (unsigned long)ot.start_mark.column);
            } else {
                printf("OUR: ERROR %s \"%s\" @%lu:%lu",
                    err_name(our.error),
                    our.problem ? our.problem : "",
                    (unsigned long)our.problem_mark.line,
                    (unsigned long)our.problem_mark.column);
            }
            printf("  |  ");
            if (rok) {
                print_token("REF", &rt,
                    (unsigned long)rt.start_mark.line,
                    (unsigned long)rt.start_mark.column);
            } else {
                printf("REF: ERROR %s \"%s\" @%lu:%lu",
                    err_name(ref.error),
                    ref.problem ? ref.problem : "",
                    (unsigned long)ref.problem_mark.line,
                    (unsigned long)ref.problem_mark.column);
            }
            printf("  *** DIVERGENCE ***\n");
            divergences++;

            if (ook) yaml_token_delete(&ot);
            if (rok) ref_tok_delete(&rt);
            break;
        }

        if (!ook) {
            /* Both failed */
            const char *our_prob = our.problem ? our.problem : "";
            const char *ref_prob = ref.problem ? ref.problem : "";
            printf("[%d] OUR ERROR: %s \"%s\" @%lu:%lu\n", tok_num,
                err_name(our.error), our_prob,
                (unsigned long)our.problem_mark.line,
                (unsigned long)our.problem_mark.column);
            printf("     REF ERROR: %s \"%s\" @%lu:%lu\n",
                err_name(ref.error), ref_prob,
                (unsigned long)ref.problem_mark.line,
                (unsigned long)ref.problem_mark.column);
            if (our.error != ref.error || strcmp(our_prob, ref_prob) != 0) {
                printf("     *** ERROR DIVERGENCE ***\n");
                error_divergences++;
            } else if (our.problem_mark.line != ref.problem_mark.line ||
                       our.problem_mark.column != ref.problem_mark.column) {
                printf("     (position differs, same error)\n");
            } else {
                printf("     (identical errors)\n");
            }
            break;
        }

        /* Both succeeded — print side by side */
        int div = 0;
        printf("[%d] ", tok_num);
        print_token("OUR", &ot,
            (unsigned long)ot.start_mark.line,
            (unsigned long)ot.start_mark.column);
        printf("  |  ");
        print_token("REF", &rt,
            (unsigned long)rt.start_mark.line,
            (unsigned long)rt.start_mark.column);

        if (ot.type != rt.type) {
            div = 1;
        } else if (ot.type == YAML_SCALAR_TOKEN) {
            if (ot.data.scalar.length != rt.data.scalar.length ||
                memcmp(ot.data.scalar.value, rt.data.scalar.value,
                       ot.data.scalar.length) != 0) {
                div = 1;
            }
        }

        if (div) {
            printf("  *** DIVERGENCE ***");
            divergences++;
        }
        printf("\n");

        int done = (ot.type == YAML_STREAM_END_TOKEN);
        yaml_token_delete(&ot);
        ref_tok_delete(&rt);
        if (done) break;
    }

    printf("\n--- Summary ---\n");
    printf("Tokens compared: %d\n", tok_num);
    printf("Output divergences: %d\n", divergences);
    printf("Error divergences: %d\n", error_divergences);

    yaml_parser_delete(&our);
    ref_delete(&ref);
    free(data);

    return (divergences + error_divergences) > 0 ? 1 : 0;
}
