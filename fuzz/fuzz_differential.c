/*
 * Differential fuzz harness: compare libqyaml vs reference libyaml.
 *
 * For each fuzzed input, feeds it to both libraries and compares outputs
 * at all three API levels (scanner tokens, parser events, loaded documents).
 * Detects both output divergences (different results) and error divergences
 * (both fail but with different error codes or descriptions).
 * Any divergence triggers an abort, which the fuzzer records as a crash.
 *
 * Build:
 *   clang -fsanitize=fuzzer,address -I../include -O1 -g \
 *         fuzz_differential.c -L../build-asan -lqyaml -ldl \
 *         -o fuzz_differential
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
typedef int (*fn_parser_parse)(yaml_parser_t *, yaml_event_t *);
typedef void (*fn_event_delete)(yaml_event_t *);
typedef int (*fn_parser_load)(yaml_parser_t *, yaml_document_t *);
typedef void (*fn_document_delete)(yaml_document_t *);
typedef yaml_node_t *(*fn_document_get_root_node)(yaml_document_t *);
typedef yaml_node_t *(*fn_document_get_node)(yaml_document_t *, int);

static fn_parser_initialize       ref_init;
static fn_parser_delete           ref_delete;
static fn_parser_set_input_string ref_set_input;
static fn_parser_scan             ref_scan;
static fn_token_delete            ref_tok_delete;
static fn_parser_parse            ref_parse;
static fn_event_delete            ref_evt_delete;
static fn_parser_load             ref_load;
static fn_document_delete         ref_doc_delete;
static fn_document_get_root_node  ref_get_root;
static fn_document_get_node       ref_get_node;

static int initialized = 0;

static void init_ref(void) {
    if (initialized) return;
    initialized = 1;

    void *h = dlopen("libyaml-0.so.2", RTLD_LAZY);
    if (!h) {
        fprintf(stderr, "FATAL: cannot load libyaml: %s\n", dlerror());
        abort();
    }

    ref_init       = (fn_parser_initialize)dlsym(h, "yaml_parser_initialize");
    ref_delete     = (fn_parser_delete)dlsym(h, "yaml_parser_delete");
    ref_set_input  = (fn_parser_set_input_string)dlsym(h, "yaml_parser_set_input_string");
    ref_scan       = (fn_parser_scan)dlsym(h, "yaml_parser_scan");
    ref_tok_delete = (fn_token_delete)dlsym(h, "yaml_token_delete");
    ref_parse      = (fn_parser_parse)dlsym(h, "yaml_parser_parse");
    ref_evt_delete = (fn_event_delete)dlsym(h, "yaml_event_delete");
    ref_load       = (fn_parser_load)dlsym(h, "yaml_parser_load");
    ref_doc_delete = (fn_document_delete)dlsym(h, "yaml_document_delete");
    ref_get_root   = (fn_document_get_root_node)dlsym(h, "yaml_document_get_root_node");
    ref_get_node   = (fn_document_get_node)dlsym(h, "yaml_document_get_node");

    if (!ref_init || !ref_delete || !ref_set_input || !ref_scan ||
        !ref_tok_delete || !ref_parse || !ref_evt_delete || !ref_load ||
        !ref_doc_delete || !ref_get_root || !ref_get_node) {
        fprintf(stderr, "FATAL: missing libyaml symbols\n");
        abort();
    }
}

/*
 * Compare scanner token streams. Returns 0 on match, 1 on divergence.
 */
