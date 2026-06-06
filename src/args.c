//
// Created by erikd on 06.06.26.
//

#include "args.h"

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
        .max_steps = 0,
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

            // Might add a check if code was already given with -e or file?

            a.flags |= CODE;
            a.code = argv[i];
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

            char* end;
            const long val = strtol(argv[i], &end, 10);
            if (*end != '\0' || val < 0) {
                fprintf(stderr, "Error: --max-steps requires a non-negative integer\n");
                exit(EXIT_FAILURE);
            }

            a.flags |= MAX_STEPS;
            a.max_steps = (size_t)val;
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
