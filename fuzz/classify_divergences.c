/*
 * Standalone divergence classifier for libqyaml vs reference libyaml.
 * Scans all files in a seed directory, compares outputs at all API levels,
 * and categorizes divergences by type and input pattern.
 *
 * Build:
 *   clang -O1 -g -I../include classify_divergences.c -L../build -lqyaml -ldl -o classify
 * Run:
 *   ./classify <seed_directory>
 */

#include <yaml.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

/* Reference libyaml function pointers */
typedef int (*fn_parser_initialize)(yaml_parser_t *);
typedef void (*fn_parser_delete)(yaml_parser_t *);
typedef void (*fn_parser_set_input_string)(yaml_parser_t *, const unsigned char *, size_t);
typedef int (*fn_parser_scan)(yaml_parser_t *, yaml_token_t *);
typedef void (*fn_token_delete)(yaml_token_t *);
typedef int (*fn_parser_parse)(yaml_parser_t *, yaml_event_t *);
typedef void (*fn_event_delete)(yaml_event_t *);
typedef int (*fn_parser_load)(yaml_parser_t *, yaml_document_t *);
typedef void (*fn_document_delete)(yaml_document_t *);

static fn_parser_initialize ref_init;
static fn_parser_delete ref_delete;
static fn_parser_set_input_string ref_set_input;
static fn_parser_scan ref_scan;
static fn_token_delete ref_tok_delete;
static fn_parser_parse ref_parse;
static fn_event_delete ref_evt_delete;
static fn_parser_load ref_load;
static fn_document_delete ref_doc_delete;

/* Pattern detection */
static int has_mid_bom(const unsigned char *d, size_t s) {
    for (size_t i = 3; i + 2 < s; i++)
        if (d[i]==0xEF && d[i+1]==0xBB && d[i+2]==0xBF) return 1;
    return 0;
}

static int has_nondir_percent(const unsigned char *d, size_t s) {
    for (size_t i = 0; i < s; i++) {
        if (d[i] == '%' && (i == 0 || d[i-1] == '\n')) {
            /* check if it looks like a directive (%YAML, %TAG) */
            if (i + 4 < s && (memcmp(d+i, "%YAML", 5) == 0 || memcmp(d+i, "%TAG ", 5) == 0))
                continue;
            return 1; /* % at start of line but not a known directive */
        }
    }
    return 0;
}

/* Divergence types */
enum div_type { DIV_NONE=0, DIV_PERMISSIVE=1, DIV_RESTRICTIVE=2, DIV_TOKEN_TYPE=3,
                DIV_SCALAR_VALUE=4, DIV_ANCHOR_VALUE=5, DIV_TAG_VALUE=6,
                DIV_ERROR_DIVERGENCE=7 };

static const char *div_names[] = {
    "NONE", "PERMISSIVE", "RESTRICTIVE", "TOKEN_TYPE",
    "SCALAR_VALUE", "ANCHOR_VALUE", "TAG_VALUE", "ERROR_DIVERGENCE"
};

