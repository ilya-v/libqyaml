#include "test_helper.h"

/* Test stream event initialization */
static void test_stream_events(void) {
    yaml_event_t event;

    /* Stream start */
    ASSERT(yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING),
           "stream start init");
    ASSERT_EQ_INT(event.type, YAML_STREAM_START_EVENT, "stream start type");
    ASSERT_EQ_INT(event.data.stream_start.encoding, YAML_UTF8_ENCODING,
                 "stream start encoding");
    yaml_event_delete(&event);

    ASSERT(yaml_stream_start_event_initialize(&event, YAML_ANY_ENCODING),
           "stream start any");
    ASSERT_EQ_INT(event.data.stream_start.encoding, YAML_ANY_ENCODING,
                 "stream start any encoding");
    yaml_event_delete(&event);

    /* Stream end */
    ASSERT(yaml_stream_end_event_initialize(&event), "stream end init");
    ASSERT_EQ_INT(event.type, YAML_STREAM_END_EVENT, "stream end type");
    yaml_event_delete(&event);
}

/* Test document event initialization */
static void test_document_events(void) {
    yaml_event_t event;

    /* Document start - implicit */
    ASSERT(yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1),
           "doc start implicit");
    ASSERT_EQ_INT(event.type, YAML_DOCUMENT_START_EVENT, "doc start type");
    ASSERT(event.data.document_start.implicit, "doc start: is implicit");
    ASSERT_NULL(event.data.document_start.version_directive, "doc start: no version");
    yaml_event_delete(&event);

    /* Document start - explicit with version */
    yaml_version_directive_t version = {1, 1};
    ASSERT(yaml_document_start_event_initialize(&event, &version, NULL, NULL, 0),
           "doc start explicit");
    ASSERT(!event.data.document_start.implicit, "doc start: not implicit");
    ASSERT_NOT_NULL(event.data.document_start.version_directive, "doc start: has version");
    ASSERT_EQ_INT(event.data.document_start.version_directive->major, 1, "version major");
    ASSERT_EQ_INT(event.data.document_start.version_directive->minor, 1, "version minor");
    yaml_event_delete(&event);

    /* Document start with tag directives */
    yaml_tag_directive_t tags[2] = {
        {(yaml_char_t *)"!", (yaml_char_t *)"!"},
        {(yaml_char_t *)"!!", (yaml_char_t *)"tag:yaml.org,2002:"}
    };
    ASSERT(yaml_document_start_event_initialize(&event, NULL, tags, tags + 2, 0),
           "doc start with tags");
    yaml_event_delete(&event);

    /* Document end - implicit */
    ASSERT(yaml_document_end_event_initialize(&event, 1), "doc end implicit");
    ASSERT_EQ_INT(event.type, YAML_DOCUMENT_END_EVENT, "doc end type");
    ASSERT(event.data.document_end.implicit, "doc end: is implicit");
    yaml_event_delete(&event);

    /* Document end - explicit */
    ASSERT(yaml_document_end_event_initialize(&event, 0), "doc end explicit");
    ASSERT(!event.data.document_end.implicit, "doc end: not implicit");
    yaml_event_delete(&event);
}

/* Test scalar event initialization */
static void test_scalar_event(void) {
    yaml_event_t event;

    /* Simple plain scalar */
    ASSERT(yaml_scalar_event_initialize(&event, NULL, NULL,
           (const yaml_char_t *)"hello", 5, 1, 0, YAML_PLAIN_SCALAR_STYLE),
           "scalar init plain");
    ASSERT_EQ_INT(event.type, YAML_SCALAR_EVENT, "scalar type");
    ASSERT_EQ_STR(event.data.scalar.value, "hello", "scalar value");
    ASSERT_EQ_INT((int)event.data.scalar.length, 5, "scalar length");
    ASSERT_EQ_INT(event.data.scalar.style, YAML_PLAIN_SCALAR_STYLE, "scalar style");
    ASSERT(event.data.scalar.plain_implicit, "scalar: plain implicit");
    ASSERT(!event.data.scalar.quoted_implicit, "scalar: not quoted implicit");
    ASSERT_NULL(event.data.scalar.anchor, "scalar: no anchor");
    ASSERT_NULL(event.data.scalar.tag, "scalar: no tag");
    yaml_event_delete(&event);

    /* Scalar with anchor and tag */
    ASSERT(yaml_scalar_event_initialize(&event,
           (const yaml_char_t *)"myanchor",
           (const yaml_char_t *)"tag:yaml.org,2002:str",
           (const yaml_char_t *)"world", 5, 0, 1, YAML_DOUBLE_QUOTED_SCALAR_STYLE),
           "scalar init anchored");
    ASSERT_EQ_STR(event.data.scalar.anchor, "myanchor", "scalar: anchor");
    ASSERT_EQ_STR(event.data.scalar.tag, "tag:yaml.org,2002:str", "scalar: tag");
    ASSERT_EQ_INT(event.data.scalar.style, YAML_DOUBLE_QUOTED_SCALAR_STYLE, "scalar: quoted");
    yaml_event_delete(&event);

    /* Scalar with length -1 (auto-detect) */
    ASSERT(yaml_scalar_event_initialize(&event, NULL, NULL,
           (const yaml_char_t *)"test", -1, 1, 0, YAML_PLAIN_SCALAR_STYLE),
           "scalar init auto length");
    ASSERT_EQ_INT((int)event.data.scalar.length, 4, "scalar: auto length=4");
    yaml_event_delete(&event);

    /* Empty scalar */
    ASSERT(yaml_scalar_event_initialize(&event, NULL, NULL,
           (const yaml_char_t *)"", 0, 1, 0, YAML_PLAIN_SCALAR_STYLE),
           "scalar init empty");
    ASSERT_EQ_INT((int)event.data.scalar.length, 0, "scalar: empty length=0");
    yaml_event_delete(&event);
}

