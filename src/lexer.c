//
// Created by erikd on 04.06.26.
// Refactored by erikd on 29.10.26
//

#include <stdbool.h>

#include "lexer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* lexer_token_kind_names[] = {
    [TOK_INVALID] = "INVALID",

    [TOK_STORE_LIT] = "STORE_LIT",
    [TOK_STORE_COPY] = "STORE_COPY",
    [TOK_STORE_ARITH] = "STORE_ARITH",
    [TOK_FLOW_LABEL] = "FLOW_LABEL",
    [TOK_FLOW_JUMP] = "FLOW_JUMP",
    [TOK_FLOW_CJUMP] = "FLOW_CJUMP",
    [TOK_IO_PUTC] = "IO_PUTC",
    [TOK_IO_GETC] = "IO_GETC",
    [TOK_IO_PUTI] = "IO_PUTI",

    [TOK_NUM] = "NUM",
    [TOK_LABEL] = "LABEL",
    [TOK_OP] = "OP",

    [TOK_EOF] = "EOF",
    [TOK_ERROR] = "ERROR",
};

_Static_assert(sizeof(lexer_token_kind_names) / sizeof(lexer_token_kind_names[0]) == LEXER_TOKEN_KIND_COUNT,
    "Lexer: lexer_token_kind_names must be in sync with lexer_token_kind.");

#define STR_(x) #x
#define STR(x)  STR_(x)

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

static inline lexer_token make_error(const lexer_state* s, const char* start, const char* msg) {
    lexer_token t = make(s, start, TOK_ERROR);
    t.error = msg;
    return t;
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
        return make_error(s, start, "Lexer: Word exceeds maximum length of " STR(MAX_LABEL_LEN) " characters.\n\tHelp: Shorten this label to " STR(MAX_LABEL_LEN) " ASCII letters or fewer.");

    return make(s, start, TOK_LABEL);
}

static lexer_token scan_number(lexer_state* s) {
    const char* start = s->cur;

    char* end;
    const long long v = strtoll(start, &end, 10);
    s->cur = end;   // consume whole lexeme

    if (v < INT32_MIN || v > INT32_MAX)
        return make_error(s, start,
            "Lexer: Integer literal is outside the int32 range.\n\tHelp: Use a value between -2147483648 and 2147483647. For Registers use a value between 0 and 2147483647."
        );

    lexer_token t = make(s, start, TOK_NUM);
    t.num = (int32_t)v;
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
    return make_error(s, start,
        "Lexer: Unexpected character. \n\tHelp: Remove this character or replace it with a valid Kage token."
    );
}

lexer_result lexer_tokenize(const char* source) {
    lexer_state s = (lexer_state){
        .cur = source,
        .line_start = source,
        .line = 1,
        .error_count = 0,
        .tokens = malloc(sizeof(lexer_token) * INITIAL_TOKEN_VECTOR_CAP),
        .tokens_cap = INITIAL_TOKEN_VECTOR_CAP,
        .tokens_len = 0,
    };
    if (!s.tokens) {
        fprintf(stderr, "Lexer: Out of Memory");
        return (lexer_result){0};
    }

    for (;;) {
        const lexer_token t = next_token(&s);

        if (s.tokens_cap == s.tokens_len) {
            const size_t new_cap = s.tokens_cap * 2;
            lexer_token* new_tokens = realloc(s.tokens, sizeof(lexer_token) * new_cap);
            if (!new_tokens) {
                fprintf(stderr, "Lexer: Out of Memory");
                free(s.tokens);
                return (lexer_result){0};
            }

            s.tokens = new_tokens;
            s.tokens_cap = new_cap;
        }

        s.tokens[s.tokens_len++] = t;
        if (t.kind == TOK_ERROR)
            s.error_count++;
        if (t.kind == TOK_EOF)
            break;
    }

    return (lexer_result){
        .tokens = s.tokens,
        .token_count = s.tokens_len,
        .error_count = s.error_count,
    };
}

void lexer_free(lexer_result* lr) {
    free(lr->tokens);
    lr->tokens      = NULL;
    lr->token_count = 0;
    lr->error_count = 0;
}

const char* lexer_token_kind_name(const lexer_token_kind kind) {
    if ((unsigned)kind >= LEXER_TOKEN_KIND_COUNT || !lexer_token_kind_names[kind])
        return "?";
    return lexer_token_kind_names[kind];
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
            printf("error=\"%.*s\"", (int)strcspn(t->error, "\n"), t->error);
            break;
        default:
            break;
        }

        printf("\n");
    }

    printf("\n%zu tokens, %zu errors\n", lr->token_count, lr->error_count);
    printf("=== END TOKENS ===\n\n");
}