/*
 * Dump document tree from both libqyaml and reference libyaml.
 * Shows node types, scalar values, tag info, and tree structure.
 *
 * Usage: ./doc_dump <yaml_file>
 */

#include <yaml.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef int (*fn_parser_initialize)(yaml_parser_t *);
typedef void (*fn_parser_delete)(yaml_parser_t *);
typedef void (*fn_parser_set_input_string)(yaml_parser_t *, const unsigned char *, size_t);
typedef int (*fn_parser_load)(yaml_parser_t *, yaml_document_t *);
typedef void (*fn_document_delete)(yaml_document_t *);
typedef yaml_node_t *(*fn_document_get_node)(yaml_document_t *, int);
typedef yaml_node_t *(*fn_document_get_root_node)(yaml_document_t *);

static fn_parser_initialize ref_init;
static fn_parser_delete ref_delete_p;
static fn_parser_set_input_string ref_set_input;
static fn_parser_load ref_load;
static fn_document_delete ref_doc_delete;
static fn_document_get_node ref_get_node;
static fn_document_get_root_node ref_get_root;

static void init_ref(void) {
    void *lib = dlopen("libyaml-0.so.2", RTLD_NOW);
    if (!lib) { fprintf(stderr, "dlopen: %s\n", dlerror()); exit(1); }
    ref_init = dlsym(lib, "yaml_parser_initialize");
    ref_delete_p = dlsym(lib, "yaml_parser_delete");
    ref_set_input = dlsym(lib, "yaml_parser_set_input_string");
    ref_load = dlsym(lib, "yaml_parser_load");
    ref_doc_delete = dlsym(lib, "yaml_document_delete");
    ref_get_node = dlsym(lib, "yaml_document_get_node");
    ref_get_root = dlsym(lib, "yaml_document_get_root_node");
}

static const char *node_type_name(yaml_node_type_t t) {
    switch (t) {
    case YAML_NO_NODE: return "NO_NODE";
    case YAML_SCALAR_NODE: return "SCALAR";
    case YAML_SEQUENCE_NODE: return "SEQUENCE";
    case YAML_MAPPING_NODE: return "MAPPING";
    default: return "UNKNOWN";
    }
}

static void dump_node(yaml_document_t *doc, int id,
                       yaml_node_t *(*get_node)(yaml_document_t *, int),
                       int depth) {
    if (depth > 20) { printf("%*s...\n", depth * 2, ""); return; }
    yaml_node_t *n = get_node(doc, id);
    if (!n) { printf("%*s(null node id=%d)\n", depth * 2, "", id); return; }

    const char *tag = n->tag ? (const char *)n->tag : "(no tag)";

    switch (n->type) {
    case YAML_SCALAR_NODE:
        printf("%*s[%d] SCALAR tag=%s value=\"%.*s\" len=%zu style=%d\n",
               depth * 2, "", id, tag,
               (int)n->data.scalar.length,
               n->data.scalar.value,
               n->data.scalar.length,
               n->data.scalar.style);
        break;
    case YAML_SEQUENCE_NODE: {
        int count = (int)(n->data.sequence.items.top -
                          n->data.sequence.items.start);
        printf("%*s[%d] SEQUENCE tag=%s items=%d style=%d\n",
               depth * 2, "", id, tag, count,
               n->data.sequence.style);
        for (yaml_node_item_t *it = n->data.sequence.items.start;
             it < n->data.sequence.items.top; it++)
            dump_node(doc, *it, get_node, depth + 1);
        break;
    }
    case YAML_MAPPING_NODE: {
        int count = (int)(n->data.mapping.pairs.top -
                          n->data.mapping.pairs.start);
        printf("%*s[%d] MAPPING tag=%s pairs=%d style=%d\n",
               depth * 2, "", id, tag, count,
               n->data.mapping.style);
        for (yaml_node_pair_t *p = n->data.mapping.pairs.start;
             p < n->data.mapping.pairs.top; p++) {
            printf("%*skey:\n", (depth + 1) * 2, "");
            dump_node(doc, p->key, get_node, depth + 2);
            printf("%*svalue:\n", (depth + 1) * 2, "");
            dump_node(doc, p->value, get_node, depth + 2);
        }
        break;
    }
    default:
        printf("%*s[%d] %s\n", depth * 2, "", id,
               node_type_name(n->type));
        break;
    }
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <yaml_file>\n", argv[0]);
        return 1;
    }

    init_ref();

    FILE *f = fopen(argv[1], "rb");
    if (!f) { perror("fopen"); return 1; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *buf = malloc(sz);
    fread(buf, 1, sz, f);
    fclose(f);

    printf("Input: %ld bytes\n", sz);
    printf("Raw: \"");
    for (long i = 0; i < sz && i < 200; i++) {
        if (buf[i] == '\n') printf("\\n");
        else if (buf[i] == '\t') printf("\\t");
        else if (buf[i] >= 32 && buf[i] < 127) putchar(buf[i]);
        else printf("\\x%02x", buf[i]);
    }
    printf("\"\n\n");

    /* Our library */
    printf("=== LIBQYAML ===\n");
    {
        yaml_parser_t p;
        yaml_document_t doc;
        yaml_parser_initialize(&p);
        yaml_parser_set_input_string(&p, buf, sz);
        int doc_num = 0;
        while (1) {
            int ok = yaml_parser_load(&p, &doc);
            if (!ok) {
                printf("Load error: %d '%s' at %zu:%zu\n",
                       p.error, p.problem ? p.problem : "(none)",
                       p.problem_mark.line, p.problem_mark.column);
                break;
            }
            yaml_node_t *root = yaml_document_get_root_node(&doc);
            if (!root) {
                yaml_document_delete(&doc);
                break;
            }
            printf("Document %d:\n", doc_num++);
            dump_node(&doc, 1, yaml_document_get_node, 1);
            yaml_document_delete(&doc);
        }
        yaml_parser_delete(&p);
    }

    /* Reference library */
    printf("\n=== REFERENCE LIBYAML ===\n");
    {
        yaml_parser_t p;
        yaml_document_t doc;
        ref_init(&p);
        ref_set_input(&p, buf, sz);
        int doc_num = 0;
        while (1) {
            int ok = ref_load(&p, &doc);
            if (!ok) {
                printf("Load error: %d '%s' at %zu:%zu\n",
                       p.error, p.problem ? p.problem : "(none)",
                       p.problem_mark.line, p.problem_mark.column);
                break;
            }
            yaml_node_t *root = ref_get_root(&doc);
            if (!root) {
                ref_doc_delete(&doc);
                break;
            }
            printf("Document %d:\n", doc_num++);
            dump_node(&doc, 1, ref_get_node, 1);
            ref_doc_delete(&doc);
        }
        ref_delete_p(&p);
    }

    free(buf);
    return 0;
}
