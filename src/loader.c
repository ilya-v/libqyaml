
#include "yaml_private.h"

/*
 * Token access macros (replicated from parser.c for loader fast paths).
 */

#define LOADER_PEEK_TOKEN(parser)                                               \
    ((parser->token_available || yaml_parser_fetch_more_tokens(parser)) ?       \
        parser->tokens.head : NULL)

#define LOADER_SKIP_TOKEN(parser)                                               \
    (parser->tokens_parsed ++,                                                  \
     parser->stream_end_produced =                                              \
        (parser->tokens.head->type == YAML_STREAM_END_TOKEN),                   \
     parser->tokens.head ++,                                                    \
     parser->token_available =                                                  \
        (__builtin_expect(parser->tokens.head != parser->tokens.tail            \
            && parser->possible_simple_key_count == 0, 1)))

/*
 * Interned default tag strings. These are shared across all nodes
 * to avoid per-node malloc for the common case of untagged nodes.
 * yaml_document_delete checks for these pointers to skip freeing them.
 */

const yaml_char_t yaml_interned_str_tag[] = YAML_DEFAULT_SCALAR_TAG;
const yaml_char_t yaml_interned_seq_tag[] = YAML_DEFAULT_SEQUENCE_TAG;
const yaml_char_t yaml_interned_map_tag[] = YAML_DEFAULT_MAPPING_TAG;

/*
 * API functions.
 */

YAML_DECLARE(int)
yaml_parser_load(yaml_parser_t *parser, yaml_document_t *document);

/*
 * Error handling.
 */

static int
yaml_parser_set_composer_error(yaml_parser_t *parser,
        const char *problem, yaml_mark_t problem_mark);

static int
yaml_parser_set_composer_error_context(yaml_parser_t *parser,
        const char *context, yaml_mark_t context_mark,
        const char *problem, yaml_mark_t problem_mark);


/*
 * Alias handling.
 */

static int
yaml_parser_register_anchor(yaml_parser_t *parser,
        int index, yaml_char_t *anchor);

/*
 * Clean up functions.
 */

static void
yaml_parser_delete_aliases(yaml_parser_t *parser);

/*
 * Document loading context.
 */
struct loader_ctx {
    int *start;
    int *end;
    int *top;
};

/*
 * Composer functions.
 */
static int
yaml_parser_load_nodes(yaml_parser_t *parser, struct loader_ctx *ctx);

static int
yaml_parser_load_document(yaml_parser_t *parser, yaml_event_t *event);

static int
yaml_parser_load_alias(yaml_parser_t *parser, yaml_event_t *event,
        struct loader_ctx *ctx);

static int
yaml_parser_load_scalar(yaml_parser_t *parser, yaml_event_t *event,
        struct loader_ctx *ctx);

static int
yaml_parser_load_sequence(yaml_parser_t *parser, yaml_event_t *event,
        struct loader_ctx *ctx);

static int
yaml_parser_load_mapping(yaml_parser_t *parser, yaml_event_t *event,
        struct loader_ctx *ctx);

static int
yaml_parser_load_sequence_end(yaml_parser_t *parser, yaml_event_t *event,
        struct loader_ctx *ctx);

static int
yaml_parser_load_mapping_end(yaml_parser_t *parser, yaml_event_t *event,
        struct loader_ctx *ctx);

static int
yaml_parser_load_mapping_pairs_batch(yaml_parser_t *parser,
        struct loader_ctx *ctx);

static int
yaml_parser_load_sequence_items_batch(yaml_parser_t *parser,
        struct loader_ctx *ctx);

/*
 * Load the next document of the stream.
 */

