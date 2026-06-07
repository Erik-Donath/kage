//
// Created by erikd on 04.06.26.
//

#include "main.h"
#include "args.h"
#include "version.h"
#include "tokens.h"
#include "lexer.h"
#include "parser.h"
#include "vm.h"
#include <stdlib.h>
#include <stdio.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

const char* read_file(const char* path) {
    if (!path)
        return NULL;

    FILE *file = fopen(path, "r");
    if (!file)
        return NULL;

    fseek(file, 0, SEEK_END);
    const size_t size = (size_t)ftell(file);
    rewind(file);

    char *buffer = malloc(size + 1);
    (void)fread(buffer, 1, size, file);
    buffer[size] = '\0';

    fclose(file);
    return buffer;
}

int main(const int argc, const char *argv[]) {
#ifdef _WIN32
    // Set stdin to binary mode so \r\n in redirected input is not translated.
    // stdout is left in text mode so output is captured correctly by CMake.
    _setmode(_fileno(stdin), _O_BINARY);
#endif

    const args a = parse_args(argc, argv);

    if (a.flags & HELP) {
        printf(
            "Kage " KAGE_VERSION "\n"
            "Usage: kage [options] <file>\n"
            "       kage [options] -e <code>\n"
            "\n"
            "Options:\n"
            "  -h, --help           Displays this help message\n"
            "  -v, --version        Print kage version\n"
            "  -e <code>            Execute given code directly\n"
            "      --verbose        Enable debug output (token dump, ir dump)\n"
            "      --max-steps N    Abort after N instructions (0 = unlimited)\n"
            "\n"
        );
        return EXIT_SUCCESS;
    }

    if (a.flags & VERSION) {
        printf("Kage " KAGE_VERSION "\n");
        return EXIT_SUCCESS;
    }

    const char* code = (a.flags & CODE) ? a.code : read_file(a.file);
    if (!code) {
        fprintf(stderr, "Error: no code was given\n");
        return EXIT_FAILURE;
    }

    token_vec vec = lexer(code);

    if (!(a.flags & CODE))
        free((void*)code);

    if (a.flags & VERBOSE)
        token_dump(&vec);

    ir_arr ir = parse(&vec);
    token_vec_free(&vec);

    if (a.flags & VERBOSE) {
        ir_dump(&ir);
        printf("\n=== RUN ===\n");
    }

    run_vm(&ir, a.max_steps);
    ir_arr_free(&ir);

    if (a.flags & VERBOSE)
        printf("\n\n=== DONE ===\n");

    fflush(stdout);
    return EXIT_SUCCESS;
}
