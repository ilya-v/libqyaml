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

/* ==================================================================
 * Extended event API tests
 * ================================================================== */

static void test_scalar_empty_value(void) {
    yaml_event_t event;
    /* API requires non-NULL value (assert enforced), use empty string */
    ASSERT(yaml_scalar_event_initialize(&event, NULL, NULL,
           (const yaml_char_t *)"", 0, 1, 0, YAML_PLAIN_SCALAR_STYLE),
           "empty value init");
    ASSERT_EQ_INT((int)event.data.scalar.length, 0, "empty value: length=0");
    yaml_event_delete(&event);
}

static void test_scalar_binary_content(void) {
    yaml_event_t event;
    const unsigned char binary[] = {0x01, 0x02, 0x03, 0x00, 0x04};
    ASSERT(yaml_scalar_event_initialize(&event, NULL, NULL,
           binary, 5, 0, 0, YAML_DOUBLE_QUOTED_SCALAR_STYLE),
           "binary content init");
    ASSERT_EQ_INT((int)event.data.scalar.length, 5, "binary: length=5");
    yaml_event_delete(&event);
}

static void test_document_start_all_params(void) {
    yaml_event_t event;
    yaml_version_directive_t version = {1, 2};
    yaml_tag_directive_t tags[] = {
        {(yaml_char_t *)"!", (yaml_char_t *)"!"},
        {(yaml_char_t *)"!!", (yaml_char_t *)"tag:yaml.org,2002:"},
        {(yaml_char_t *)"!e!", (yaml_char_t *)"tag:example.com,2000:"}
    };
    ASSERT(yaml_document_start_event_initialize(&event, &version, tags, tags + 3, 0),
           "doc start all params");
    ASSERT_NOT_NULL(event.data.document_start.version_directive, "all: version");
    ASSERT_EQ_INT(event.data.document_start.version_directive->major, 1, "all: major");
    ASSERT_EQ_INT(event.data.document_start.version_directive->minor, 2, "all: minor");
    ASSERT(!event.data.document_start.implicit, "all: explicit");
    yaml_event_delete(&event);
}

static void test_sequence_all_styles(void) {
    yaml_event_t event;
    yaml_sequence_style_t styles[] = {
        YAML_ANY_SEQUENCE_STYLE,
        YAML_BLOCK_SEQUENCE_STYLE,
        YAML_FLOW_SEQUENCE_STYLE
    };
    for (int i = 0; i < 3; i++) {
        ASSERT(yaml_sequence_start_event_initialize(&event, NULL, NULL, 1, styles[i]),
               "seq style init");
        ASSERT_EQ_INT(event.data.sequence_start.style, styles[i], "seq style match");
        yaml_event_delete(&event);
    }
}

static void test_mapping_all_styles(void) {
    yaml_event_t event;
    yaml_mapping_style_t styles[] = {
        YAML_ANY_MAPPING_STYLE,
        YAML_BLOCK_MAPPING_STYLE,
        YAML_FLOW_MAPPING_STYLE
    };
    for (int i = 0; i < 3; i++) {
        ASSERT(yaml_mapping_start_event_initialize(&event, NULL, NULL, 1, styles[i]),
               "map style init");
        ASSERT_EQ_INT(event.data.mapping_start.style, styles[i], "map style match");
        yaml_event_delete(&event);
    }
}

static void test_scalar_long_value(void) {
    yaml_event_t event;
    char value[1024];
    memset(value, 'x', sizeof(value) - 1);
    value[sizeof(value) - 1] = '\0';
    ASSERT(yaml_scalar_event_initialize(&event, NULL, NULL,
           (const yaml_char_t *)value, sizeof(value) - 1, 1, 0,
           YAML_PLAIN_SCALAR_STYLE), "long scalar init");
    ASSERT_EQ_INT((int)event.data.scalar.length, 1023, "long scalar: length");
    yaml_event_delete(&event);
}

