//
// Created by erikd on 04.06.26.
// Refactored by erikd on 29.10.26 and 31.10.26
//

#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    TOK_ERROR = 0,

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

    TOK_EOF, // The token list needs to ALWAYS end with TOK_EOF.
    LEXER_TOKEN_KIND_COUNT,
} lexer_token_kind;

typedef enum {
    TOK_ERR_INVALID = 0, // This should never be emitted

    TOK_ERR_WORD_TOO_LONG,
    TOK_ERR_INT32_TOO_SMALL,
    TOK_ERR_INT32_TOO_LARGE,
    TOK_ERR_UNEXPECTED_CHAR,

    TOK_ERR_KIND_COUNT,
} lexer_token_error_kind;

typedef struct {
    lexer_token_kind kind;
    uint32_t line, column;
    const char* text;               // lexeme in source, NOT NUL-terminated
    uint32_t len;                   // len of lexeme

    union {
        int32_t     num;            // TOK_NUM
        char        op;             // TOK_OP
        lexer_token_error_kind err; // TOK_ERROR
    };
} lexer_token;

typedef struct {
    lexer_token* tokens;
    size_t token_count;
    size_t error_count;
    bool out_of_memory;
    const char* source;
} lexer_result;

// Note that the lexer_result contains references to the given source. Do not free the source before freeing the lexer result.
lexer_result lexer_tokenize(const char* source);
void lexer_free(lexer_result* lr);

const char* lexer_token_kind_name(lexer_token_kind kind);
const char* lexer_token_err_kind_name_simple(lexer_token_error_kind kind);
void lexer_dump_tokens(const lexer_result* lr);