YAML_DECLARE(int)
yaml_parser_load(yaml_parser_t *parser, yaml_document_t *document)
{
    yaml_event_t event;

    assert(parser);     /* Non-NULL parser object is expected. */
    assert(document);   /* Non-NULL document object is expected. */

    memset(document, 0, sizeof(yaml_document_t));

    /* Estimate initial node count from input size to avoid reallocs.
     * Each node represents roughly 10 bytes of YAML on average.
     * Use the default stack size (16) for small inputs to avoid
     * over-allocating for tiny documents. Scale up for larger inputs,
     * clamped to 16384. Only for string inputs (file inputs don't
     * know total size upfront). */
    {
        size_t init_nodes = INITIAL_STACK_SIZE;
        if (parser->input.string.start && parser->input.string.end
                && parser->input.string.end > parser->input.string.start) {
            size_t input_size = parser->input.string.end
                    - parser->input.string.start;
            size_t estimate = input_size / 10;
            if (estimate > init_nodes) {
                /* Round up to next power of 2 */
                init_nodes = INITIAL_STACK_SIZE;
                while (init_nodes < estimate && init_nodes < 16384)
                    init_nodes *= 2;
            }
        }
        if (!STACK_INIT_SIZED(parser, document->nodes, yaml_node_t*, init_nodes))
            goto error;
    }

    if (!parser->stream_start_produced) {
        if (!yaml_parser_parse(parser, &event)) goto error;
        assert(event.type == YAML_STREAM_START_EVENT);
                        /* STREAM-START is expected. */
    }

    if (parser->stream_end_produced) {
        return 1;
    }

    if (!yaml_parser_parse(parser, &event)) goto error;
    if (event.type == YAML_STREAM_END_EVENT) {
        return 1;
    }

    /* Reuse existing aliases stack if already allocated (avoids
     * malloc/free per load call). Allocate only on first use. */
    if (parser->aliases.start) {
        /* Free any leftover anchors (shouldn't happen, but defensive) */
        while (!STACK_EMPTY(parser, parser->aliases)) {
            yaml_free(POP(parser, parser->aliases).anchor);
        }
    } else {
        if (!STACK_INIT(parser, parser->aliases, yaml_alias_data_t*))
            goto error_with_event;
    }

    parser->document = document;

    if (!yaml_parser_load_document(parser, &event)) goto error;

    yaml_parser_delete_aliases(parser);
    parser->document = NULL;

    return 1;

error_with_event:
    yaml_event_delete(&event);

error:

    yaml_parser_delete_aliases(parser);
    yaml_document_delete(document);
    parser->document = NULL;

    return 0;
}

/*
 * Set composer error.
 */

static int
yaml_parser_set_composer_error(yaml_parser_t *parser,
        const char *problem, yaml_mark_t problem_mark)
{
    parser->error = YAML_COMPOSER_ERROR;
    parser->problem = problem;
    parser->problem_mark = problem_mark;

    return 0;
}

/*
 * Set composer error with context.
 */

static int
yaml_parser_set_composer_error_context(yaml_parser_t *parser,
        const char *context, yaml_mark_t context_mark,
        const char *problem, yaml_mark_t problem_mark)
{
    parser->error = YAML_COMPOSER_ERROR;
    parser->context = context;
    parser->context_mark = context_mark;
    parser->problem = problem;
    parser->problem_mark = problem_mark;

    return 0;
}

/*
 * Delete the stack of aliases.
 */

static void
yaml_parser_delete_aliases(yaml_parser_t *parser)
{
    while (!STACK_EMPTY(parser, parser->aliases)) {
        yaml_free(POP(parser, parser->aliases).anchor);
    }
    /* Don't free the stack itself -- it will be reused by the next
     * yaml_parser_load call. The stack memory is freed when the
     * parser is destroyed (yaml_parser_delete). */
}

/*
 * Compose a document object.
 */

static int
yaml_parser_load_document(yaml_parser_t *parser, yaml_event_t *event)
{
    struct loader_ctx ctx = { NULL, NULL, NULL };

    assert(event->type == YAML_DOCUMENT_START_EVENT);
                        /* DOCUMENT-START is expected. */

    parser->document->version_directive
        = event->data.document_start.version_directive;
    parser->document->tag_directives.start
        = event->data.document_start.tag_directives.start;
    parser->document->tag_directives.end
        = event->data.document_start.tag_directives.end;
    parser->document->start_implicit
        = event->data.document_start.implicit;
    parser->document->start_mark = event->start_mark;

    if (!STACK_INIT(parser, ctx, int*)) return 0;
    if (!yaml_parser_load_nodes(parser, &ctx)) {
        STACK_DEL(parser, ctx);
        return 0;
    }
    STACK_DEL(parser, ctx);

    return 1;
}

/*
 * Compose a node tree.
 */

