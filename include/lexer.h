//
// Created by erikd on 04.06.26.
// Refactored by erikd on 29.10.26
//

#pragma once
#include <stddef.h>
#include <stdint.h>

typedef enum {
    TOK_INVALID = 0,

    TOK_STORE_LIT,
    TOK_STORE_COPY,
    TOK_STORE_ARITH,
    TOK_FLOW_LABEL,
    TOK_FLOW_JUMP,
    TOK_FLOW_CJUMP,
    TOK_IO_PUTC,
    TOK_IO_GETC,
    TOK_IO_PUTI,

    TOK_NUM,
    TOK_LABEL,
    TOK_OP,

    TOK_EOF,
    TOK_ERROR,
} lexer_token_kind;

#define LEXER_TOKEN_KIND_COUNT 15
// Note: We might move away from Error as a Token scheme in the future

typedef struct {
    lexer_token_kind kind;
    uint32_t line, column;
    const char* text;      // lexeme in source, NOT NUL-terminated
    uint32_t len;

    union {
        int32_t     num;   // TOK_NUM
        char        op;    // TOK_OP
        const char* error; // TOK_ERROR, static string
    };
} lexer_token;

typedef struct {
    lexer_token* tokens;
    size_t token_count;
    size_t error_count;
} lexer_result;

// Note that the lexer_result contains references to the given source. Do not free the source before freeing the lexer result.
lexer_result lexer_tokenize(const char* source);
void lexer_free(lexer_result* lr);

const char* lexer_token_kind_name(lexer_token_kind kind);
void lexer_dump_tokens(const lexer_result* lr);
