//
// Created by erikd on 04.06.26.
// Refactored by erikd on 29.10.26 and 31.10.26
//

#include <stdbool.h>

#include "lexer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define D(kind) [kind] = #kind

static const char* lexer_token_kind_names[] = {
    D(TOK_ERROR),

    D(TOK_STORE_LIT),
    D(TOK_STORE_COPY),
    D(TOK_STORE_ARITH),
    D(TOK_FLOW_LABEL),
    D(TOK_FLOW_JUMP),
    D(TOK_FLOW_CJUMP),
    D(TOK_IO_PUTC),
    D(TOK_IO_GETC),
    D(TOK_IO_PUTI),

    D(TOK_NUM),
    D(TOK_LABEL),
    D(TOK_OP),

    D(TOK_EOF),
};

static const char* lexer_token_err_kind_names[] = {
    D(TOK_ERR_INVALID),
    D(TOK_ERR_WORD_TOO_LONG),
    D(TOK_ERR_INT32_TOO_SMALL),
    D(TOK_ERR_INT32_TOO_LARGE),
    D(TOK_ERR_UNEXPECTED_CHAR),
};
#undef D

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
_Static_assert(ARRAY_SIZE(lexer_token_kind_names) == LEXER_TOKEN_KIND_COUNT,
    "Lexer: lexer_token_kind_names must be in sync with lexer_token_kind.");

_Static_assert(ARRAY_SIZE(lexer_token_err_kind_names) == TOK_ERR_KIND_COUNT,
    "Lexer: lexer_token_err_kind_names_simple must be in sync with lexer_token_error_kind.");

static const lexer_token lexer_empty_result[1] = {
    { .kind = TOK_EOF, .line = 1, .column = 1, .text = NULL, .len = 0 }
};

#define INITIAL_TOKEN_VECTOR_CAP 16

typedef struct {
    const char* cur;
    uint32_t line;
    const char* line_start; // Can be used to calculate column.

    size_t error_count;

    // token vector
    lexer_token* tokens;
    size_t tokens_len, tokens_cap;
} lexer_state;

// Helpers

static inline unsigned char current(const lexer_state* s) {
    return (unsigned char)*s->cur;
}

static inline unsigned char peek(const lexer_state* s) {
    return (*s->cur) ? (unsigned char)*(s->cur + 1) : '\0'; // If the current is 0 everything after it should be treated like it is 0 as well.
}

static void advance(lexer_state* s) {
    if (current(s) == '\n') {
        s->line++;
        s->line_start = s->cur + 1;
    }
    s->cur++;
}

static inline bool is_digit(const unsigned char c) {
    return (c >= '0' && c <= '9');
}
static inline bool is_alpha(const unsigned char c) {
    return (c | 32) >= 'a' && (c | 32) <= 'z';
}
static inline bool is_skippable(const unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}
static inline bool is_operation(const unsigned char c) {
    return c == '+' || c == '-' || c == '*' || c == '/' || c == '%';
}

static void skip_trivs(lexer_state* s) {
    for (;;) {
        if (is_skippable(current(s)))
            advance(s);
        else if (current(s) == '#') {
            const char* cur_line = s->line_start;
            while (s->line_start == cur_line && current(s))
                advance(s);
        }
        else
            return;
    }
}

static inline lexer_token make(const lexer_state* s, const char* start, const lexer_token_kind kind) {
    return (lexer_token){
        .kind = kind,
        .line = s->line,
        .column = (uint32_t)(start - s->line_start + 1),
        .text = start,
        .len  = (uint32_t)(s->cur - start),
    };
}

static inline lexer_token make_error(const lexer_state* s, const char* start,
                                     const lexer_token_error_kind error) {
    lexer_token t = make(s, start, TOK_ERROR);
    t.err = error;
    return t;
}

static bool push_new_token(lexer_state* s, const lexer_token t) {
    if (s->tokens_cap == s->tokens_len) {
        const size_t new_cap = s->tokens_cap * 2;
        lexer_token* new_tokens = realloc(s->tokens, sizeof(lexer_token) * new_cap);
        if (!new_tokens)
            return false;

        s->tokens = new_tokens;
        s->tokens_cap = new_cap;
    }

    s->tokens[s->tokens_len++] = t;
    return true;
}

// Tokenizer

#define MAX_LABEL_LEN 31

#define KW(str, k) {str, sizeof(str) - 1, k }
static const struct {
    const char*      kw;
    size_t           len;
    lexer_token_kind kind;
} keywords[] = {
    KW("yami",   TOK_STORE_LIT),  KW("Yami",   TOK_STORE_COPY),  KW("YAMI",   TOK_STORE_ARITH),
    KW("hikari", TOK_FLOW_LABEL), KW("Hikari", TOK_FLOW_JUMP),   KW("HIKARI", TOK_FLOW_CJUMP),
    KW("rei",    TOK_IO_PUTC),    KW("Rei",    TOK_IO_GETC),     KW("REI",    TOK_IO_PUTI),
};
#undef KW

static lexer_token scan_word(lexer_state* s) {
    const char* start = s->cur;
    while (is_alpha(current(s)))
        advance(s);
    const size_t len = (size_t)(s->cur - start);

    for (size_t i = 0; i < sizeof(keywords) / sizeof(keywords[0]); ++i) {
        if (keywords[i].len == len && memcmp(keywords[i].kw, start, len) == 0)
            return make(s, start, keywords[i].kind);
    }

    if (len > MAX_LABEL_LEN)
        return make_error(s, start, TOK_ERR_WORD_TOO_LONG);

    return make(s, start, TOK_LABEL);
}