static int diff_scan(const unsigned char *data, size_t size) {
    yaml_parser_t our, ref;
    yaml_token_t our_tok, ref_tok;

    if (!yaml_parser_initialize(&our)) return 0;
    if (!ref_init(&ref)) { yaml_parser_delete(&our); return 0; }

    yaml_parser_set_input_string(&our, data, size);
    ref_set_input(&ref, data, size);

    while (1) {
        int our_ok = yaml_parser_scan(&our, &our_tok);
        int ref_ok = ref_scan(&ref, &ref_tok);

        if (our_ok != ref_ok) {
            fprintf(stderr, "DIFF: scan status divergence (ours=%d, ref=%d)\n",
                    our_ok, ref_ok);
            if (our_ok) yaml_token_delete(&our_tok);
            if (ref_ok) ref_tok_delete(&ref_tok);
            yaml_parser_delete(&our);
            ref_delete(&ref);
            return 1;
        }
        if (!our_ok) {
            /* Both failed — compare error codes and descriptions */
            const char *our_prob = our.problem ? our.problem : "";
            const char *ref_prob = ref.problem ? ref.problem : "";
            if (our.error != ref.error || strcmp(our_prob, ref_prob) != 0) {
                fprintf(stderr, "DIFF: scan error divergence "
                        "(ours=%d:'%s', ref=%d:'%s')\n",
                        our.error, our_prob, ref.error, ref_prob);
                yaml_parser_delete(&our);
                ref_delete(&ref);
                return 2; /* error divergence */
            }
            break;
        }

        if (our_tok.type != ref_tok.type) {
            fprintf(stderr, "DIFF: token type divergence (ours=%d, ref=%d)\n",
                    our_tok.type, ref_tok.type);
            yaml_token_delete(&our_tok);
            ref_tok_delete(&ref_tok);
            yaml_parser_delete(&our);
            ref_delete(&ref);
            return 1;
        }

        /* Compare scalar token values */
        if (our_tok.type == YAML_SCALAR_TOKEN) {
            size_t ol = our_tok.data.scalar.length;
            size_t rl = ref_tok.data.scalar.length;
            if (ol != rl || memcmp(our_tok.data.scalar.value,
                                    ref_tok.data.scalar.value, ol) != 0) {
                fprintf(stderr, "DIFF: scalar token value divergence\n");
                yaml_token_delete(&our_tok);
                ref_tok_delete(&ref_tok);
                yaml_parser_delete(&our);
                ref_delete(&ref);
                return 1;
            }
        }

        /* Compare anchor/alias token values */
        if (our_tok.type == YAML_ANCHOR_TOKEN ||
            our_tok.type == YAML_ALIAS_TOKEN) {
            const char *ov = (const char *)our_tok.data.anchor.value;
            const char *rv = (const char *)ref_tok.data.anchor.value;
            if ((ov == NULL) != (rv == NULL) ||
                (ov && rv && strcmp(ov, rv) != 0)) {
                fprintf(stderr, "DIFF: anchor/alias token divergence\n");
                yaml_token_delete(&our_tok);
                ref_tok_delete(&ref_tok);
                yaml_parser_delete(&our);
                ref_delete(&ref);
                return 1;
            }
        }

        /* Compare tag token values */
        if (our_tok.type == YAML_TAG_TOKEN) {
            const char *oh = (const char *)our_tok.data.tag.handle;
            const char *rh = (const char *)ref_tok.data.tag.handle;
            const char *os = (const char *)our_tok.data.tag.suffix;
            const char *rs = (const char *)ref_tok.data.tag.suffix;
            if ((oh == NULL) != (rh == NULL) ||
                (oh && rh && strcmp(oh, rh) != 0) ||
                (os == NULL) != (rs == NULL) ||
                (os && rs && strcmp(os, rs) != 0)) {
                fprintf(stderr, "DIFF: tag token divergence\n");
                yaml_token_delete(&our_tok);
                ref_tok_delete(&ref_tok);
                yaml_parser_delete(&our);
                ref_delete(&ref);
                return 1;
            }
        }

        int done = (our_tok.type == YAML_STREAM_END_TOKEN);
        yaml_token_delete(&our_tok);
        ref_tok_delete(&ref_tok);
        if (done) break;
    }

    yaml_parser_delete(&our);
    ref_delete(&ref);
    return 0;
}

/*
 * Compare parser event streams. Returns 0 on match, 1 on divergence.
 */
