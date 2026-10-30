//
// Created by erikd on 04.06.26.
//

#include "args.h"
#include "version.h"
#include "diagnostics.h"
#include "lexer.h"
#include "parser.h"
#include "vm.h"
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static char* read_file(const char* path) {
    if (!path)
        return NULL;

    FILE *file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "Error: cannot open source file %s: %s\n", path, strerror(errno));
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fprintf(stderr, "Error: cannot seek source file %s: %s\n", path, strerror(errno));
        fclose(file);
        return NULL;
    }

    const long file_size = ftell(file);
    if (file_size < 0 || (uintmax_t)file_size > SIZE_MAX - 1) {
        fprintf(stderr, "Error: invalid source file size for %s\n", path);
        fclose(file);
        return NULL;
    }

    const size_t size = (size_t)file_size;
    if (fseek(file, 0, SEEK_SET) != 0) {
        fprintf(stderr, "Error: cannot rewind source file %s: %s\n", path, strerror(errno));
        fclose(file);
        return NULL;
    }

    char *buffer = malloc(size + 1);
    if (!buffer) {
        fprintf(stderr, "Error: out of memory while reading %s\n", path);
        fclose(file);
        return NULL;
    }
    const size_t read = fread(buffer, 1, size, file);
    if (read != size) {
        if (ferror(file))
            fprintf(stderr, "Error: cannot read source file %s: %s\n", path, strerror(errno));
        else
            fprintf(stderr, "Error: source file %s changed while it was being read\n", path);
        free(buffer);
        fclose(file);
        return NULL;
    }
    buffer[size] = '\0';

    if (fclose(file) != 0) {
        fprintf(stderr, "Error: cannot close source file %s: %s\n", path, strerror(errno));
        free(buffer);
        return NULL;
    }
    return buffer;
}

int main(const int argc, const char *argv[]) {
    const args a = parse_args(argc, argv);

    if (a.flags & HELP) {
        printf(
            "Kage " KAGE_VERSION "\n"
            "Usage: kage [options] <file>\n"
            "       kage [options] -e <code>\n"
            "\n"
            "Options:\n"
            "  -h,       --help           Show this help message\n"
            "  -v,       --version        Print kage version\n"
            "  -e <code>                  Execute given code directly\n"
            "  -i <file> --input <file>   Read VM input from a file instead of stdin\n"
            "  -o <file> --output <file>  Write VM output to a file instead of stdout\n"
            "            --verbose        Enable debug output (token dump, ir dump)\n"
            "            --max-steps N    Fail if execution reaches N instructions (0 = unlimited)\n"
            "\n"
        );
        return EXIT_SUCCESS;
    }

    if (a.flags & VERSION) {
        printf("Kage " KAGE_VERSION "\n");
        return EXIT_SUCCESS;
    }

    char* file_code = (a.flags & CODE) ? NULL : read_file(a.file);
    const char* code = (a.flags & CODE) ? a.code : file_code;
    if (!code) {
        if (!a.file && !(a.flags & CODE))
            fprintf(stderr, "Error: no code was given\n");
        return EXIT_FAILURE;
    }

    lexer_result lexer = lexer_tokenize(code);
    //if (lexer.error_count)
    //    return EXIT_FAILURE;

    if (a.flags & VERBOSE)
        lexer_dump_tokens(&lexer);


    diagnostic_list diagnostics;
    diagnostics_init(&diagnostics);

    ir_arr ir = {0};
    const bool parsed = parse(&lexer, &diagnostics, &ir);
    lexer_free(&lexer);

    if (!parsed || diagnostics.len > 0 || diagnostics.allocation_failed) {
        diagnostics_report(&diagnostics);
        diagnostics_free(&diagnostics);
        ir_arr_free(&ir);
        return EXIT_FAILURE;
    }
    diagnostics_free(&diagnostics);

    if (a.flags & VERBOSE) {
        ir_dump(&ir);
        printf("\n=== RUN ===\n");
    }

    if (!(a.flags & CODE))
        free(file_code);

    const int exit_code = run_vm(&ir, &a);
    ir_arr_free(&ir);

    if (exit_code == EXIT_SUCCESS && (a.flags & VERBOSE))
        printf("\n\n=== DONE ===\n");

    return exit_code;
}