static lexer_token scan_number(lexer_state* s) {
    const char* start = s->cur;
    const bool neg = (current(s) == '-');
    if (neg) advance(s);

    uint64_t mag = 0;
    while (is_digit(current(s))) {
        if (mag <= (uint64_t)INT32_MAX + 1)
            mag = mag * 10 + (uint64_t)(current(s) - '0');
        advance(s);
    }

    const uint64_t limit = neg ? (uint64_t)INT32_MAX + 1 : (uint64_t)INT32_MAX + 0;
    if (mag > limit) {
        return make_error(s, start, neg
            ? TOK_ERR_INT32_TOO_SMALL
            : TOK_ERR_INT32_TOO_LARGE
        );
    }

    lexer_token t = make(s, start, TOK_NUM);
    t.num = neg
        ? (int32_t)-(int64_t)mag
        : (int32_t)mag;
    return t;
}

static lexer_token next_token(lexer_state* s) {
    skip_trivs(s);
    const char* start = s->cur;
    const unsigned char c = current(s);

    if (c == '\0')
        return make(s, start, TOK_EOF);
    if (is_alpha(c))
        return scan_word(s);
    if (is_digit(c) || (c == '-' && is_digit(peek(s))))
        return scan_number(s);
    if (is_operation(c)) {
        advance(s);
        lexer_token t = make(s, start, TOK_OP);
        t.op = (char)c;
        return t;
    }

    advance(s);
    return make_error(s, start, TOK_ERR_UNEXPECTED_CHAR);
}

bool lexer_tokenize(const char* source, lexer_result* out) {
    *out = (lexer_result){
        .tokens = (lexer_token*)lexer_empty_result,
        .token_count = 1,
        .error_count = 0,
        .source = source,
    };

    lexer_token* tokens = malloc(sizeof(lexer_token) * INITIAL_TOKEN_VECTOR_CAP);
    if (!tokens) return false;

    lexer_state s = (lexer_state){
        .cur = source,
        .line_start = source,
        .line = 1,
        .error_count = 0,
        .tokens = tokens,
        .tokens_cap = INITIAL_TOKEN_VECTOR_CAP,
        .tokens_len = 0,
    };

    for (;;) {
        const lexer_token t = next_token(&s);
        if (!push_new_token(&s, t)) {
            free(s.tokens);
            return false;
        }

        if (t.kind == TOK_ERROR)
            s.error_count++;
        if (t.kind == TOK_EOF)
            break;
    }

    *out = (lexer_result){
        .tokens = s.tokens,
        .token_count = s.tokens_len,
        .error_count = s.error_count,
        .source = source,
    };
    return true;
}

void lexer_free(lexer_result* lr) {
    if ((void*)lr->tokens != &lexer_empty_result) // Do not try to free static lexer_empty_result.
        free(lr->tokens);
    *lr = (lexer_result){ 0 };
}

const char* lexer_token_kind_name(const lexer_token_kind kind) {
    if ((unsigned)kind >= LEXER_TOKEN_KIND_COUNT || !lexer_token_kind_names[kind])
        return "?";
    return lexer_token_kind_names[kind];
}

const char* lexer_token_err_kind_name(const lexer_token_error_kind kind) {
    if ((unsigned)kind >= TOK_ERR_KIND_COUNT || !lexer_token_err_kind_names[kind])
        return "?";
    return lexer_token_err_kind_names[kind];
}

void lexer_dump_tokens(const lexer_result* lr) {
    // The lexeme column is as wide as the longest quoted lexeme.
    int lex_width = 6; // "LEXEME"
    for (size_t i = 0; i < lr->token_count; ++i) {
        const int w = (int)lr->tokens[i].len + 2; // + 2 quotes
        if (w > lex_width)
            lex_width = w;
    }

    printf("\n=== TOKEN DUMP ===\n");
    printf("%-5s  %-12s  %-8s  %-*s  %s\n", "IDX", "KIND", "LOC", lex_width, "LEXEME", "DATA");

    for (size_t i = 0; i < lr->token_count; ++i) {
        const lexer_token* t = &lr->tokens[i];

        char loc[24];
        snprintf(loc, sizeof loc, "%u:%u", t->line, t->column);

        // IDX, KIND, LOC
        printf("%04zu   %-12s  %-8s  ", i, lexer_token_kind_name(t->kind), loc);

        // LEXEME (quoted, padded to the column width; EOF has none)
        if (t->kind == TOK_EOF) {
            printf("%-*s  ", lex_width, "");
        } else {
            printf("\"%.*s\"%*s  ", (int)t->len, t->text, lex_width - (int)t->len - 2, "");
        }

        // DATA
        switch (t->kind) {
        case TOK_NUM:
            printf("val=%d", t->num);
            break;
        case TOK_OP:
            printf("op='%c'", t->op);
            break;
        case TOK_ERROR:
            printf("error=\"%s\"", lexer_token_err_kind_name(t->err));
            break;
        default:
            break;
        }

        printf("\n");
    }

    printf("\n%zu tokens, %zu errors\n", lr->token_count, lr->error_count);
    printf("=== END TOKENS ===\n\n");
}
