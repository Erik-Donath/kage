//
// Created by erikd on 04.06.26.
//

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    DIAGNOSTIC_LEXER,
    DIAGNOSTIC_PARSER,
    DIAGNOSTIC_LABEL,
} diagnostic_kind;

typedef struct {
    diagnostic_kind kind;
    uint32_t line;
    uint32_t column;
    char* message;
    char* help;
} diagnostic;

typedef struct {
    diagnostic* items;
    size_t len;
    size_t cap;
    bool allocation_failed;
} diagnostic_list;

void diagnostics_init(diagnostic_list* list);
void diagnostics_free(diagnostic_list* list);
bool diagnostics_add(diagnostic_list* list,
                    diagnostic_kind kind,
                    uint32_t line,
                    uint32_t column,
                    const char* message,
                    const char* help);
void diagnostics_report(const diagnostic_list* list);
