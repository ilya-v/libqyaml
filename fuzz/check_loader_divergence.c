/*
 * Standalone loader-level divergence checker.
 * For each input file, runs both libqyaml and reference libyaml through
 * the loader API and reports whether they agree at that level.
 *
 * Usage: ./check_loader_divergence <directory_of_crash_files>
 *
 * Reports: MATCH, BOTH-FAIL, TRUE-DIVERGENCE, ERROR-DIVERGENCE
 */

#include <yaml.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

/* Reference libyaml function pointers */
typedef int (*fn_parser_initialize)(yaml_parser_t *);
typedef void (*fn_parser_delete)(yaml_parser_t *);
typedef void (*fn_parser_set_input_string)(yaml_parser_t *, const unsigned char *, size_t);
typedef int (*fn_parser_load)(yaml_parser_t *, yaml_document_t *);
typedef void (*fn_document_delete)(yaml_document_t *);
typedef yaml_node_t *(*fn_document_get_node)(yaml_document_t *, int);
typedef yaml_node_t *(*fn_document_get_root_node)(yaml_document_t *);

static fn_parser_initialize ref_init;
static fn_parser_delete ref_delete;
static fn_parser_set_input_string ref_set_input;
static fn_parser_load ref_load;
static fn_document_delete ref_doc_delete;
static fn_document_get_node ref_get_node;
static fn_document_get_root_node ref_get_root;
static void *ref_lib;

static void init_ref(void) {
    if (ref_lib) return;
    ref_lib = dlopen("libyaml-0.so.2", RTLD_NOW);
    if (!ref_lib) { fprintf(stderr, "dlopen: %s\n", dlerror()); exit(1); }
    ref_init = dlsym(ref_lib, "yaml_parser_initialize");
    ref_delete = dlsym(ref_lib, "yaml_parser_delete");
    ref_set_input = dlsym(ref_lib, "yaml_parser_set_input_string");
    ref_load = dlsym(ref_lib, "yaml_parser_load");
    ref_doc_delete = dlsym(ref_lib, "yaml_document_delete");
    ref_get_node = dlsym(ref_lib, "yaml_document_get_node");
    ref_get_root = dlsym(ref_lib, "yaml_document_get_root_node");
}

/* Recursively compare document nodes. Returns 0=match, 1=diverge */
static int diff_node(int id, yaml_document_t *our_doc,
                      yaml_document_t *ref_doc, int depth) {
    if (depth > 64) return 0;
    yaml_node_t *on = yaml_document_get_node(our_doc, id);
    yaml_node_t *rn = ref_get_node(ref_doc, id);
    if ((on == NULL) != (rn == NULL)) return 1;
    if (!on) return 0;
    if (on->type != rn->type) return 1;

    switch (on->type) {
    case YAML_SCALAR_NODE: {
        size_t ol = on->data.scalar.length;
        size_t rl = rn->data.scalar.length;
        if (ol != rl || memcmp(on->data.scalar.value,
                                rn->data.scalar.value, ol) != 0)
            return 1;
        break;
    }
    case YAML_SEQUENCE_NODE: {
        yaml_node_item_t *oi = on->data.sequence.items.start;
        yaml_node_item_t *ri = rn->data.sequence.items.start;
        yaml_node_item_t *oe = on->data.sequence.items.top;
        yaml_node_item_t *re = rn->data.sequence.items.top;
        if ((oe - oi) != (re - ri)) return 1;
        for (; oi < oe; oi++, ri++)
            if (diff_node(*oi, our_doc, ref_doc, depth + 1)) return 1;
        break;
    }
    case YAML_MAPPING_NODE: {
        yaml_node_pair_t *op = on->data.mapping.pairs.start;
        yaml_node_pair_t *rp = rn->data.mapping.pairs.start;
        yaml_node_pair_t *ope = on->data.mapping.pairs.top;
        yaml_node_pair_t *rpe = rn->data.mapping.pairs.top;
        if ((ope - op) != (rpe - rp)) return 1;
        for (; op < ope; op++, rp++) {
            if (diff_node(op->key, our_doc, ref_doc, depth + 1)) return 1;
            if (diff_node(op->value, our_doc, ref_doc, depth + 1)) return 1;
        }
        break;
    }
    default:
        break;
    }
    return 0;
}

/*
 * Check a single input at loader level.
 * Returns: 0=MATCH, 1=TRUE-DIVERGENCE, 2=BOTH-FAIL, 3=ERROR-DIVERGENCE
 */
