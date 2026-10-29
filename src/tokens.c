//
// Created by erikd on 04.06.26.
//

#include "tokens.h"
#include <stdlib.h>
#include <stdio.h>

bool token_vec_init(token_vec* vec) {
    vec->len = 0;
    vec->cap = 16;
    vec->tokens = malloc(sizeof(token) * vec->cap);
    if (!vec->tokens) {
        vec->cap = 0;
        return false;
    }
    return true;
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

bool token_vec_push(token_vec* vec, const token t) {
    if (vec->len >= vec->cap) {
        if (vec->cap > SIZE_MAX / 2 ||
            vec->cap * 2 > SIZE_MAX / sizeof(vec->tokens[0]))
            return false;

        const size_t new_cap = vec->cap * 2;
        token* tokens = realloc(vec->tokens, new_cap * sizeof(vec->tokens[0]));
        if (!tokens)
            return false;
        vec->tokens = tokens;
        vec->cap = new_cap;
    }
    vec->tokens[vec->len++] = t;
    return true;
}

const char* token_names[] = {
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
    [INVALID]       = "INVALID",
};

void token_dump(const token_vec* vec) {
    printf("\n=== TOKEN DUMP ===\n");
    printf("%-6s  %-16s  %-10s  %s\n", "INDEX", "TYPE", "LOC", "DATA");
    printf("------  ----------------  ----------  ----\n");

    for (size_t i = 0; i < vec->len; ++i) {
        const token* t = &vec->tokens[i];
        printf("%04zu    %-16s  ", i, token_names[t->type]);

        char loc[16];
        snprintf(loc, sizeof(loc), "%d:%d", t->line, t->column);
        printf("%-10s  ", loc);

        switch (t->type) {
            case NUM:   printf("val=%d",    t->num); break;
            case OP:    printf("op='%c'",   t->op);  break;
            case LABEL: printf("name=\"%s\"", t->str); break;
            default:    break;
        }

        printf("\n");
    }

    printf("=== END TOKENS ===\n\n");
}
