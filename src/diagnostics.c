//
// Created by erikd on 31.10.26.
//

#include "diagnostics.h"

#include <stdlib.h>

#define LEXEME_PREVIEW_MAX_LENGTH 32

typedef struct {
    const char* name;
    diag_severity severity;
    const char* msg;
    const char* help; // maybe null
    const char* note; // maybe null; text for the secondary location
} diag_info;

#define D(code, sev, msg, help, note) [code] = { #code, sev, msg, help, note }

static const diag_info diag_table[] = {
    D(DIAG_INVALID, DIAG_ERROR,
        "invalid diagnostic",
        NULL,
        NULL),

    // lexical
    D(DIAG_UNEXPECTED_CHARS, DIAG_ERROR,
        "unexpected '%.*s%s'",
        "Kage only knows letters, digits, + - * / %% and '#' comments",
        NULL),
    D(DIAG_WORD_TOO_LONG, DIAG_ERROR,
        "label '%.*s%s' is too long (maximum is 31 letters",
        "shorten the label to at most 31 letters",
        NULL),
    D(DIAG_INT32_TOO_SMALL, DIAG_ERROR,
        "number '%.*%s' does not fit in 32 bits (minimum is -2147483648)",
        "use a value of at least -2147483648",
        NULL),
    D(DIAG_INT32_TOO_LARGE, DIAG_ERROR,
        "number '%.*%s' does not fit in 32 bits (maximum is 2147483647)",
        "use a value of at most 2147483647",
        NULL),

    // syntax
    D(DIAG_EXPECTED_INSTRUCTION, DIAG_ERROR,
        "expected an instruction, found '%.*s'",
        "an instruction starts with a register number (like '1 yami 5') or with hikari, Hikari, HIKARI, rei, Rei or REI",
        NULL),
    D(DIAG_EXPECTED_STORE_KEYWORD, DIAG_ERROR,
        "expected yami, Yami or YAMI after the register number, found '%.*s'",
        "'yami' stores a literal, 'Yami' copies a register, 'YAMI' does arithmetic",
        NULL),
    D(DIAG_EXPECTED_REGISTER, DIAG_ERROR,
        "expected register, found '%.*s'",
        "registers are non-negative integers; register 0 always reads as 0",
        NULL),
    D(DIAG_EXPECTED_LITERAL, DIAG_ERROR,
        "expected an integer literal, found '%.*s'",
        "write a number between -2147483648 and 2147483647",
        NULL),
    D(DIAG_EXPECTED_OPERATOR, DIAG_ERROR,
        "expected an operator, found '%.*s'",
        "use on of + - * / %%",
        NULL),
    D(DIAG_EXPECTED_LABEL, DIAG_ERROR,
        "expected a label name, found '%.*s'",
        "labels consist only of letters (at most 31) and must not be a keyword",
        NULL),
    D(DIAG_MISSING_SPACE_AFTER_MINUS, DIAG_ERROR,
        "'%.*s' is read as a negative number, not as a subtraction",
        "to subtract, put a space after '-' (for example: 3 YAMI 1 - 2)",
        NULL),
    D(DIAG_LABEL_HAS_DIGITS, DIAG_ERROR,
      "digits directly after a label are not part of it ('%.*s')",
      "labels consist only of letters; remove the digits",
      NULL),
    D(DIAG_UNEXPECTED_EOF, DIAG_ERROR,
      "unexpected end of file",
      "the last instruction is incomplete",
      "the incomplete instruction starts here"),

    // semantic
    D(DIAG_REGISTER_NEGATIVE, DIAG_ERROR,
      "register address '%.*s%s' cannot be negative",
      "use a non-negative register; only the literal after 'yami' may be negative",
      NULL),
    D(DIAG_REGISTER_NUM_TOO_LARGE, DIAG_ERROR,
      "register address '%.*s%s' is too large",
      "the largest register address is 2147483647",
      NULL),
    D(DIAG_LABEL_REDEFINED, DIAG_ERROR,
      "label '%.*s' is defined more than once",
      "each label may only be defined once; rename or remove one of them",
      "first defined here"),
    D(DIAG_LABEL_UNDEFINED, DIAG_ERROR,
      "jump to undefined label '%.*s'",
      "define it somewhere with 'hikari %.*s'",
      NULL),
};
#undef D

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
_Static_assert(ARRAY_SIZE(diag_table) == DIAG_CODE_COUNT, "Diagnostics: diag_table must be in sync with diag_code.");

