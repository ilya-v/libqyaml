/* Reproduce double-free crash found by fuzz_load */
#include <stdio.h>
#include <string.h>
#include "yaml.h"

int main(void) {
    const char *input = "-:\n%YAML 1.1\n---\niVc:\n%YAML 0";
    size_t len = strlen(input);

    yaml_parser_t parser;
    yaml_document_t document;

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)input, len);

    /* This is what fuzz_load does */
    while (yaml_parser_load(&parser, &document)) {
        int done = !yaml_document_get_root_node(&document);
        yaml_document_delete(&document);
        if (done) break;
    }

    yaml_parser_delete(&parser);
    printf("OK - no crash\n");
    return 0;
}
