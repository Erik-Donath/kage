//
// Created by erikd on 04.06.26.
//

#pragma once
#include <stdint.h>
#include <stddef.h>

typedef enum : uint8_t {
    STORE_LIT       = 0b00000100,
    STORE_CPY       = 0b00000101,
    STORE_EXP       = 0b00000110,
    FLOW_LAB        = 0b00001000,
    FLOW_JMP        = 0b00001001,
    FLOW_COND_JMP   = 0b00001010,
    IO_PRINT        = 0b00010000,
    IO_READ         = 0b00010001,
    IO_DEC_PRINT    = 0b00010010,
    NUM             = 0b00100000,
    OP              = 0b00100001,
    LABEL           = 0b00100010,
    EOF_TOKEN       = 0b10000000,
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