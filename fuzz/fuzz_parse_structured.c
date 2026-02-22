/*
 * Structure-aware fuzz harness for the event-based parser API.
 * Uses LLVMFuzzerCustomMutator to generate YAML-aware mutations
 * rather than purely random byte flips.
 *
 * Build: clang -fsanitize=fuzzer,address -I../include -O1 -g \
 *        fuzz_parse_structured.c ../src/*.c -o fuzz_parse_structured
 */

#include <yaml.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

/* Parse input -- same as fuzz_parse.c */
int LLVMFuzzerTestOneInput(const unsigned char *data, size_t size)
{
    yaml_parser_t parser;
    yaml_event_t event;

    if (!yaml_parser_initialize(&parser))
        return 0;

    yaml_parser_set_input_string(&parser, data, size);

    while (1) {
        if (!yaml_parser_parse(&parser, &event))
            break;
        int done = (event.type == YAML_STREAM_END_EVENT);
        yaml_event_delete(&event);
        if (done)
            break;
    }

    yaml_parser_delete(&parser);
    return 0;
}

/*
 * YAML fragment templates for structure-aware mutation.
 * These represent common YAML constructs.
 */
static const char *yaml_fragments[] = {
    /* Scalars */
    "hello",
    "'single quoted'",
    "\"double quoted\"",
    "|\n  literal\n  block\n",
    ">\n  folded\n  block\n",
    "true",
    "false",
    "null",
    "~",
    "42",
    "3.14",
    ".inf",
    "-.inf",
    ".nan",
    "2024-01-15",

    /* Mappings */
    "key: value\n",
    "a: 1\nb: 2\n",
    "{a: b}",
    "{a: b, c: d}",
    "? key\n: value\n",

    /* Sequences */
    "- item\n",
    "- a\n- b\n- c\n",
    "[1, 2, 3]",
    "[a, b, c]",

    /* Nested */
    "parent:\n  child: value\n",
    "list:\n  - a\n  - b\n",
    "map:\n  key1: val1\n  key2:\n    - nested\n",
    "{a: [1, 2], b: {c: d}}",
    "- key: value\n  list:\n    - 1\n    - 2\n",

    /* Anchors and aliases */
    "&anchor value",
    "*anchor",
    "- &ref data\n- *ref\n",
    "key: &a val\nother: *a\n",

    /* Tags */
    "!!str hello",
    "!!int 42",
    "!!float 3.14",
    "!!null ~",
    "!!bool true",
    "!custom tagged",
    "!<tag:yaml.org,2002:str> hello",

    /* Directives */
    "%YAML 1.1\n---\nhello\n",
    "%YAML 1.2\n---\nhello\n",
    "%TAG !e! tag:example.com,2000:\n---\n!e!foo bar\n",

    /* Documents */
    "---\nhello\n...\n",
    "---\na: 1\n...\n---\nb: 2\n...\n",

    /* Comments */
    "# comment\nvalue\n",
    "key: value  # inline comment\n",

    /* Special characters */
    "\"\\n\\t\\\\\"",
    "\"\\x41\\u0041\\U00000041\"",

    /* Multiline */
    "key:\n  multi\n  line\n",

    /* Flow collections with edge cases */
    "[]",
    "{}",
    "[[], []]",
    "[{}, {}]",
    "{a: [], b: {}}",
    "[a: b, c: d]",

    /* Empty values */
    "key:\n",
    ":\n",
    "-\n",
    "key: \n",

    /* BOM */
    "\xEF\xBB\xBFhello\n",

    /* Line endings */
    "a: b\r\nc: d\r\n",
    "a: b\rc: d\r",

    /* Merge keys */
    "<<: {a: 1}\nb: 2\n",
};
static const int num_fragments = sizeof(yaml_fragments) / sizeof(yaml_fragments[0]);

/* Random number helper using fuzzer-provided data */
static uint32_t fuzz_rand(uint8_t **seed_ptr, size_t *seed_remain)
{
    uint32_t val = 0;
    if (*seed_remain >= 4) {
        memcpy(&val, *seed_ptr, 4);
        *seed_ptr += 4;
        *seed_remain -= 4;
    } else if (*seed_remain > 0) {
        memcpy(&val, *seed_ptr, *seed_remain);
        *seed_ptr += *seed_remain;
        *seed_remain = 0;
    }
    return val;
}

/*
 * Custom mutator: instead of random byte mutations, we perform
 * YAML-aware transformations on the input.
 */