static int
yaml_parser_load_nodes(yaml_parser_t *parser, struct loader_ctx *ctx)
{
    yaml_event_t event;

    do {
        if (!yaml_parser_parse(parser, &event)) return 0;

        switch (event.type) {
            case YAML_ALIAS_EVENT:
                if (!yaml_parser_load_alias(parser, &event, ctx)) return 0;
                break;
            case YAML_SCALAR_EVENT:
                if (!yaml_parser_load_scalar(parser, &event, ctx)) return 0;
                break;
            case YAML_SEQUENCE_START_EVENT:
                if (!yaml_parser_load_sequence(parser, &event, ctx)) return 0;
                if ((parser->state == YAML_PARSE_BLOCK_SEQUENCE_FIRST_ENTRY_STATE ||
                        parser->state == YAML_PARSE_BLOCK_SEQUENCE_ENTRY_STATE) &&
                        !yaml_parser_load_sequence_items_batch(parser, ctx))
                    return 0;
                break;
            case YAML_SEQUENCE_END_EVENT:
                if (!yaml_parser_load_sequence_end(parser, &event, ctx))
                    return 0;
                break;
            case YAML_MAPPING_START_EVENT:
                if (!yaml_parser_load_mapping(parser, &event, ctx)) return 0;
                if ((parser->state == YAML_PARSE_BLOCK_MAPPING_FIRST_KEY_STATE ||
                        parser->state == YAML_PARSE_BLOCK_MAPPING_KEY_STATE) &&
                        !yaml_parser_load_mapping_pairs_batch(parser, ctx))
                    return 0;
                break;
            case YAML_MAPPING_END_EVENT:
                if (!yaml_parser_load_mapping_end(parser, &event, ctx))
                    return 0;
                break;
            default:
                assert(0);  /* Could not happen. */
                return 0;
            case YAML_DOCUMENT_END_EVENT:
                break;
        }
    } while (event.type != YAML_DOCUMENT_END_EVENT);

    parser->document->end_implicit = event.data.document_end.implicit;
    parser->document->end_mark = event.end_mark;

    return 1;
}

/*
 * Add an anchor.
 */

static int
yaml_parser_register_anchor(yaml_parser_t *parser,
        int index, yaml_char_t *anchor)
{
    yaml_alias_data_t data;
    yaml_alias_data_t *alias_data;

    if (!anchor) return 1;

    data.anchor = anchor;
    data.index = index;
    data.mark = parser->document->nodes.start[index-1].start_mark;

    for (alias_data = parser->aliases.start;
            alias_data != parser->aliases.top; alias_data ++) {
        if (strcmp((char *)alias_data->anchor, (char *)anchor) == 0) {
            yaml_free(anchor);
            return yaml_parser_set_composer_error_context(parser,
                    "found duplicate anchor; first occurrence",
                    alias_data->mark, "second occurrence", data.mark);
        }
    }

    if (!PUSH(parser, parser->aliases, data)) {
        yaml_free(anchor);
        return 0;
    }

    return 1;
}

/*
 * Compose node into its parent in the stree.
 */

static int
yaml_parser_load_node_add(yaml_parser_t *parser, struct loader_ctx *ctx,
        int index)
{
    struct yaml_node_s *parent;
    int parent_index;

    if (STACK_EMPTY(parser, *ctx)) {
        /* This is the root node, there's no tree to add it to. */
        return 1;
    }

    parent_index = *((*ctx).top - 1);
    parent = &parser->document->nodes.start[parent_index-1];

    switch (parent->type) {
        case YAML_SEQUENCE_NODE:
            if (!STACK_LIMIT(parser, parent->data.sequence.items, INT_MAX-1))
                return 0;
            if (!PUSH(parser, parent->data.sequence.items, index))
                return 0;
            break;
        case YAML_MAPPING_NODE: {
            yaml_node_pair_t pair;
            if (!STACK_EMPTY(parser, parent->data.mapping.pairs)) {
                yaml_node_pair_t *p = parent->data.mapping.pairs.top - 1;
                if (p->key != 0 && p->value == 0) {
                    p->value = index;
                    break;
                }
            }

            pair.key = index;
            pair.value = 0;
            if (!STACK_LIMIT(parser, parent->data.mapping.pairs, INT_MAX-1))
                return 0;
            if (!PUSH(parser, parent->data.mapping.pairs, pair))
                return 0;

            break;
        }
        default:
            assert(0); /* Could not happen. */
            return 0;
    }
    return 1;
}

/*
 * Compose a node corresponding to an alias.
 */

static int
yaml_parser_load_alias(yaml_parser_t *parser, yaml_event_t *event,
        struct loader_ctx *ctx)
{
    yaml_char_t *anchor = event->data.alias.anchor;
    yaml_alias_data_t *alias_data;

    for (alias_data = parser->aliases.start;
            alias_data != parser->aliases.top; alias_data ++) {
        if (strcmp((char *)alias_data->anchor, (char *)anchor) == 0) {
            yaml_free(anchor);
            return yaml_parser_load_node_add(parser, ctx, alias_data->index);
        }
    }

    yaml_free(anchor);
    return yaml_parser_set_composer_error(parser, "found undefined alias",
            event->start_mark);
}

/*
 * Compose a scalar node.
 */

static int
yaml_parser_load_scalar(yaml_parser_t *parser, yaml_event_t *event,
        struct loader_ctx *ctx)
{
    yaml_node_t *node;
    int index;
    yaml_char_t *tag = event->data.scalar.tag;

    if (!STACK_LIMIT(parser, parser->document->nodes, INT_MAX-1)) goto error;

    if (!tag) {
        tag = (yaml_char_t *)yaml_interned_str_tag;
    } else if (strcmp((char *)tag, "!") == 0) {
        yaml_free(tag);
        tag = (yaml_char_t *)yaml_interned_str_tag;
    }

