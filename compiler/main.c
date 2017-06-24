#include <stdio.h>

#include "parser.h"

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("No input file specified.\n");
        return 1;
    }
    parser_t parser;
    if (!init_parser(&parser, argv[1])) {
        return 2;
    }

    parse_program(&parser);
    printf("Done parsing\n");

    release_parser(&parser);
    return 0;
}