static int diff_parse(const unsigned char *data, size_t size) {
    yaml_parser_t our, ref;
    yaml_event_t our_evt, ref_evt;

    if (!yaml_parser_initialize(&our)) return 0;
    if (!ref_init(&ref)) { yaml_parser_delete(&our); return 0; }

    yaml_parser_set_input_string(&our, data, size);
    ref_set_input(&ref, data, size);

    while (1) {
        int our_ok = yaml_parser_parse(&our, &our_evt);
        int ref_ok = ref_parse(&ref, &ref_evt);

        if (our_ok != ref_ok) {
            fprintf(stderr, "DIFF: parse status divergence (ours=%d, ref=%d)\n",
                    our_ok, ref_ok);
            if (our_ok) yaml_event_delete(&our_evt);
            if (ref_ok) ref_evt_delete(&ref_evt);
            yaml_parser_delete(&our);
            ref_delete(&ref);
            return 1;
        }
        if (!our_ok) {
            /* Both failed — compare error codes and descriptions */
            const char *our_prob = our.problem ? our.problem : "";
            const char *ref_prob = ref.problem ? ref.problem : "";
            if (our.error != ref.error || strcmp(our_prob, ref_prob) != 0) {
                fprintf(stderr, "DIFF: parse error divergence "
                        "(ours=%d:'%s', ref=%d:'%s')\n",
                        our.error, our_prob, ref.error, ref_prob);
                yaml_parser_delete(&our);
                ref_delete(&ref);
                return 2; /* error divergence */
            }
            break;
        }

        if (our_evt.type != ref_evt.type) {
            fprintf(stderr, "DIFF: event type divergence (ours=%d, ref=%d)\n",
                    our_evt.type, ref_evt.type);
            yaml_event_delete(&our_evt);
            ref_evt_delete(&ref_evt);
            yaml_parser_delete(&our);
            ref_delete(&ref);
            return 1;
        }

        /* Compare scalar event values */
        if (our_evt.type == YAML_SCALAR_EVENT) {
            size_t ol = our_evt.data.scalar.length;
            size_t rl = ref_evt.data.scalar.length;
            if (ol != rl || memcmp(our_evt.data.scalar.value,
                                    ref_evt.data.scalar.value, ol) != 0) {
                fprintf(stderr, "DIFF: scalar event value divergence\n");
                yaml_event_delete(&our_evt);
                ref_evt_delete(&ref_evt);
                yaml_parser_delete(&our);
                ref_delete(&ref);
                return 1;
            }
        }

        /* Compare alias anchor */
        if (our_evt.type == YAML_ALIAS_EVENT) {
            const char *oa = (const char *)our_evt.data.alias.anchor;
            const char *ra = (const char *)ref_evt.data.alias.anchor;
            if ((oa == NULL) != (ra == NULL) ||
                (oa && ra && strcmp(oa, ra) != 0)) {
                fprintf(stderr, "DIFF: alias anchor divergence\n");
                yaml_event_delete(&our_evt);
                ref_evt_delete(&ref_evt);
                yaml_parser_delete(&our);
                ref_delete(&ref);
                return 1;
            }
        }

        int done = (our_evt.type == YAML_STREAM_END_EVENT);
        yaml_event_delete(&our_evt);
        ref_evt_delete(&ref_evt);
        if (done) break;
    }

    yaml_parser_delete(&our);
    ref_delete(&ref);
    return 0;
}

/*
 * Recursively compare document nodes. Returns 0 on match, 1 on divergence.
 */
static int diff_node(int id, yaml_document_t *our_doc,
                      yaml_document_t *ref_doc, int depth) {
    if (depth > 64) return 0; /* prevent stack overflow on deep docs */

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
        int oc = on->data.sequence.items.top - on->data.sequence.items.start;
        int rc = rn->data.sequence.items.top - rn->data.sequence.items.start;
        if (oc != rc) return 1;
        for (int i = 0; i < oc; i++) {
            if (diff_node(on->data.sequence.items.start[i],
                          our_doc, ref_doc, depth + 1))
                return 1;
        }
        break;
    }
    case YAML_MAPPING_NODE: {
        int oc = on->data.mapping.pairs.top - on->data.mapping.pairs.start;
        int rc = rn->data.mapping.pairs.top - rn->data.mapping.pairs.start;
        if (oc != rc) return 1;
        for (int i = 0; i < oc; i++) {
            if (diff_node(on->data.mapping.pairs.start[i].key,
                          our_doc, ref_doc, depth + 1))
                return 1;
            if (diff_node(on->data.mapping.pairs.start[i].value,
                          our_doc, ref_doc, depth + 1))
                return 1;
        }
        break;
    }
    default:
        break;
    }
    return 0;
}