    if (!STACK_PUSH_RESERVE(parser, parser->document->nodes)) goto error;

    /* Move scalar value into the document's string arena to avoid
     * per-scalar malloc/free overhead in document_delete. */
    yaml_char_t *value = event->data.scalar.value;
    size_t length = event->data.scalar.length;
    yaml_char_t *arena_value = yaml_arena_alloc(parser->document, length + 1);
    if (!arena_value) goto error;
    memcpy(arena_value, value, length + 1);
    yaml_free_internal(value);
    value = arena_value;

    node = parser->document->nodes.top;
    node->type = YAML_SCALAR_NODE;
    node->tag = tag;
    node->data.scalar.value = value;
    node->data.scalar.length = length;
    node->data.scalar.style = event->data.scalar.style;
    node->start_mark = event->start_mark;
    node->end_mark = event->end_mark;

    STACK_PUSH_COMMIT(parser->document->nodes);

    index = parser->document->nodes.top - parser->document->nodes.start;

    if (event->data.scalar.anchor) {
        if (!yaml_parser_register_anchor(parser, index,
                    event->data.scalar.anchor)) return 0;
    }

    return yaml_parser_load_node_add(parser, ctx, index);

error:
    if (!YAML_TAG_IS_INTERNED(tag))
        yaml_free(tag);
    yaml_free(event->data.scalar.anchor);
    yaml_free(event->data.scalar.value);
    return 0;
}

/*
 * Compose a sequence node.
 */

static int
yaml_parser_load_sequence(yaml_parser_t *parser, yaml_event_t *event,
        struct loader_ctx *ctx)
{
    yaml_node_t *node;
    struct {
        yaml_node_item_t *start;
        yaml_node_item_t *end;
        yaml_node_item_t *top;
    } items = { NULL, NULL, NULL };
    int index;
    yaml_char_t *tag = event->data.sequence_start.tag;

    if (!STACK_LIMIT(parser, parser->document->nodes, INT_MAX-1)) goto error;

    if (!tag) {
        tag = (yaml_char_t *)yaml_interned_seq_tag;
    } else if (strcmp((char *)tag, "!") == 0) {
        yaml_free(tag);
        tag = (yaml_char_t *)yaml_interned_seq_tag;
    }

    if (!STACK_INIT(parser, items, yaml_node_item_t*)) goto error;

    if (!STACK_PUSH_RESERVE(parser, parser->document->nodes))
        goto error_after_items;

    node = parser->document->nodes.top;
    node->type = YAML_SEQUENCE_NODE;
    node->tag = tag;
    node->data.sequence.items.start = items.start;
    node->data.sequence.items.end = items.end;
    node->data.sequence.items.top = items.start;
    node->data.sequence.style = event->data.sequence_start.style;
    node->start_mark = event->start_mark;
    node->end_mark = event->end_mark;

    STACK_PUSH_COMMIT(parser->document->nodes);

    index = parser->document->nodes.top - parser->document->nodes.start;

    if (event->data.sequence_start.anchor) {
        if (!yaml_parser_register_anchor(parser, index,
                    event->data.sequence_start.anchor)) return 0;
    }

    if (!yaml_parser_load_node_add(parser, ctx, index)) return 0;

    if (!STACK_LIMIT(parser, *ctx, INT_MAX-1)) return 0;
    if (!PUSH(parser, *ctx, index)) return 0;

    return 1;

error_after_items:
    STACK_DEL(parser, items);

error:
    if (!YAML_TAG_IS_INTERNED(tag))
        yaml_free(tag);
    yaml_free(event->data.sequence_start.anchor);
    return 0;
}

static int
yaml_parser_load_sequence_end(yaml_parser_t *parser, yaml_event_t *event,
        struct loader_ctx *ctx)
{
    int index;

    assert(((*ctx).top - (*ctx).start) > 0);

    index = *((*ctx).top - 1);
    assert(parser->document->nodes.start[index-1].type == YAML_SEQUENCE_NODE);
    parser->document->nodes.start[index-1].end_mark = event->end_mark;

    (void)POP(parser, *ctx);

    return 1;
}

/*
 * Compose a mapping node.
 */

static int
yaml_parser_load_mapping(yaml_parser_t *parser, yaml_event_t *event,
        struct loader_ctx *ctx)
{
    yaml_node_t *node;
    struct {
        yaml_node_pair_t *start;
        yaml_node_pair_t *end;
        yaml_node_pair_t *top;
    } pairs = { NULL, NULL, NULL };
    int index;
    yaml_char_t *tag = event->data.mapping_start.tag;

