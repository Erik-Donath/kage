//
// Created by erikd on 04.06.26.
//

#include "tokens.h"
#include <stdlib.h>
#include <stdio.h>

void token_vec_init(token_vec* vec) {
    vec->len = 0;
    vec->cap = 16;
    vec->tokens = malloc(sizeof(token) * vec->cap);
}

void token_vec_free(token_vec* vec) {
    for (size_t i = 0; i < vec->len; ++i) {
        if (vec->tokens[i].type == LABEL)
            free((void*)vec->tokens[i].str);
    }

    free(vec->tokens);
    vec->len = 0;
    vec->cap = 0;
}

void token_vec_push(token_vec* vec, const token t) {
    if (vec->len >= vec->cap) {
        vec->cap *= 2;
        vec->tokens = realloc(vec->tokens, vec->cap * sizeof(t));
    }
    vec->tokens[vec->len++] = t;
}

static const char* token_names[] = {
    [STORE_LIT]     = "STORE_LIT",
    [STORE_CPY]     = "STORE_CPY",
    [STORE_EXP]     = "STORE_EXP",
    [FLOW_LAB]      = "FLOW_LAB",
    [FLOW_JMP]      = "FLOW_JMP",
    [FLOW_COND_JMP] = "FLOW_COND_JMP",
    [IO_PRINT]      = "IO_PRINT",
    [IO_READ]       = "IO_READ",
    [IO_DEC_PRINT]  = "IO_DEC_PRINT",
    [NUM]           = "NUM",
    [OP]            = "OP",
    [LABEL]         = "LABEL",
    [EOF_TOKEN]     = "EOF_TOKEN",
};

void token_dump(const token_vec* vec) {
    printf("\n=== TOKEN DUMP ===\n");

    for (size_t i = 0; i < vec->len; ++i) {
        const token* t = &vec->tokens[i];
        printf("%04zu | %-13s ", i, token_names[t->type]);

        switch (t->type) {
            case NUM:
                printf("%d", t->num);
                break;
            case OP:
                printf("'%c'", t->op);
                break;
            case LABEL:
                printf("\"%s\"", t->str);
                break;
            default:
                break;
        }

        printf("  [%d:%d]\n", t->line, t->column);
    }

    printf("=== END TOKENS ===\n\n");
}