static enum div_type check_scan(const unsigned char *data, size_t size,
                                  char *our_err, size_t our_err_sz,
                                  char *ref_err, size_t ref_err_sz,
                                  int *tok_num) {
    yaml_parser_t our, ref;
    yaml_parser_initialize(&our);
    ref_init(&ref);
    yaml_parser_set_input_string(&our, data, size);
    ref_set_input(&ref, data, size);

    *tok_num = 0;
    our_err[0] = ref_err[0] = '\0';

    while (1) {
        yaml_token_t ot, rt;
        int ook = yaml_parser_scan(&our, &ot);
        int rok = ref_scan(&ref, &rt);

        if (ook != rok) {
            enum div_type dt = (ook > rok) ? DIV_PERMISSIVE : DIV_RESTRICTIVE;
            if (!ook && our.problem)
                snprintf(our_err, our_err_sz, "%s at %lu:%lu", our.problem,
                         (unsigned long)our.problem_mark.line, (unsigned long)our.problem_mark.column);
            if (!rok && ref.problem)
                snprintf(ref_err, ref_err_sz, "%s at %lu:%lu", ref.problem,
                         (unsigned long)ref.problem_mark.line, (unsigned long)ref.problem_mark.column);
            if (ook) yaml_token_delete(&ot);
            if (rok) ref_tok_delete(&rt);
            yaml_parser_delete(&our);
            ref_delete(&ref);
            return dt;
        }
        if (!ook) {
            /* Both failed — compare error codes and descriptions */
            const char *op = our.problem ? our.problem : "";
            const char *rp = ref.problem ? ref.problem : "";
            if (our.error != ref.error || strcmp(op, rp) != 0) {
                snprintf(our_err, our_err_sz, "err=%d '%s' at %lu:%lu",
                         our.error, op,
                         (unsigned long)our.problem_mark.line,
                         (unsigned long)our.problem_mark.column);
                snprintf(ref_err, ref_err_sz, "err=%d '%s' at %lu:%lu",
                         ref.error, rp,
                         (unsigned long)ref.problem_mark.line,
                         (unsigned long)ref.problem_mark.column);
                yaml_parser_delete(&our);
                ref_delete(&ref);
                return DIV_ERROR_DIVERGENCE;
            }
            break;
        }

        if (ot.type != rt.type) {
            snprintf(our_err, our_err_sz, "tok_type=%d", ot.type);
            snprintf(ref_err, ref_err_sz, "tok_type=%d", rt.type);
            yaml_token_delete(&ot);
            ref_tok_delete(&rt);
            yaml_parser_delete(&our);
            ref_delete(&ref);
            return DIV_TOKEN_TYPE;
        }

        /* Compare scalar values */
        if (ot.type == YAML_SCALAR_TOKEN) {
            if (ot.data.scalar.length != rt.data.scalar.length ||
                memcmp(ot.data.scalar.value, rt.data.scalar.value, ot.data.scalar.length)) {
                snprintf(our_err, our_err_sz, "scalar_len=%zu", ot.data.scalar.length);
                snprintf(ref_err, ref_err_sz, "scalar_len=%zu", rt.data.scalar.length);
                yaml_token_delete(&ot);
                ref_tok_delete(&rt);
                yaml_parser_delete(&our);
                ref_delete(&ref);
                return DIV_SCALAR_VALUE;
            }
        }

        int done = (ot.type == YAML_STREAM_END_TOKEN);
        yaml_token_delete(&ot);
        ref_tok_delete(&rt);
        if (done) break;
        (*tok_num)++;
    }
    yaml_parser_delete(&our);
    ref_delete(&ref);
    return DIV_NONE;
}

/* Divergence record for detailed reporting */
struct div_record {
    char filename[256];
    enum div_type scan_div;
    int has_bom;
    int has_pct;
    int tok_num;
    char our_err[256];
    char ref_err[256];
    unsigned char input_preview[80];
    size_t preview_len;
    size_t input_size;
};