    if (!STACK_LIMIT(parser, parser->document->nodes, INT_MAX-1)) goto error;

    if (!tag) {
        tag = (yaml_char_t *)yaml_interned_map_tag;
    } else if (strcmp((char *)tag, "!") == 0) {
        yaml_free(tag);
        tag = (yaml_char_t *)yaml_interned_map_tag;
    }

    if (!STACK_INIT(parser, pairs, yaml_node_pair_t*)) goto error;

    if (!STACK_PUSH_RESERVE(parser, parser->document->nodes))
        goto error_after_pairs;

    node = parser->document->nodes.top;
    node->type = YAML_MAPPING_NODE;
    node->tag = tag;
    node->data.mapping.pairs.start = pairs.start;
    node->data.mapping.pairs.end = pairs.end;
    node->data.mapping.pairs.top = pairs.start;
    node->data.mapping.style = event->data.mapping_start.style;
    node->start_mark = event->start_mark;
    node->end_mark = event->end_mark;

    STACK_PUSH_COMMIT(parser->document->nodes);

    index = parser->document->nodes.top - parser->document->nodes.start;

    if (event->data.mapping_start.anchor) {
        if (!yaml_parser_register_anchor(parser, index,
                    event->data.mapping_start.anchor)) return 0;
    }

    if (!yaml_parser_load_node_add(parser, ctx, index)) return 0;

    if (!STACK_LIMIT(parser, *ctx, INT_MAX-1)) return 0;
    if (!PUSH(parser, *ctx, index)) return 0;

    return 1;

error_after_pairs:
    STACK_DEL(parser, pairs);

error:
    if (!YAML_TAG_IS_INTERNED(tag))
        yaml_free(tag);
    yaml_free(event->data.mapping_start.anchor);
    return 0;
}

/*
 * Create a plain scalar node from a malloc'd value string by copying it
 * into the document's string arena. The caller must have already called
 * STACK_PUSH_RESERVE to ensure space at document->nodes.top.
 *
 * On success: frees the original value, fills the node, commits it,
 * and returns the 1-based node index (always > 0).
 * On failure (OOM): leaves the original value unfreed (caller must
 * handle cleanup) and returns 0.
 */
static inline __attribute__((always_inline)) int
loader_create_plain_scalar(yaml_document_t *document,
        yaml_char_t *value, size_t length,
        yaml_mark_t start_mark, yaml_mark_t end_mark)
{
    yaml_char_t *arena_val = yaml_arena_alloc(document, length + 1);
    if (!arena_val) return 0;
    memcpy(arena_val, value, length + 1);
    yaml_free_internal(value);

    yaml_node_t *node = document->nodes.top;
    node->type = YAML_SCALAR_NODE;
    node->tag = (yaml_char_t *)yaml_interned_str_tag;
    node->data.scalar.value = arena_val;
    node->data.scalar.length = length;
    node->data.scalar.style = YAML_PLAIN_SCALAR_STYLE;
    node->start_mark = start_mark;
    node->end_mark = end_mark;
    STACK_PUSH_COMMIT(document->nodes);

    return (int)(document->nodes.top - document->nodes.start);
}

/*
 * Batch-load plain scalar key-value pairs directly from the token queue.
 * This bypasses the parser state machine for the common case of block
 * mappings containing only plain scalar keys and values without anchors
 * or tags. Falls back to the normal event loop for anything else.
 *
 * The parser must be in BLOCK_MAPPING_FIRST_KEY_STATE or
 * BLOCK_MAPPING_KEY_STATE when this is called.
 */

static int
yaml_parser_load_mapping_pairs_batch(yaml_parser_t *parser,
        struct loader_ctx *ctx)
{
    yaml_token_t *token;
    yaml_document_t *document = parser->document;
    int mapping_index;

    mapping_index = *((*ctx).top - 1);

    /* On first entry, the BLOCK-MAPPING-START token needs to be consumed. */
    if (parser->state == YAML_PARSE_BLOCK_MAPPING_FIRST_KEY_STATE) {
        token = LOADER_PEEK_TOKEN(parser);
        if (!token) return 0;
        if (!PUSH(parser, parser->marks, token->start_mark))
            return 0;
        LOADER_SKIP_TOKEN(parser);
    }

