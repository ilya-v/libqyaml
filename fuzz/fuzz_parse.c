/*
 * Fuzz harness for the event-based parser API (yaml_parser_parse).
 * Build: clang -fsanitize=fuzzer,address -I../include -O1 -g \
 *        fuzz_parse.c ../src/*.c -o fuzz_parse
 */

#include <yaml.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const unsigned char *data, size_t size)
{
    yaml_parser_t parser;
    yaml_event_t event;

    if (!yaml_parser_initialize(&parser))
        return 0;

    yaml_parser_set_input_string(&parser, data, size);

    while (1) {
        if (!yaml_parser_parse(&parser, &event)) {
            break;
        }
        int done = (event.type == YAML_STREAM_END_EVENT);
        yaml_event_delete(&event);
        if (done)
            break;
    }

    yaml_parser_delete(&parser);
    return 0;
}