// Helpers

static const diag_info* info_of(const diag_code code) {
    if ((unsigned)code >= DIAG_CODE_COUNT)
        return &diag_table[DIAG_INVALID];
    return &diag_table[code];
}

static diag* push(diag_list* dl, const diag_code code, const uint32_t line, const uint32_t column, const uint32_t len, const char* text) {
    if (dl->diag_out_of_memory)
        return NULL; // We are not going to try again

    if (dl->len == dl->cap) {
        const size_t new_cap = dl->cap * 2;
        diag* new_items = realloc(dl->items, sizeof(diag) * new_cap);
        if (!new_items) {
            dl->diag_out_of_memory = true;
            return NULL;
        }

        dl->items = new_items;
        dl->cap = new_cap;
    }

    const diag_info* info = info_of(code);
    diag* d = &dl->items[dl->len++];

    *d = (diag){
        .code = code,
        .severity = info->severity,
        .line = line,
        .column = column,
        .len = len,
        .text = text ? text : ""
    };

    if (info->severity & DIAG_ERROR)
        dl->error_count++;
    if (info->severity & DIAG_WARNING)
        dl->warning_count++;

    return d;
}

static diag* push_tok(diag_list* dl, const diag_code code, const lexer_token* t) {
    return push(dl, code, t->line, t->column, t->len, t->text);
}

static diag_code generic_code(const diag_expect expected) {
    switch (expected) {
        case EXPECT_INSTRUCTION:   return DIAG_EXPECTED_INSTRUCTION;
        case EXPECT_STORE_KEYWORD: return DIAG_EXPECTED_STORE_KEYWORD;
        case EXPECT_REGISTER:      return DIAG_EXPECTED_REGISTER;
        case EXPECT_LITERAL:       return DIAG_EXPECTED_LITERAL;
        case EXPECT_OPERATOR:      return DIAG_EXPECTED_OPERATOR;
        case EXPECT_LABEL:         return DIAG_EXPECTED_LABEL;
        default:                   return DIAG_INVALID;
    }
}

static diag_code lexical_code(const lexer_token_error_kind err) {
    switch (err) {
        case TOK_ERR_WORD_TOO_LONG:   return DIAG_WORD_TOO_LONG;
        case TOK_ERR_INT32_TOO_SMALL: return DIAG_INT32_TOO_SMALL;
        case TOK_ERR_INT32_TOO_LARGE: return DIAG_INT32_TOO_LARGE;
        case TOK_ERR_UNEXPECTED_CHAR: return DIAG_UNEXPECTED_CHARS;
        default:                      return DIAG_INVALID;
    }
}

#define INITIAL_CAPACITY 1

void diag_list_init(diag_list* dl, const char* source) {
    *dl = (diag_list){
        .items = malloc(sizeof(diag) * INITIAL_CAPACITY),
        .len = 0,
        .cap = INITIAL_CAPACITY,
        .error_count = 0,
        .warning_count = 0,
        .source = source,
    };

    dl->diag_out_of_memory = dl->items == NULL;
}

void diag_list_free(diag_list* dl) {
    free(dl->items);
    *dl = (diag_list){ 0 };
}

// TODO: Do the rest

// Report functions (append on prev calls)
void diag_unexpected(diag_list* dl, diag_expect expected, const lexer_token* got, const diag_stmt* stmt) {}
void diag_unexpected_chars(diag_list* dl, const diag_span* span) {}
void diag_skipped_tokens(diag_list* dl, const lexer_token* err) {}
void diag_label_redefined(diag_list* dl, const lexer_token* again, const lexer_token* first) {}
void diag_label_undefined(diag_list* dl, const lexer_token* use) {}
void diag_register_out_of_range(diag_list* dl, const lexer_token* num) {}

// Outputs
void diag_render(const diag_list* dl, diag_severity kinds, const char* source_filename) {}

const char* diag_code_name(const diag_code code) {
    return info_of(code)->name;
}