    for (;;) {
        yaml_token_t *value_scalar;
        /* Key scalar data is copied into locals before skipping, because
         * subsequent LOADER_PEEK_TOKEN calls may reallocate the token
         * queue and invalidate any pointer into it. */
        yaml_char_t *key_value;
        size_t key_length;
        yaml_mark_t key_start_mark, key_end_mark;

        /* Peek at the current token -- must be a KEY. */
        token = LOADER_PEEK_TOKEN(parser);
        if (!token) return 0;

        if (token->type != YAML_KEY_TOKEN)
            break;  /* BLOCK_END or error -- fall back to normal path */

        /* Skip KEY token, peek next. */
        LOADER_SKIP_TOKEN(parser);
        token = LOADER_PEEK_TOKEN(parser);
        if (!token) return 0;

        /* Must be a plain scalar with no anchor -- otherwise fall back. */
        if (token->type != YAML_SCALAR_TOKEN ||
                token->data.scalar.style != YAML_PLAIN_SCALAR_STYLE)
            goto fallback_key;

        /* Copy key scalar fields into locals before skipping. After the
         * skip, the token queue may be reallocated by the next PEEK,
         * invalidating the token pointer. */
        key_value = token->data.scalar.value;
        key_length = token->data.scalar.length;
        key_start_mark = token->start_mark;
        key_end_mark = token->end_mark;

        /* Skip scalar, peek for VALUE token.
         * After this skip, key_value is orphaned from the token queue --
         * we MUST free it on any error path before returning. */
        LOADER_SKIP_TOKEN(parser);
        token = LOADER_PEEK_TOKEN(parser);
        if (!token) goto error_free_key;

        if (token->type != YAML_VALUE_TOKEN)
            goto fallback_value_after_key;

        /* Skip VALUE token, peek next. */
        LOADER_SKIP_TOKEN(parser);
        value_scalar = LOADER_PEEK_TOKEN(parser);
        if (!value_scalar) goto error_free_key;

        /* Check for empty value (KEY, VALUE, or BLOCK_END after VALUE). */
        if (value_scalar->type == YAML_KEY_TOKEN ||
                value_scalar->type == YAML_VALUE_TOKEN ||
                value_scalar->type == YAML_BLOCK_END_TOKEN)
            goto batch_empty_value;

        /* Must be a plain scalar with no anchor -- otherwise fall back. */
        if (value_scalar->type != YAML_SCALAR_TOKEN ||
                value_scalar->data.scalar.style != YAML_PLAIN_SCALAR_STYLE)
            goto fallback_value;

        /* We have a complete key: plain_scalar, value: plain_scalar pair.
         * Create both nodes directly, bypassing the parser state machine. */

        /* Reserve space for 2 nodes. */
        if (!STACK_LIMIT(parser, document->nodes, INT_MAX-2))
            goto error_free_key;
        if (!STACK_PUSH_RESERVE(parser, document->nodes))
            goto error_free_key;

        /* Create key node. */
        int key_index = loader_create_plain_scalar(document,
                key_value, key_length, key_start_mark, key_end_mark);
        if (!key_index) goto error_free_key;
        key_value = NULL;  /* helper freed it */

        if (!STACK_PUSH_RESERVE(parser, document->nodes)) return 0;

        /* Create value node.
         * value_scalar is peeked, not skipped. On success, the helper
         * frees the value string so we NULL the token's copy to prevent
         * double-free during parser cleanup. On failure, the value is
         * still in the token and parser cleanup handles it correctly. */
        int value_index = loader_create_plain_scalar(document,
                value_scalar->data.scalar.value,
                value_scalar->data.scalar.length,
                value_scalar->start_mark, value_scalar->end_mark);
        if (!value_index) return 0;
        value_scalar->data.scalar.value = NULL;  /* helper freed it */

        /* Add key-value pair to the mapping node. */
        {
            yaml_node_t *mapping = &document->nodes.start[mapping_index - 1];
            yaml_node_pair_t pair;
            pair.key = key_index;
            pair.value = value_index;
            if (!STACK_LIMIT(parser, mapping->data.mapping.pairs, INT_MAX-1))
                return 0;
            if (!PUSH(parser, mapping->data.mapping.pairs, pair))
                return 0;
        }

        /* Skip the value scalar token. */
        LOADER_SKIP_TOKEN(parser);

        /* Parser state stays at BLOCK_MAPPING_KEY_STATE for next iteration. */
        continue;

    batch_empty_value:
        /* We consumed KEY + scalar_key + VALUE, and the next token
         * indicates an empty value (KEY, VALUE, or BLOCK_END).
         * Create key node + empty scalar value node. */
        {
            if (!STACK_LIMIT(parser, document->nodes, INT_MAX-2))
                goto error_free_key;
            if (!STACK_PUSH_RESERVE(parser, document->nodes))
                goto error_free_key;

            /* Create key node. */
            int ki = loader_create_plain_scalar(document,
                    key_value, key_length, key_start_mark, key_end_mark);
            if (!ki) goto error_free_key;
            key_value = NULL;  /* helper freed it */

            /* Create empty value node. */
            if (!STACK_PUSH_RESERVE(parser, document->nodes)) return 0;

            yaml_char_t *empty_val = yaml_arena_alloc(document, 1);
            if (!empty_val) return 0;
            empty_val[0] = '\0';

            yaml_node_t *vnode = document->nodes.top;
            vnode->type = YAML_SCALAR_NODE;
            vnode->tag = (yaml_char_t *)yaml_interned_str_tag;
            vnode->data.scalar.value = empty_val;
            vnode->data.scalar.length = 0;
            vnode->data.scalar.style = YAML_PLAIN_SCALAR_STYLE;
            vnode->start_mark = value_scalar->start_mark;
            vnode->end_mark = value_scalar->start_mark;
            STACK_PUSH_COMMIT(document->nodes);

            int vi = document->nodes.top - document->nodes.start;

            /* Add complete pair to mapping. */
            yaml_node_t *mapping_node = &document->nodes.start[mapping_index - 1];
            yaml_node_pair_t pair;
            pair.key = ki;
            pair.value = vi;
            if (!PUSH(parser, mapping_node->data.mapping.pairs, pair))
                return 0;
        }
        /* Continue scanning for more pairs. */
        continue;

    fallback_key:
        /* We consumed the KEY token but the next token isn't a plain scalar.
         * Set parser state so normal event loop processes the key node. */
        parser->state = YAML_PARSE_BLOCK_NODE_OR_INDENTLESS_SEQUENCE_STATE;
        if (!PUSH(parser, parser->states,
                    YAML_PARSE_BLOCK_MAPPING_VALUE_STATE))
            return 0;
        return 1;

    fallback_value_after_key:
        /* We consumed KEY + scalar key, but no VALUE token follows.
         * The key scalar is already consumed. Create the key node,
         * and set parser state to expect the value. */
        {
            if (!STACK_LIMIT(parser, document->nodes, INT_MAX-1))
                goto error_free_key;
            if (!STACK_PUSH_RESERVE(parser, document->nodes))
                goto error_free_key;

            int ki = loader_create_plain_scalar(document,
                    key_value, key_length, key_start_mark, key_end_mark);
            if (!ki) goto error_free_key;
            key_value = NULL;  /* helper freed it */

            /* Add partial pair (key only) to mapping. */
            yaml_node_t *mapping = &document->nodes.start[mapping_index - 1];
            yaml_node_pair_t pair;
            pair.key = ki;
            pair.value = 0;
            if (!PUSH(parser, mapping->data.mapping.pairs, pair))
                return 0;
        }
        parser->state = YAML_PARSE_BLOCK_MAPPING_VALUE_STATE;
        return 1;

    fallback_value:
        /* We consumed KEY + scalar_key + VALUE, but value isn't a plain scalar.
         * Create the key node and set state for value node processing. */
        {
            if (!STACK_LIMIT(parser, document->nodes, INT_MAX-1))
                goto error_free_key;
            if (!STACK_PUSH_RESERVE(parser, document->nodes))
                goto error_free_key;

            int ki = loader_create_plain_scalar(document,
                    key_value, key_length, key_start_mark, key_end_mark);
            if (!ki) goto error_free_key;
            key_value = NULL;  /* helper freed it */

            /* Add partial pair (key only) to mapping. */
            yaml_node_t *mapping = &document->nodes.start[mapping_index - 1];
            yaml_node_pair_t pair;
            pair.key = ki;
            pair.value = 0;
            if (!PUSH(parser, mapping->data.mapping.pairs, pair))
                return 0;
        }
        /* The value token is consumed. Parser should expect a value node. */
        if (!PUSH(parser, parser->states,
                    YAML_PARSE_BLOCK_MAPPING_KEY_STATE))
            return 0;
        parser->state = YAML_PARSE_BLOCK_NODE_OR_INDENTLESS_SEQUENCE_STATE;
        return 1;

    error_free_key:
        /* The key scalar was skipped (removed from queue) but its string
         * was not yet transferred to the arena. Free it to prevent leak. */
        yaml_free_internal(key_value);
        return 0;
    }

