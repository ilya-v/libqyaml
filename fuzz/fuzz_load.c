/*
 * Fuzz harness for the document-based API (yaml_parser_load).
 * Build: clang -fsanitize=fuzzer,address -I../include -O1 -g \
 *        fuzz_load.c ../src/*.c -o fuzz_load
 */

#include <yaml.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const unsigned char *data, size_t size)
{
    yaml_parser_t parser;
    yaml_document_t document;

    if (!yaml_parser_initialize(&parser))
        return 0;

    yaml_parser_set_input_string(&parser, data, size);

    while (1) {
        if (!yaml_parser_load(&parser, &document)) {
            yaml_document_delete(&document);
            break;
        }

        /* Check if we reached end of stream (empty document) */
        if (!yaml_document_get_root_node(&document)) {
            yaml_document_delete(&document);
            break;
        }

        yaml_document_delete(&document);
    }

    yaml_parser_delete(&parser);
    return 0;
}