#define MAX_RECORDS 2000
static struct div_record records[MAX_RECORDS];
static int nrecords = 0;

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "Usage: %s <seed_dir>\n", argv[0]); return 1; }

    void *h = dlopen("libyaml-0.so.2", RTLD_LAZY);
    if (!h) { fprintf(stderr, "Cannot load libyaml: %s\n", dlerror()); return 1; }
    ref_init = dlsym(h, "yaml_parser_initialize");
    ref_delete = dlsym(h, "yaml_parser_delete");
    ref_set_input = dlsym(h, "yaml_parser_set_input_string");
    ref_scan = dlsym(h, "yaml_parser_scan");
    ref_tok_delete = dlsym(h, "yaml_token_delete");
    ref_parse = dlsym(h, "yaml_parser_parse");
    ref_evt_delete = dlsym(h, "yaml_event_delete");
    ref_load = dlsym(h, "yaml_parser_load");
    ref_doc_delete = dlsym(h, "yaml_document_delete");

    DIR *d = opendir(argv[1]);
    if (!d) { perror("opendir"); return 1; }
    struct dirent *e;

    int total_files = 0, total_divergent = 0;
    int scan_perm = 0, scan_rest = 0, scan_type = 0, scan_scalar = 0;
    int scan_err_div = 0;
    int perm_bom = 0, perm_pct = 0, perm_other = 0;
    int rest_bom = 0, rest_pct = 0, rest_other = 0;
    int type_bom = 0, type_pct = 0, type_other = 0;
    int errdiv_bom = 0, errdiv_pct = 0, errdiv_other = 0;

    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;
        char path[4096];
        snprintf(path, sizeof(path), "%s/%s", argv[1], e->d_name);

        FILE *f = fopen(path, "rb");
        if (!f) continue;
        fseek(f, 0, SEEK_END);
        size_t sz = ftell(f);
        fseek(f, 0, SEEK_SET);
        unsigned char *buf = malloc(sz + 1);
        fread(buf, 1, sz, f);
        buf[sz] = 0;
        fclose(f);
        total_files++;

        char our_err[256], ref_err[256];
        int tok_num;
        enum div_type dt = check_scan(buf, sz, our_err, sizeof(our_err),
                                        ref_err, sizeof(ref_err), &tok_num);

        if (dt != DIV_NONE) {
            total_divergent++;
            int bom = has_mid_bom(buf, sz);
            int pct = has_nondir_percent(buf, sz);

            switch (dt) {
            case DIV_PERMISSIVE:
                scan_perm++;
                if (bom) perm_bom++;
                else if (pct) perm_pct++;
                else perm_other++;
                break;
            case DIV_RESTRICTIVE:
                scan_rest++;
                if (bom) rest_bom++;
                else if (pct) rest_pct++;
                else rest_other++;
                break;
            case DIV_TOKEN_TYPE:
                scan_type++;
                if (bom) type_bom++;
                else if (pct) type_pct++;
                else type_other++;
                break;
            case DIV_SCALAR_VALUE:
                scan_scalar++;
                break;
            case DIV_ERROR_DIVERGENCE:
                scan_err_div++;
                if (bom) errdiv_bom++;
                else if (pct) errdiv_pct++;
                else errdiv_other++;
                break;
            default:
                break;
            }

            /* Save record for detailed output */
            if (nrecords < MAX_RECORDS) {
                struct div_record *r = &records[nrecords++];
                strncpy(r->filename, e->d_name, sizeof(r->filename)-1);
                r->scan_div = dt;
                r->has_bom = bom;
                r->has_pct = pct;
                r->tok_num = tok_num;
                strncpy(r->our_err, our_err, sizeof(r->our_err)-1);
                strncpy(r->ref_err, ref_err, sizeof(r->ref_err)-1);
                r->input_size = sz;
                r->preview_len = sz < 80 ? sz : 80;
                memcpy(r->input_preview, buf, r->preview_len);
            }
        }
        free(buf);
    }
    closedir(d);

    /* Summary */
    printf("=== Divergence Classification Report ===\n");
    printf("Total files scanned: %d\n", total_files);
    printf("Total divergent:     %d\n\n", total_divergent);

    printf("--- Scanner Divergences by Type ---\n");
    printf("PERMISSIVE (ours accepts, ref rejects): %d\n", scan_perm);
    printf("  BOM-related:    %d\n", perm_bom);
    printf("  %%-related:      %d\n", perm_pct);
    printf("  Other:          %d\n\n", perm_other);

    printf("RESTRICTIVE (ours rejects, ref accepts): %d\n", scan_rest);
    printf("  BOM-related:    %d\n", rest_bom);
    printf("  %%-related:      %d\n", rest_pct);
    printf("  Other:          %d\n\n", rest_other);

    printf("TOKEN_TYPE (different token types): %d\n", scan_type);
    printf("  BOM-related:    %d\n", type_bom);
    printf("  %%-related:      %d\n", type_pct);
    printf("  Other:          %d\n\n", type_other);

    printf("SCALAR_VALUE (different scalar content): %d\n\n", scan_scalar);

    printf("ERROR_DIVERGENCE (both fail, different error info): %d\n", scan_err_div);
    printf("  BOM-related:    %d\n", errdiv_bom);
    printf("  %%-related:      %d\n", errdiv_pct);
    printf("  Other:          %d\n\n", errdiv_other);

    /* Detailed "Other" divergences (not BOM, not %) */
    printf("=== DETAILED: Non-BOM, Non-%% Divergences ===\n\n");
    int other_count = 0;
    for (int i = 0; i < nrecords; i++) {
        struct div_record *r = &records[i];
        if (r->has_bom || r->has_pct) continue;
        other_count++;

        printf("[%d] %s (%s) tok#%d, %zu bytes\n", other_count, r->filename,
               div_names[r->scan_div], r->tok_num, r->input_size);
        if (r->our_err[0]) printf("  Our: %s\n", r->our_err);
        if (r->ref_err[0]) printf("  Ref: %s\n", r->ref_err);
        printf("  Input: ");
        for (size_t j = 0; j < r->preview_len; j++) {
            unsigned char c = r->input_preview[j];
            if (c == '\n') printf("\\n");
            else if (c == '\t') printf("\\t");
            else if (c == '\r') printf("\\r");
            else if (c >= 32 && c < 127) putchar(c);
            else printf("\\x%02x", c);
        }
        printf("\n\n");
    }

    if (other_count == 0)
        printf("(none)\n");

    return 0;
}