    /* If we get here, the token is BLOCK_END (or something else).
     * Set parser state for the normal event loop to handle MAPPING_END. */
    parser->state = YAML_PARSE_BLOCK_MAPPING_KEY_STATE;
    return 1;
}

/*
 * Batch-load plain scalar items directly from the token queue for block
 * sequences. Bypasses the parser state machine for the common case of
 * block sequences containing only plain scalar items without anchors or tags.
 * Falls back to the normal event loop for anything else.
 *
 * The parser must be in BLOCK_SEQUENCE_FIRST_ENTRY_STATE or
 * BLOCK_SEQUENCE_ENTRY_STATE when this is called.
 */

static int
yaml_parser_load_sequence_items_batch(yaml_parser_t *parser,
        struct loader_ctx *ctx)
{
    yaml_token_t *token;
    yaml_document_t *document = parser->document;
    int sequence_index;

    sequence_index = *((*ctx).top - 1);

    /* On first entry, consume the BLOCK-SEQUENCE-START token. */
    if (parser->state == YAML_PARSE_BLOCK_SEQUENCE_FIRST_ENTRY_STATE) {
        token = LOADER_PEEK_TOKEN(parser);
        if (!token) return 0;
        if (!PUSH(parser, parser->marks, token->start_mark))
            return 0;
        LOADER_SKIP_TOKEN(parser);
    }