static int check_loader(const unsigned char *data, size_t size,
                         char *detail, size_t detail_sz) {
    yaml_parser_t our, ref;
    yaml_document_t our_doc, ref_doc;

    if (!yaml_parser_initialize(&our)) return 0;
    if (!ref_init(&ref)) { yaml_parser_delete(&our); return 0; }

    yaml_parser_set_input_string(&our, data, size);
    ref_set_input(&ref, data, size);

    int result = 0; /* MATCH */

    while (1) {
        int our_ok = yaml_parser_load(&our, &our_doc);
        int ref_ok = ref_load(&ref, &ref_doc);

        if (our_ok != ref_ok) {
            snprintf(detail, detail_sz,
                     "load status divergence: ours=%d ref=%d, "
                     "our_err=%d:'%s' ref_err=%d:'%s'",
                     our_ok, ref_ok,
                     our.error, our.problem ? our.problem : "(none)",
                     ref.error, ref.problem ? ref.problem : "(none)");
            if (our_ok) yaml_document_delete(&our_doc);
            if (ref_ok) ref_doc_delete(&ref_doc);
            result = 1; /* TRUE-DIVERGENCE */
            break;
        }

        if (!our_ok) {
            /* Both failed */
            const char *our_prob = our.problem ? our.problem : "";
            const char *ref_prob = ref.problem ? ref.problem : "";
            if (our.error != ref.error || strcmp(our_prob, ref_prob) != 0) {
                snprintf(detail, detail_sz,
                         "both fail with different error: "
                         "ours=%d:'%s' ref=%d:'%s'",
                         our.error, our_prob, ref.error, ref_prob);
                result = 3; /* ERROR-DIVERGENCE */
            } else {
                snprintf(detail, detail_sz, "both fail: err=%d '%s'",
                         our.error, our_prob);
                result = 2; /* BOTH-FAIL */
            }
            break;
        }

        yaml_node_t *or = yaml_document_get_root_node(&our_doc);
        yaml_node_t *rr = ref_get_root(&ref_doc);

        if ((or == NULL) != (rr == NULL)) {
            snprintf(detail, detail_sz, "root existence divergence");
            yaml_document_delete(&our_doc);
            ref_doc_delete(&ref_doc);
            result = 1;
            break;
        }

        if (!or) {
            /* Empty doc on both sides — end of stream */
            yaml_document_delete(&our_doc);
            ref_doc_delete(&ref_doc);
            break;
        }

        if (diff_node(1, &our_doc, &ref_doc, 0)) {
            snprintf(detail, detail_sz, "document node content divergence");
            yaml_document_delete(&our_doc);
            ref_doc_delete(&ref_doc);
            result = 1;
            break;
        }

        yaml_document_delete(&our_doc);
        ref_doc_delete(&ref_doc);
    }

    yaml_parser_delete(&our);
    ref_delete(&ref);
    return result;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <crash_directory>\n", argv[0]);
        return 1;
    }

    init_ref();

    DIR *d = opendir(argv[1]);
    if (!d) { perror("opendir"); return 1; }

    int total = 0, match = 0, true_div = 0, both_fail = 0, error_div = 0;
    struct dirent *ent;

    while ((ent = readdir(d))) {
        if (ent->d_name[0] == '.') continue;

        char path[4096];
        snprintf(path, sizeof(path), "%s/%s", argv[1], ent->d_name);

        struct stat st;
        if (stat(path, &st) || !S_ISREG(st.st_mode)) continue;

        FILE *f = fopen(path, "rb");
        if (!f) continue;
        unsigned char *buf = malloc(st.st_size);
        if (!buf) { fclose(f); continue; }
        size_t nread = fread(buf, 1, st.st_size, f);
        fclose(f);

        char detail[512] = "";
        int rc = check_loader(buf, nread, detail, sizeof(detail));
        total++;

        const char *label;
        switch (rc) {
        case 0: label = "MATCH"; match++; break;
        case 1: label = "TRUE-DIVERGENCE"; true_div++; break;
        case 2: label = "BOTH-FAIL"; both_fail++; break;
        case 3: label = "ERROR-DIVERGENCE"; error_div++; break;
        default: label = "UNKNOWN"; break;
        }

        if (rc == 1 || rc == 3) {
            printf("[%s] %s: %s\n", label, ent->d_name, detail);
        }

        free(buf);
    }
    closedir(d);

    printf("\n=== LOADER-LEVEL SUMMARY ===\n");
    printf("Total inputs:       %d\n", total);
    printf("MATCH:              %d\n", match);
    printf("BOTH-FAIL:          %d (cosmetic, both reject)\n", both_fail);
    printf("TRUE-DIVERGENCE:    %d (one succeeds, other fails, or different output)\n", true_div);
    printf("ERROR-DIVERGENCE:   %d (both fail, different error info)\n", error_div);
    printf("\n");

    if (true_div > 0) {
        printf("*** CRITICAL: %d true loader-level divergences found ***\n", true_div);
        return 1;
    }
    if (error_div > 0) {
        printf("*** WARNING: %d error divergences (both fail, different error messages) ***\n", error_div);
        return 2;
    }
    printf("CLEAN: No loader-level divergences.\n");
    return 0;
}
