/*
 * Fuzz harness for the scanner API (yaml_parser_scan).
 * Build: clang -fsanitize=fuzzer,address -I../include -O1 -g \
 *        fuzz_scan.c ../src/*.c -o fuzz_scan
 */

#include <yaml.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const unsigned char *data, size_t size)
{
    yaml_parser_t parser;
    yaml_token_t token;

    if (!yaml_parser_initialize(&parser))
        return 0;

    yaml_parser_set_input_string(&parser, data, size);

    while (1) {
        if (!yaml_parser_scan(&parser, &token)) {
            break;
        }
        int done = (token.type == YAML_STREAM_END_TOKEN);
        yaml_token_delete(&token);
        if (done)
            break;
    }

    yaml_parser_delete(&parser);
    return 0;
}
