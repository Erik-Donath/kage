//
// Created by erikd on 06.06.26.
//

#pragma once
#include <stddef.h>

typedef enum {
    HELP        = 0x01,
    VERSION     = 0x02,
    CODE        = 0x04,
    VERBOSE     = 0x08,
    MAX_STEPS   = 0x10,
    INPUT       = 0x20,
    OUTPUT      = 0x40,
} args_flags;

typedef struct {
    args_flags flags;
    size_t max_steps;

    union {
        const char* file;
        const char* code;
    };
    const char* input;
    const char* output;
} args;

args parse_args(int argc, const char* argv[]);
