//
// Created by erikd on 06.06.26.
//

#include "args.h"

#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

args parse_args(const int argc, const char* argv[]) {
    if (argc < 2) {
        return (args){
            .flags = HELP,
        };
    }

    args a = (args){
        .flags = 0x0,
        .file = NULL,
        .code = NULL,
        .max_steps = 0,
        .input = NULL,
        .output = NULL,
    };

    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];

        if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
            a.flags |= HELP;
            continue;
        }
        if (strcmp(arg, "-v") == 0 || strcmp(arg, "--version") == 0) {
            a.flags |= VERSION;
            continue;
        }
        if (strcmp(arg, "-e") == 0) {
            i++;
            if (i >= argc) {
                fprintf(stderr, "Error: -e requires an argument\n");
                exit(EXIT_FAILURE);
            }

            if ((a.flags & CODE) || a.file != NULL) {
                fprintf(stderr, "Error: source code or file was specified more than once\n");
                exit(EXIT_FAILURE);
            }

            a.flags |= CODE;
            a.code = argv[i];
            continue;
        }
        if (strcmp(arg, "-i") == 0 || strcmp(arg, "--input") == 0) {
            i++;
            if (i >= argc) {
                fprintf(stderr, "Error: -i requires an argument\n");
                exit(EXIT_FAILURE);
            }

            if (a.flags & INPUT) {
                fprintf(stderr, "Error: input file was specified more than once\n");
                exit(EXIT_FAILURE);
            }
            a.flags |= INPUT;
            a.input = argv[i];
            continue;
        }
        if (strcmp(arg, "-o") == 0 || strcmp(arg, "--output") == 0) {
            i++;
            if (i >= argc) {
                fprintf(stderr, "Error: -o requires an argument\n");
                exit(EXIT_FAILURE);
            }

            if (a.flags & OUTPUT) {
                fprintf(stderr, "Error: output file was specified more than once\n");
                exit(EXIT_FAILURE);
            }
            a.flags |= OUTPUT;
            a.output = argv[i];
            continue;
        }
        if (strcmp(arg, "--verbose") == 0) {
            a.flags |= VERBOSE;
            continue;
        }
        if (strcmp(arg, "--max-steps") == 0) {
            i++;
            if (i >= argc) {
                fprintf(stderr, "Error: --max-steps requires an argument\n");
                exit(EXIT_FAILURE);
            }
            if (a.flags & MAX_STEPS) {
                fprintf(stderr, "Error: --max-steps was specified more than once\n");
                exit(EXIT_FAILURE);
            }

            const char* value = argv[i];
            bool digits_only = value[0] != '\0';
            for (size_t j = 0; value[j] != '\0'; ++j) {
                if (value[j] < '0' || value[j] > '9') {
                    digits_only = false;
                    break;
                }
            }

            errno = 0;
            char* end;
            const uintmax_t parsed = strtoumax(value, &end, 10);
            if (!digits_only || errno == ERANGE || *end != '\0' || parsed > SIZE_MAX) {
                fprintf(stderr, "Error: --max-steps requires a non-negative integer\n");
                exit(EXIT_FAILURE);
            }

            a.flags |= MAX_STEPS;
            a.max_steps = (size_t)parsed;
            continue;
        }
        if (arg[0] == '-') {
            fprintf(stderr, "Error: unknown flag '%s'\n", arg);
            exit(EXIT_FAILURE);
        }

        if (a.flags & CODE || a.file != NULL) {
            fprintf(stderr, "Error: unexpected argument '%s'\n", arg);
            exit(EXIT_FAILURE);
        }

        a.file = arg;
    }

    return a;
}