    for (;;) {
        token = LOADER_PEEK_TOKEN(parser);
        if (!token) return 0;

        if (token->type != YAML_BLOCK_ENTRY_TOKEN)
            break;

        /* Skip BLOCK_ENTRY, peek at the item. */
        LOADER_SKIP_TOKEN(parser);
        token = LOADER_PEEK_TOKEN(parser);
        if (!token) return 0;

        /* Must be a plain scalar -- otherwise fall back. */
        if (token->type != YAML_SCALAR_TOKEN ||
                token->data.scalar.style != YAML_PLAIN_SCALAR_STYLE)
            goto fallback;

        /* Create scalar node directly. */
        if (!STACK_LIMIT(parser, document->nodes, INT_MAX-1)) return 0;
        if (!STACK_PUSH_RESERVE(parser, document->nodes)) return 0;

        /* Create scalar node via helper. On success, helper frees the
         * value string; NULL the token's copy to prevent double-free. */
        int item_index = loader_create_plain_scalar(document,
                token->data.scalar.value, token->data.scalar.length,
                token->start_mark, token->end_mark);
        if (!item_index) return 0;
        token->data.scalar.value = NULL;  /* helper freed it */

        /* Add item to the sequence node. */
        {
            yaml_node_t *seq = &document->nodes.start[sequence_index - 1];
            if (!STACK_LIMIT(parser, seq->data.sequence.items, INT_MAX-1))
                return 0;
            if (!PUSH(parser, seq->data.sequence.items, item_index))
                return 0;
        }

        LOADER_SKIP_TOKEN(parser);
        continue;

    fallback:
        /* We consumed BLOCK_ENTRY but the item isn't a plain scalar.
         * Set parser state so normal event loop processes the item. */
        if (token->type != YAML_BLOCK_ENTRY_TOKEN &&
                token->type != YAML_BLOCK_END_TOKEN) {
            if (!PUSH(parser, parser->states,
                        YAML_PARSE_BLOCK_SEQUENCE_ENTRY_STATE))
                return 0;
            parser->state = YAML_PARSE_BLOCK_NODE_OR_INDENTLESS_SEQUENCE_STATE;
            return 1;
        }

        /* Empty item (next BLOCK_ENTRY or BLOCK_END follows immediately).
         * Create an empty scalar node for the item. */
        if (!STACK_LIMIT(parser, document->nodes, INT_MAX-1)) return 0;
        if (!STACK_PUSH_RESERVE(parser, document->nodes)) return 0;

        {
            yaml_char_t *empty_val = yaml_arena_alloc(document, 1);
            if (!empty_val) return 0;
            empty_val[0] = '\0';

            yaml_node_t *node = document->nodes.top;
            node->type = YAML_SCALAR_NODE;
            node->tag = (yaml_char_t *)yaml_interned_str_tag;
            node->data.scalar.value = empty_val;
            node->data.scalar.length = 0;
            node->data.scalar.style = YAML_PLAIN_SCALAR_STYLE;
            node->start_mark = token->start_mark;
            node->end_mark = token->start_mark;
            STACK_PUSH_COMMIT(document->nodes);
        }

        {
            int item_index = document->nodes.top - document->nodes.start;
            yaml_node_t *seq = &document->nodes.start[sequence_index - 1];
            if (!STACK_LIMIT(parser, seq->data.sequence.items, INT_MAX-1))
                return 0;
            if (!PUSH(parser, seq->data.sequence.items, item_index))
                return 0;
        }
        continue;
    }

    /* BLOCK_END (or unexpected token) -- let normal event loop handle it. */
    parser->state = YAML_PARSE_BLOCK_SEQUENCE_ENTRY_STATE;
    return 1;
}

static int
yaml_parser_load_mapping_end(yaml_parser_t *parser, yaml_event_t *event,
        struct loader_ctx *ctx)
{
    int index;

    assert(((*ctx).top - (*ctx).start) > 0);

    index = *((*ctx).top - 1);
    assert(parser->document->nodes.start[index-1].type == YAML_MAPPING_NODE);
    parser->document->nodes.start[index-1].end_mark = event->end_mark;

    (void)POP(parser, *ctx);

    return 1;
}