//
// Created by erikd on 31.10.26.
//

#pragma once

#include "lexer.h"

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

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
    DIAG_MISSING_SPACE_AFTER_MINUS,
    DIAG_LABEL_HAS_DIGITS,
    DIAG_UNEXPECTED_EOF,

    // semantic
    DIAG_REGISTER_NEGATIVE,
    DIAG_REGISTER_NUM_TOO_LARGE,
    DIAG_LABEL_REDEFINED,
    DIAG_LABEL_UNDEFINED,

    DIAG_CODE_COUNT,
} diag_code;

typedef struct {
    diag_code     code;
    diag_severity severity;

    uint32_t line, column, len;
    const char* text;

    uint32_t note_line, note_column;
} diag;

// What parser was trying to read
typedef enum {
    EXPECT_INSTRUCTION,
    EXPECT_STORE_KEYWORD, // after a leading register number
    EXPECT_REGISTER,
    EXPECT_LITERAL,
    EXPECT_OPERATOR,
    EXPECT_LABEL,
} diag_expect;

typedef struct {
    const lexer_token* head;
    const lexer_token* keyword;
} diag_stmt;

// Used for adjacent errors that the parser found. For example: "unexpected char, unexpected char, ..."
typedef struct {
    uint32_t line, column, len;
    const char* text;
} diag_span;

typedef struct {
    diag* items;
    size_t len, cap;
    size_t error_count, warning_count;
    const char* source;
    bool diag_out_of_memory; // Should be checked by caller frequently. If true no new diag will be added to the existing list.
} diag_list;

void diag_list_init(diag_list* dl, const char* source);
void diag_list_free(diag_list* dl);

// Report functions (append on prev calls)
void diag_unexpected(diag_list* dl, diag_expect expected, const lexer_token* got, const diag_stmt* stmt);
void diag_unexpected_chars(diag_list* dl, const diag_span* span);
void diag_skipped_tokens(diag_list* dl, const lexer_token* err); // errors that are skipped during resync in the parser.
void diag_label_redefined(diag_list* dl, const lexer_token* again, const lexer_token* first);
void diag_label_undefined(diag_list* dl, const lexer_token* use);
void diag_register_out_of_range(diag_list* dl, const lexer_token* num);

// Outputs
// TODO: Implement printing to file and printing with color. NOTE: if source_filename is NULL we assume that there is no source file e.g. kage code came from -e flag.
void diag_render(const diag_list* dl, diag_severity kinds, const char* source_filename);
const char* diag_code_name(diag_code code);
