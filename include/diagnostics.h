//
// Created by erikd on 31.10.26.
//

#pragma once

#include "lexer.h"

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

typedef enum {
    DIAG_ERROR   = 0x1,
    DIAG_WARNING = 0x2,
    DIAG_ALL     = (DIAG_ERROR | DIAG_WARNING),
} diag_severity; // Do not use DIAG_ALL for reporting.

typedef enum {
    DIAG_INVALID = 0,

    // lexical
    DIAG_UNEXPECTED_CHARS,
    DIAG_WORD_TOO_LONG,
    DIAG_INT32_TOO_SMALL,
    DIAG_INT32_TOO_LARGE,

    // syntax
    DIAG_EXPECTED_INSTRUCTION,
    DIAG_EXPECTED_STORE_KEYWORD,
    DIAG_EXPECTED_REGISTER,
    DIAG_EXPECTED_LITERAL,
    DIAG_EXPECTED_OPERATOR,
    DIAG_EXPECTED_LABEL,
    DIAG_LABEL_HAS_DIGITS,
    DIAG_UNEXPECTED_EOF,

    // semantic
    DIAG_REGISTER_NEGATIVE,
    DIAG_REGISTER_NUM_TOO_LARGE,
    DIAG_LABEL_REDEFINED,
    DIAG_LABEL_UNDEFINED,

    // warnings
    DIAG_LABEL_UNUSED,

    DIAG_CODE_COUNT,
} diag_code;

// What parser was trying to read
typedef enum {
    EXPECT_INSTRUCTION      = DIAG_EXPECTED_INSTRUCTION,
    EXPECT_STORE_KEYWORD    = DIAG_EXPECTED_STORE_KEYWORD, // after a leading register number
    EXPECT_REGISTER         = DIAG_EXPECTED_REGISTER,
    EXPECT_LITERAL          = DIAG_EXPECTED_LITERAL,
    EXPECT_OPERATOR         = DIAG_EXPECTED_OPERATOR,
    EXPECT_LABEL            = DIAG_EXPECTED_LABEL,
} diag_expect; // Acts as a subset of diag_code.

// The statement being parsed when something unexpected came up.
typedef struct {
    const lexer_token* head;
    const lexer_token* keyword;
} diag_stmt;

typedef struct {
    diag_code     code;
    diag_severity severity;

    uint32_t line, column, len;
    const char* text;

    uint32_t note_line, note_column, note_len;
    const char* note_text; // NULL = no note

    lexer_token_kind stmt_kind;
} diag;

typedef struct {
    diag* items;
    size_t len, cap;
    size_t error_count, warning_count;
    bool diag_out_of_memory;  // If true, no new diagnostics are added. Check once after a stage is done: the list is still valid, but incomplete.
} diag_list;

void diag_list_init(diag_list* dl);
void diag_list_free(diag_list* dl);

// Report functions (append on prev calls)
void diag_report(diag_list* dl, diag_code code, const lexer_token* at);
void diag_unexpected(diag_list* dl, diag_expect expected, const lexer_token* got, const diag_stmt* stmt);
void diag_report_span(diag_list* dl, diag_code code, const lexer_token* start, uint32_t text_len);
void diag_report_note(diag_list* dl, diag_code code, const lexer_token* at, const lexer_token* note_at);

// Outputs
// TODO: Printing with color.
// NOTE: If source_filename is NULL we assume that there is no source file e.g. kage code came from -e flag input.
void diag_render(const diag_list* dl, diag_severity kinds, const char* source_filename, FILE* stream);
const char* diag_code_name(diag_code code);
