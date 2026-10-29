//
// Created by erikd on 04.06.26.
//

#include "lexer.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *current;

    int line;
    int column;

    token_vec tokens;
    diagnostic_list* diagnostics;
    bool failed;
} lexer_state;

#define CURRENT(l) (*(l)->current)
#define PEEK(l) ((l)->current[1])
#define AT_END(l) (CURRENT(l) == '\0')

#define CONSUME(l) lexer_consume(l)

static bool is_ascii_alpha(const char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

static bool is_ascii_digit(const char c) {
    return c >= '0' && c <= '9';
}

static bool is_space(const char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

static void push_token(lexer_state *l, const token t) {
    if (!token_vec_push(&l->tokens, t)) {
        l->failed = true;
        l->diagnostics->allocation_failed = true;
    }
}

static const struct {
    token_type type;
    const char *keyword;
} keywords[] = {
    { STORE_LIT,      "yami"   },
    { STORE_CPY,      "Yami"   },
    { STORE_EXP,      "YAMI"   },

    { FLOW_LAB,       "hikari" },
    { FLOW_JMP,       "Hikari" },
    { FLOW_COND_JMP,  "HIKARI" },

    { IO_PRINT,       "rei"    },
    { IO_READ,        "Rei"    },
    { IO_DEC_PRINT,   "REI"    }
};

#define KEYWORD_COUNT (sizeof(keywords) / sizeof(keywords[0]))

static void lexer_consume(lexer_state *l) {
    if (CURRENT(l) == '\n') {
        l->line++;
        l->column = 1;
    }
    else
        l->column++;
    l->current++;
}

static void lexer_error(lexer_state* l, const int line, const int column,
                        const char* message, const char* help) {
    if (!diagnostics_add(l->diagnostics, DIAGNOSTIC_LEXER,
                         line, column, message, help))
        l->failed = true;
}

#define MAX_SCAN_WORD_LEN 32
static void scan_word(lexer_state *l) {
    const int start_line = l->line;
    const int start_col = l->column;
    char word[MAX_SCAN_WORD_LEN + 1];
    size_t len = 0;

    bool too_long = false;
    while (is_ascii_alpha(CURRENT(l))) {
        if (len < MAX_SCAN_WORD_LEN)
            word[len++] = CURRENT(l);
        else
            too_long = true;
        CONSUME(l);
    }

    word[len] = '\0';
    if (too_long) {
        lexer_error(l, start_line, start_col,
                    "word exceeds maximum length of 31 characters",
                    "Shorten this label to 31 ASCII letters or fewer.");
        push_token(l, (token){
            .type = INVALID,
            .line = start_line,
            .column = start_col,
        });
        return;
    }
    for (size_t i = 0; i < KEYWORD_COUNT; ++i) {
        if (strcmp(word, keywords[i].keyword) == 0) {
            const token t = (token){
                .type = keywords[i].type,
                .line = start_line,
                .column = start_col,
            };
            push_token(l, t);
            return;
        }
    }

    char *copy = malloc(len + 1);
    if (!copy) {
        l->failed = true;
        l->diagnostics->allocation_failed = true;
        return;
    }
    memcpy(copy, word, len + 1);

    const token t = (token){
        .type = LABEL,
        .str = copy,
        .line = start_line,
        .column = start_col,
    };
    push_token(l, t);
}

static void scan_number(lexer_state *l) {
    const int start_line = l->line;
    const int start_col = l->column;
    const bool negative = CURRENT(l) == '-';
    uint32_t magnitude = 0;
    const uint32_t limit = negative ? (uint32_t)INT32_MAX + 1U : (uint32_t)INT32_MAX;

    if (negative)
        CONSUME(l);

    bool out_of_range = false;
    while (is_ascii_digit(CURRENT(l))) {
        const uint32_t digit = (uint32_t)(CURRENT(l) - '0');
        if (!out_of_range) {
            if (magnitude > (limit - digit) / 10U)
                out_of_range = true;
            else
                magnitude = magnitude * 10U + digit;
        }
        CONSUME(l);
    }

    if (out_of_range) {
        lexer_error(l, start_line, start_col,
                    "integer literal is outside the int32 range",
                    "Use a value between -2147483648 and 2147483647.");
        push_token(l, (token){
            .type = INVALID,
            .line = start_line,
            .column = start_col,
        });
        return;
    }

    const int32_t value = negative
        ? (magnitude == (uint32_t)INT32_MAX + 1U
            ? INT32_MIN
            : -(int32_t)magnitude)
        : (int32_t)magnitude;

    const token t = (token){
        .type = NUM,
        .num = value,
        .line = start_line,
        .column = start_col,
    };
    push_token(l, t);
}

static void scan_comment(lexer_state *l) {
    while (!AT_END(l) && CURRENT(l) != '\n')
        CONSUME(l);
}

static void scan_operator(lexer_state *l) {
    const token t = (token){
        .type = OP,
        .op = CURRENT(l),
        .line = l->line,
        .column = l->column
    };
    CONSUME(l);
    push_token(l, t);
}

token_vec lexer(const char* file_content, diagnostic_list* diagnostics) {
    lexer_state l = {
        .current = file_content,
        .line = 1,
        .column = 1,
        .diagnostics = diagnostics,
        .failed = false,
    };
    if (!token_vec_init(&l.tokens)) {
        diagnostics->allocation_failed = true;
        return l.tokens;
    }

    while (!AT_END(&l) && !l.failed) {
        const char c = CURRENT(&l);

        if (is_space(c)) {
            CONSUME(&l);
            continue;
        }
        if (c == '#') {
            scan_comment(&l);
            continue;
        }
        if (is_ascii_alpha(c)) {
            scan_word(&l);
            continue;
        }
        if (is_ascii_digit(c) || (c == '-' && is_ascii_digit(PEEK(&l)))) {
            scan_number(&l);
            continue;
        }
        if (c== '+' || c == '-' || c == '*' || c == '/' || c == '%') {
            scan_operator(&l);
            continue;
        }

        lexer_error(&l, l.line, l.column, "Unexpected character",
                    "Remove this character or replace it with a valid Kage token.");
        push_token(&l, (token){
            .type = INVALID,
            .line = l.line,
            .column = l.column,
        });
        CONSUME(&l);
    }

    if (l.failed)
        return l.tokens;

    const token eof = (token){
        .type = EOF_TOKEN,
        .line = l.line,
        .column = l.column
    };
    push_token(&l, eof);
    return l.tokens;
}