/* Test alias event initialization */
static void test_alias_event(void) {
    yaml_event_t event;

    ASSERT(yaml_alias_event_initialize(&event, (const yaml_char_t *)"myanchor"),
           "alias init");
    ASSERT_EQ_INT(event.type, YAML_ALIAS_EVENT, "alias type");
    ASSERT_EQ_STR(event.data.alias.anchor, "myanchor", "alias anchor");
    yaml_event_delete(&event);
}

/* Test sequence event initialization */
static void test_sequence_events(void) {
    yaml_event_t event;

    /* Sequence start - block */
    ASSERT(yaml_sequence_start_event_initialize(&event, NULL, NULL, 1,
           YAML_BLOCK_SEQUENCE_STYLE), "seq start block");
    ASSERT_EQ_INT(event.type, YAML_SEQUENCE_START_EVENT, "seq start type");
    ASSERT(event.data.sequence_start.implicit, "seq start: implicit");
    ASSERT_EQ_INT(event.data.sequence_start.style, YAML_BLOCK_SEQUENCE_STYLE,
                 "seq start: block style");
    yaml_event_delete(&event);

    /* Sequence start - flow with anchor and tag */
    ASSERT(yaml_sequence_start_event_initialize(&event,
           (const yaml_char_t *)"seqanchor",
           (const yaml_char_t *)"tag:yaml.org,2002:seq",
           0, YAML_FLOW_SEQUENCE_STYLE), "seq start flow");
    ASSERT_EQ_STR(event.data.sequence_start.anchor, "seqanchor", "seq: anchor");
    ASSERT_EQ_STR(event.data.sequence_start.tag, "tag:yaml.org,2002:seq", "seq: tag");
    ASSERT(!event.data.sequence_start.implicit, "seq: not implicit");
    yaml_event_delete(&event);

    /* Sequence end */
    ASSERT(yaml_sequence_end_event_initialize(&event), "seq end");
    ASSERT_EQ_INT(event.type, YAML_SEQUENCE_END_EVENT, "seq end type");
    yaml_event_delete(&event);
}

/* Test mapping event initialization */
static void test_mapping_events(void) {
    yaml_event_t event;

    /* Mapping start - block */
    ASSERT(yaml_mapping_start_event_initialize(&event, NULL, NULL, 1,
           YAML_BLOCK_MAPPING_STYLE), "map start block");
    ASSERT_EQ_INT(event.type, YAML_MAPPING_START_EVENT, "map start type");
    ASSERT(event.data.mapping_start.implicit, "map start: implicit");
    ASSERT_EQ_INT(event.data.mapping_start.style, YAML_BLOCK_MAPPING_STYLE,
                 "map start: block style");
    yaml_event_delete(&event);

    /* Mapping start - flow with anchor */
    ASSERT(yaml_mapping_start_event_initialize(&event,
           (const yaml_char_t *)"mapanchor", NULL, 0,
           YAML_FLOW_MAPPING_STYLE), "map start flow");
    ASSERT_EQ_STR(event.data.mapping_start.anchor, "mapanchor", "map: anchor");
    ASSERT(!event.data.mapping_start.implicit, "map: not implicit");
    yaml_event_delete(&event);

    /* Mapping end */
    ASSERT(yaml_mapping_end_event_initialize(&event), "map end");
    ASSERT_EQ_INT(event.type, YAML_MAPPING_END_EVENT, "map end type");
    yaml_event_delete(&event);
}

/* Test all encoding values */
static void test_encodings(void) {
    yaml_event_t event;

    yaml_encoding_t encodings[] = {
        YAML_ANY_ENCODING, YAML_UTF8_ENCODING,
        YAML_UTF16LE_ENCODING, YAML_UTF16BE_ENCODING
    };

    for (int i = 0; i < 4; i++) {
        ASSERT(yaml_stream_start_event_initialize(&event, encodings[i]),
               "encoding init");
        ASSERT_EQ_INT(event.data.stream_start.encoding, encodings[i],
                     "encoding value matches");
        yaml_event_delete(&event);
    }
}

/* Test all scalar styles */
static void test_scalar_styles(void) {
    yaml_event_t event;

    yaml_scalar_style_t styles[] = {
        YAML_ANY_SCALAR_STYLE, YAML_PLAIN_SCALAR_STYLE,
        YAML_SINGLE_QUOTED_SCALAR_STYLE, YAML_DOUBLE_QUOTED_SCALAR_STYLE,
        YAML_LITERAL_SCALAR_STYLE, YAML_FOLDED_SCALAR_STYLE
    };

    for (int i = 0; i < 6; i++) {
        ASSERT(yaml_scalar_event_initialize(&event, NULL, NULL,
               (const yaml_char_t *)"test", 4, 1, 1, styles[i]),
               "scalar style init");
        ASSERT_EQ_INT(event.data.scalar.style, styles[i], "scalar style matches");
        yaml_event_delete(&event);
    }
}

int main(void) {
    TEST_SUITE_BEGIN("Event API");

    test_stream_events();
    test_document_events();
    test_scalar_event();
    test_alias_event();
    test_sequence_events();
    test_mapping_events();
    test_encodings();
    test_scalar_styles();

    TEST_SUITE_END();
}
