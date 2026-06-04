//
// Created by erikd on 04.06.26.
//

#pragma once
#include <stdint.h>
#include <stddef.h>

typedef enum {
    STORE_LIT     = 0x04,
    STORE_CPY     = 0x05,
    STORE_EXP     = 0x06,

    FLOW_LAB      = 0x08,
    FLOW_JMP      = 0x09,
    FLOW_COND_JMP = 0x0A,

    IO_PRINT      = 0x10,
    IO_READ       = 0x11,
    IO_DEC_PRINT  = 0x12,

    NUM           = 0x20,
    OP            = 0x21,
    LABEL         = 0x22,

    EOF_TOKEN     = 0x80,
} token_type;

typedef struct {
    token_type type;
    union {
        int32_t num;
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
