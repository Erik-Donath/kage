//
// Created by erikd on 04.06.26.
//

#include "diagnostics.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char* duplicate_string(const char* text) {
    if (!text)
        return NULL;

    const size_t len = strlen(text);
    char* copy = malloc(len + 1U);
    if (!copy)
        return NULL;

    memcpy(copy, text, len + 1U);
    return copy;
}

static const char* diagnostic_name(const diagnostic_kind kind) {
    switch (kind) {
        case DIAGNOSTIC_LEXER: return "LexerError";
        case DIAGNOSTIC_PARSER: return "ParseError";
        case DIAGNOSTIC_LABEL: return "LabelError";
        default: return "Diagnostic";
    }
}

void diagnostics_init(diagnostic_list* list) {
    if (!list)
        return;
    list->items = NULL;
    list->len = 0;
    list->cap = 0;
    list->allocation_failed = false;
}

void diagnostics_free(diagnostic_list* list) {
    if (!list)
        return;
    for (size_t i = 0; i < list->len; ++i) {
        free(list->items[i].message);
        free(list->items[i].help);
    }
    free(list->items);
    list->items = NULL;
    list->len = 0;
    list->cap = 0;
    list->allocation_failed = false;
}

bool diagnostics_add(diagnostic_list* list,
                    diagnostic_kind kind,
                    uint32_t line,
                    uint32_t column,
                    const char* message,
                    const char* help) {
    if (!list)
        return false;

    if (list->len >= list->cap) {
        const size_t new_capacity = list->cap == 0 ? 8U : list->cap * 2U;
        diagnostic* new_items = realloc(list->items, new_capacity * sizeof(*new_items));
        if (!new_items) {
            list->allocation_failed = true;
            return false;
        }
        list->items = new_items;
        list->cap = new_capacity;
    }

    char* message_copy = duplicate_string(message);
    if (!message_copy) {
        list->allocation_failed = true;
        return false;
    }

    char* help_copy = duplicate_string(help);
    if (help && !help_copy) {
        free(message_copy);
        list->allocation_failed = true;
        return false;
    }

    list->items[list->len++] = (diagnostic){
        .kind = kind,
        .line = line,
        .column = column,
        .message = message_copy,
        .help = help_copy,
    };
    return true;
}

void diagnostics_report(const diagnostic_list* list) {
    if (!list)
        return;

    if (list->allocation_failed && list->len == 0) {
        fprintf(stderr, "Error: out of memory while collecting diagnostics\n");
        return;
    }

    for (size_t i = 0; i < list->len; ++i) {
        const diagnostic* item = &list->items[i];
        fprintf(stderr, "%s (%u:%u): %s\n",
                diagnostic_name(item->kind),
                item->line,
                item->column,
                item->message ? item->message : "unknown error");
        if (item->help && item->help[0] != '\0')
            fprintf(stderr, "  help: %s\n", item->help);
    }
}
