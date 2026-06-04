//
// Created by erikd on 04.06.26.
//

#pragma once
#include <stdint.h>
#include <stddef.h>

typedef enum {
    STORE_LIT     = 1,
    STORE_CPY     = 2,
    STORE_EXP     = 3,

    FLOW_LAB      = 101,
    FLOW_JMP      = 4,
    FLOW_COND_JMP = 5,

    IO_PRINT      = 6,
    IO_READ       = 7,
    IO_DEC_PRINT  = 8,

    NUM           = 102,
    OP            = 103,
    LABEL         = 104,

    EOF_TOKEN     = 105,
    NONE          = 0,
} token_type;
// All Token types less than 100 result in an instruction inside the ir. All Tokens above and equals 100 will not result in an instruction.

typedef struct {
    token_type type;
    union {
        int32_t num;
        uint32_t reg;
        const char* str;
        char op;
    };
    int line;
    int column;
} token;

typedef struct {
    token* tokens;
    size_t len, cap;
} token_vec;

void token_vec_init(token_vec* vec);
void token_vec_free(token_vec* vec);
void token_vec_push(token_vec* vec, token token);
void token_dump(const token_vec* vec);
