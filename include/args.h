//
// Created by erikd on 06.06.26.
//

#pragma once
#include <stddef.h>

typedef enum {
    HELP = 0x1,
    VERSION = 0x2,
    CODE = 0x4,
    VERBOSE = 0x8,
    MAX_STEPS = 0x10,
} args_flags;

typedef struct {
    args_flags flags;
    size_t max_steps;

    union {
        const char* file;
        const char* code;
    };
} args;

args parse_args(int argc, const char* argv[]);
