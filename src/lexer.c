//
// Created by erikd on 04.06.26.
//

#include "lexer.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *start;
    const char *current;

    int line;
    int column;

    token_vec tokens;
} lexer_state;

#define CURRENT(l) (*(l)->current)
#define PEEK(l) ((l)->current[1])
#define AT_END(l) (CURRENT(l) == '\0')

#define CONSUME(l) lexer_consume(l)
#define PUSH_TOKEN(l, t) token_vec_push(&(l)->tokens, (t));

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

static void lexer_error(const lexer_state *l, const char *msg) {
    fprintf(stderr,
            "LexerError (%d:%d): %s\n",
            l->line,
            l->column,
            msg);

    exit(EXIT_FAILURE);
}

#define MAX_SCAN_WORD_LEN 32
static void scan_word(lexer_state *l) {
    const int start_line = l->line;
    const int start_col = l->column;
    char word[MAX_SCAN_WORD_LEN];
    size_t len = 0;

    while (isalpha(CURRENT(l))) {
        if (len >= MAX_SCAN_WORD_LEN - 1)
            lexer_error(l, "Keyword to long");

        word[len++] = CURRENT(l);
        CONSUME(l);
    }

    word[len] = '\0';
    for (size_t i = 0; i < KEYWORD_COUNT; ++i) {
        if (strcmp(word, keywords[i].keyword) == 0) {
            const token t = (token){
                .type = keywords[i].type,
                .line = start_line,
                .column = start_col,
            };
            PUSH_TOKEN(l, t);
            return;
        }
    }

    // LABLE
    char *copy = malloc(len + 1);
    if (!copy)
        lexer_error(l, "out of memory");
    memcpy(copy, word, len + 1);

    const token t = (token){
        .type = LABEL,
        .str = copy,
        .line = l->line,
        .column = start_col,
    };
    PUSH_TOKEN(l, t);
}

#define MAX_SCAN_NUMBER_LEN 64
static void scan_number(lexer_state *l) {
    const int start_line = l->line;
    const int start_col = l->column;

    char number[MAX_SCAN_NUMBER_LEN];
    size_t len = 0;

    if (CURRENT(l) == '-') {
        number[len++] = '-';
        CONSUME(l);
    }

    while (isdigit(CURRENT(l))) {
        if (len >= MAX_SCAN_NUMBER_LEN - 1)
            lexer_error(l, "Number too large");

        number[len++] = CURRENT(l);
        CONSUME(l);
    }

    number[len] = '\0';

    const token t = (token){
        .type = NUM,
        .num= (int32_t)strtol(number,NULL,10),
        .line = start_line,
        .column = start_col,
    };
    PUSH_TOKEN(l, t);
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
    PUSH_TOKEN(l, t);
}

token_vec lexer(const char* source) {
    lexer_state l = {
        .start = source,
        .current = source,
        .line = 1,
        .column = 1,
    };
    token_vec_init(&l.tokens);

    while (!AT_END(&l)) {
        const char c = CURRENT(&l);

        if (isspace(c)) {
            CONSUME(&l);
            continue;
        }
        if (c == '#') {
            scan_comment(&l);
            continue;
        }
        if (isalpha(c)) {
            scan_word(&l);
            continue;
        }
        if ( isdigit(c) || (c == '-' && isdigit(PEEK(&l)))) {
            scan_number(&l);
            continue;
        }
        if (c== '+' || c == '-' || c == '*' || c == '/' || c == '%') {
            scan_operator(&l);
            continue;
        }

        lexer_error(&l, "Unexpected character");
    }

    const token eof = (token){
        .type = EOF_TOKEN,
        .line = l.line,
        .column = l.column
    };
    PUSH_TOKEN(&l, eof);

    return l.tokens;
}