/*
 * Compare loaded documents. Returns 0 on match, 1 on divergence,
 * 2 on error divergence (both fail with different error info).
 */
static int diff_load(const unsigned char *data, size_t size) {
    yaml_parser_t our, ref;
    yaml_document_t our_doc, ref_doc;

    if (!yaml_parser_initialize(&our)) return 0;
    if (!ref_init(&ref)) { yaml_parser_delete(&our); return 0; }

    yaml_parser_set_input_string(&our, data, size);
    ref_set_input(&ref, data, size);

    while (1) {
        int our_ok = yaml_parser_load(&our, &our_doc);
        int ref_ok = ref_load(&ref, &ref_doc);

        if (our_ok != ref_ok) {
            fprintf(stderr, "DIFF: load status divergence (ours=%d, ref=%d)\n",
                    our_ok, ref_ok);
            if (our_ok) yaml_document_delete(&our_doc);
            if (ref_ok) ref_doc_delete(&ref_doc);
            yaml_parser_delete(&our);
            ref_delete(&ref);
            return 1;
        }
        if (!our_ok) {
            /* Both failed — compare error codes and descriptions */
            const char *our_prob = our.problem ? our.problem : "";
            const char *ref_prob = ref.problem ? ref.problem : "";
            if (our.error != ref.error || strcmp(our_prob, ref_prob) != 0) {
                fprintf(stderr, "DIFF: load error divergence "
                        "(ours=%d:'%s', ref=%d:'%s')\n",
                        our.error, our_prob, ref.error, ref_prob);
                yaml_parser_delete(&our);
                ref_delete(&ref);
                return 2; /* error divergence */
            }
            break;
        }

        yaml_node_t *or = yaml_document_get_root_node(&our_doc);
        yaml_node_t *rr = ref_get_root(&ref_doc);

        if ((or == NULL) != (rr == NULL)) {
            fprintf(stderr, "DIFF: document root existence divergence\n");
            yaml_document_delete(&our_doc);
            ref_doc_delete(&ref_doc);
            yaml_parser_delete(&our);
            ref_delete(&ref);
            return 1;
        }

        if (!or) {
            yaml_document_delete(&our_doc);
            ref_doc_delete(&ref_doc);
            break;
        }

        if (diff_node(1, &our_doc, &ref_doc, 0)) {
            fprintf(stderr, "DIFF: document node divergence\n");
            yaml_document_delete(&our_doc);
            ref_doc_delete(&ref_doc);
            yaml_parser_delete(&our);
            ref_delete(&ref);
            return 1;
        }

        yaml_document_delete(&our_doc);
        ref_doc_delete(&ref_doc);
    }

    yaml_parser_delete(&our);
    ref_delete(&ref);
    return 0;
}

int LLVMFuzzerTestOneInput(const unsigned char *data, size_t size)
{
    init_ref();

    /* Skip very large inputs to keep execution fast */
    if (size > 65536)
        return 0;

    int rc;

    rc = diff_scan(data, size);
    if (rc == 2) {
        fprintf(stderr, "ERROR_DIVERGENCE at scanner level, input size=%zu\n", size);
        abort();
    } else if (rc) {
        fprintf(stderr, "DIVERGENCE at scanner level, input size=%zu\n", size);
        abort();
    }

    rc = diff_parse(data, size);
    if (rc == 2) {
        fprintf(stderr, "ERROR_DIVERGENCE at parser level, input size=%zu\n", size);
        abort();
    } else if (rc) {
        fprintf(stderr, "DIVERGENCE at parser level, input size=%zu\n", size);
        abort();
    }

    rc = diff_load(data, size);
    if (rc == 2) {
        fprintf(stderr, "ERROR_DIVERGENCE at loader level, input size=%zu\n", size);
        abort();
    } else if (rc) {
        fprintf(stderr, "DIVERGENCE at loader level, input size=%zu\n", size);
        abort();
    }

    return 0;
}