size_t LLVMFuzzerCustomMutator(uint8_t *data, size_t size,
                                 size_t max_size, unsigned int seed)
{
    /* Use seed for random decisions */
    uint8_t *rng = (uint8_t *)&seed;
    size_t rng_remain = sizeof(seed);

    uint32_t action = fuzz_rand(&rng, &rng_remain) % 10;

    switch (action) {
    case 0:
    case 1: {
        /* Insert a YAML fragment at a random position */
        uint32_t frag_idx = fuzz_rand(&rng, &rng_remain) % num_fragments;
        const char *frag = yaml_fragments[frag_idx];
        size_t frag_len = strlen(frag);
        if (size + frag_len + 1 > max_size)
            break;
        uint32_t pos = size > 0 ? (fuzz_rand(&rng, &rng_remain) % (size + 1)) : 0;
        /* Make room */
        memmove(data + pos + frag_len, data + pos, size - pos);
        memcpy(data + pos, frag, frag_len);
        size += frag_len;
        break;
    }
    case 2: {
        /* Replace the entire input with a YAML fragment */
        uint32_t frag_idx = fuzz_rand(&rng, &rng_remain) % num_fragments;
        const char *frag = yaml_fragments[frag_idx];
        size_t frag_len = strlen(frag);
        if (frag_len > max_size)
            break;
        memcpy(data, frag, frag_len);
        size = frag_len;
        break;
    }
    case 3: {
        /* Add indentation: prepend spaces to random line */
        if (size == 0 || size + 2 > max_size)
            break;
        /* Find a newline */
        uint32_t start = fuzz_rand(&rng, &rng_remain) % size;
        size_t pos = start;
        while (pos < size && data[pos] != '\n') pos++;
        if (pos < size - 1) {
            pos++; /* after newline */
            memmove(data + pos + 2, data + pos, size - pos);
            data[pos] = ' ';
            data[pos + 1] = ' ';
            size += 2;
        }
        break;
    }
    case 4: {
        /* Insert a YAML structural character */
        if (size + 1 > max_size)
            break;
        static const char structs[] = ":-?|>!&*%#[]{},\n \t'\"";
        uint32_t ch_idx = fuzz_rand(&rng, &rng_remain) % (sizeof(structs) - 1);
        uint32_t pos = fuzz_rand(&rng, &rng_remain) % (size + 1);
        memmove(data + pos + 1, data + pos, size - pos);
        data[pos] = structs[ch_idx];
        size++;
        break;
    }
    case 5: {
        /* Wrap existing content in a collection */
        if (size + 10 > max_size)
            break;
        uint32_t wrap_type = fuzz_rand(&rng, &rng_remain) % 4;
        switch (wrap_type) {
        case 0: /* flow sequence */
            memmove(data + 1, data, size);
            data[0] = '[';
            size++;
            if (size + 1 <= max_size) { data[size] = ']'; size++; }
            break;
        case 1: /* flow mapping */
            memmove(data + 1, data, size);
            data[0] = '{';
            size++;
            if (size + 1 <= max_size) { data[size] = '}'; size++; }
            break;
        case 2: /* block sequence item */
            memmove(data + 2, data, size);
            data[0] = '-'; data[1] = ' ';
            size += 2;
            break;
        case 3: /* mapping key */
            memmove(data + 5, data, size);
            memcpy(data, "key: ", 5);
            size += 5;
            break;
        }
        break;
    }
    case 6: {
        /* Add anchor/alias */
        if (size + 8 > max_size)
            break;
        uint32_t anchor_or_alias = fuzz_rand(&rng, &rng_remain) % 2;
        const char *prefix = anchor_or_alias ? "&a " : "*a\n";
        size_t plen = strlen(prefix);
        uint32_t pos = fuzz_rand(&rng, &rng_remain) % (size + 1);
        memmove(data + pos + plen, data + pos, size - pos);
        memcpy(data + pos, prefix, plen);
        size += plen;
        break;
    }
    case 7: {
        /* Add a tag */
        if (size + 6 > max_size)
            break;
        static const char *tags[] = {"!!str ", "!!int ", "!!map ", "!!seq ", "!e! "};
        uint32_t tag_idx = fuzz_rand(&rng, &rng_remain) % 5;
        const char *tag = tags[tag_idx];
        size_t tlen = strlen(tag);
        uint32_t pos = fuzz_rand(&rng, &rng_remain) % (size + 1);
        memmove(data + pos + tlen, data + pos, size - pos);
        memcpy(data + pos, tag, tlen);
        size += tlen;
        break;
    }
    case 8: {
        /* Add document markers */
        if (size + 8 > max_size)
            break;
        uint32_t which = fuzz_rand(&rng, &rng_remain) % 2;
        const char *marker = which ? "---\n" : "...\n";
        size_t mlen = 4;
        uint32_t pos = fuzz_rand(&rng, &rng_remain) % (size + 1);
        memmove(data + pos + mlen, data + pos, size - pos);
        memcpy(data + pos, marker, mlen);
        size += mlen;
        break;
    }
    default: {
        /* Fall back to libFuzzer's default byte-level mutation */
        extern size_t LLVMFuzzerMutate(uint8_t *data, size_t size, size_t max_size);
        size = LLVMFuzzerMutate(data, size, max_size);
        break;
    }
    }

    return size;
}