static void test_event_delete_all_types(void) {
    yaml_event_t event;

    /* Stream start + delete */
    yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
    yaml_event_delete(&event);

    /* Stream end + delete */
    yaml_stream_end_event_initialize(&event);
    yaml_event_delete(&event);

    /* Document start + delete */
    yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1);
    yaml_event_delete(&event);

    /* Document end + delete */
    yaml_document_end_event_initialize(&event, 1);
    yaml_event_delete(&event);

    /* Scalar + delete */
    yaml_scalar_event_initialize(&event, NULL, NULL,
        (const yaml_char_t *)"test", 4, 1, 0, YAML_PLAIN_SCALAR_STYLE);
    yaml_event_delete(&event);

    /* Alias + delete */
    yaml_alias_event_initialize(&event, (const yaml_char_t *)"anchor");
    yaml_event_delete(&event);

    /* Sequence start + delete */
    yaml_sequence_start_event_initialize(&event, NULL, NULL, 1,
        YAML_BLOCK_SEQUENCE_STYLE);
    yaml_event_delete(&event);

    /* Sequence end + delete */
    yaml_sequence_end_event_initialize(&event);
    yaml_event_delete(&event);

    /* Mapping start + delete */
    yaml_mapping_start_event_initialize(&event, NULL, NULL, 1,
        YAML_BLOCK_MAPPING_STYLE);
    yaml_event_delete(&event);

    /* Mapping end + delete */
    yaml_mapping_end_event_initialize(&event);
    yaml_event_delete(&event);

    ASSERT(1, "event delete all types: no crash");
}

static void test_scalar_with_all_anchortag_combos(void) {
    yaml_event_t event;

    /* No anchor, no tag */
    ASSERT(yaml_scalar_event_initialize(&event, NULL, NULL,
           (const yaml_char_t *)"a", 1, 1, 0, YAML_PLAIN_SCALAR_STYLE), "combo 1");
    ASSERT_NULL(event.data.scalar.anchor, "combo 1: no anchor");
    ASSERT_NULL(event.data.scalar.tag, "combo 1: no tag");
    yaml_event_delete(&event);

    /* Anchor, no tag */
    ASSERT(yaml_scalar_event_initialize(&event,
           (const yaml_char_t *)"anc", NULL,
           (const yaml_char_t *)"b", 1, 0, 0, YAML_PLAIN_SCALAR_STYLE), "combo 2");
    ASSERT_NOT_NULL(event.data.scalar.anchor, "combo 2: has anchor");
    ASSERT_NULL(event.data.scalar.tag, "combo 2: no tag");
    yaml_event_delete(&event);

    /* No anchor, tag */
    ASSERT(yaml_scalar_event_initialize(&event, NULL,
           (const yaml_char_t *)"!!str",
           (const yaml_char_t *)"c", 1, 0, 0, YAML_PLAIN_SCALAR_STYLE), "combo 3");
    ASSERT_NULL(event.data.scalar.anchor, "combo 3: no anchor");
    ASSERT_NOT_NULL(event.data.scalar.tag, "combo 3: has tag");
    yaml_event_delete(&event);

    /* Both anchor and tag */
    ASSERT(yaml_scalar_event_initialize(&event,
           (const yaml_char_t *)"anc2",
           (const yaml_char_t *)"!!int",
           (const yaml_char_t *)"d", 1, 0, 0, YAML_PLAIN_SCALAR_STYLE), "combo 4");
    ASSERT_NOT_NULL(event.data.scalar.anchor, "combo 4: has anchor");
    ASSERT_NOT_NULL(event.data.scalar.tag, "combo 4: has tag");
    yaml_event_delete(&event);
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

    /* Extended */
    test_scalar_empty_value();
    test_scalar_binary_content();
    test_document_start_all_params();
    test_sequence_all_styles();
    test_mapping_all_styles();
    test_scalar_long_value();
    test_event_delete_all_types();
    test_scalar_with_all_anchortag_combos();

    TEST_SUITE_END();
}
