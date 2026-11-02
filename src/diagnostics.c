//
// Created by erikd on 31.10.26.
//

#include "diagnostics.h"

#include <stdio.h>
#include <stdlib.h>

#define LEXEME_PREVIEW_MAX_LENGTH 32
#define START_CAPACITY 16

// msg, help, and note are printf formats. print_fmt always passes three arguments: shown_len, text, dots. Formats may ignore args they dont need.
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
        "label '%.*s%s' is too long (maximum is 31 letters)",
        "shorten the label to at most 31 letters",
        NULL),
    D(DIAG_INT32_TOO_SMALL, DIAG_ERROR,
        "number '%.*s%s' does not fit in 32 bits (minimum is -2147483648)",
        "use a value of at least -2147483648",
        NULL),
    D(DIAG_INT32_TOO_LARGE, DIAG_ERROR,
        "number '%.*s%s' does not fit in 32 bits (maximum is 2147483647)",
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
        "expected a register number, found '%.*s'",
        "registers are non-negative integers; register 0 always reads as 0",
        NULL),
    D(DIAG_EXPECTED_LITERAL, DIAG_ERROR,
        "expected an integer literal, found '%.*s'",
        "write a number between -2147483648 and 2147483647",
        NULL),
    D(DIAG_EXPECTED_OPERATOR, DIAG_ERROR,
        "expected an operator, found '%.*s'",
        "use one of + - * / %%",
        NULL),
    D(DIAG_EXPECTED_LABEL, DIAG_ERROR,
        "expected a label name, found '%.*s'",
        "labels consist only of letters (at most 31) and must not be a keyword",
        NULL),
    D(DIAG_LABEL_HAS_DIGITS, DIAG_ERROR,
        "digits directly after a label are not part of it ('%.*s')",
        "labels consist only of letters; remove the digits",
        "the label is here"),
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

    // warnings
    D(DIAG_LABEL_UNUSED, DIAG_WARNING,
        "label '%.*s' is never jumped to",
        "remove it, or jump to it with 'Hikari %.*s'",
        NULL),
};
#undef D

static const char* const usage_table[] = {
    [TOK_ERROR]       = NULL,

    [TOK_STORE_LIT]   = "R yami N       store literal N in register R",
    [TOK_STORE_COPY]  = "R Yami S       copy register S into register R",
    [TOK_STORE_ARITH] = "R YAMI S op T  R = S op T, where op is one of + - * / %",
    [TOK_FLOW_LABEL]  = "hikari L       define label L",
    [TOK_FLOW_JUMP]   = "Hikari L       jump to label L",
    [TOK_FLOW_CJUMP]  = "HIKARI L S     jump to L if register S is not zero",
    [TOK_IO_PUTC]     = "rei S          print register S as a character (0 ends the program)",
    [TOK_IO_GETC]     = "Rei S          read a character into register S",
    [TOK_IO_PUTI]     = "REI S          print register S as a number",

    [TOK_NUM]         = NULL,
    [TOK_LABEL]       = NULL,
    [TOK_OP]          = NULL,
    [TOK_EOF]         = NULL,
};

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
_Static_assert(ARRAY_SIZE(diag_table) == DIAG_CODE_COUNT, "Diagnostics: diag_table must be in sync with diag_code.");
_Static_assert(ARRAY_SIZE(usage_table) == LEXER_TOKEN_KIND_COUNT, "Diagnostics: usage_table must be in sync with lexer_token_kind.");

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
        // We create a START_CAPACITY sized array only when the first error is encountered. An arr is not generated at init time.
        const size_t new_cap = dl->cap ? dl->cap * 2 : START_CAPACITY;
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
        .text = text
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

static void set_note(diag* d, const lexer_token* t) {
    d->note_line   = t->line;
    d->note_column = t->column;
    d->note_len    = t->len;
    d->note_text   = t->text;
}

static diag_code code_for(const diag_expect expected, const lexer_token* got) {
    if (got->kind == TOK_EOF)
        return DIAG_UNEXPECTED_EOF;

    if (got->kind == TOK_ERROR) {
        // An oversized number in a register slot is really just an register error.
        const bool reg = (expected == EXPECT_REGISTER);
        switch (got->err) {
            case TOK_ERR_INT32_TOO_SMALL:
                return reg ? DIAG_REGISTER_NEGATIVE : DIAG_INT32_TOO_SMALL;
            case TOK_ERR_INT32_TOO_LARGE:
                return reg ? DIAG_REGISTER_NUM_TOO_LARGE : DIAG_INT32_TOO_LARGE;
            case TOK_ERR_WORD_TOO_LONG:
                return DIAG_WORD_TOO_LONG;
            case TOK_ERR_UNEXPECTED_CHAR:
                return DIAG_UNEXPECTED_CHARS;
            default:
                break;
        }
    }

    if (got->kind == TOK_NUM && expected == EXPECT_REGISTER && got->num < 0)
        return DIAG_REGISTER_NEGATIVE;

    return (diag_code)expected; // We can cast since diag_expect is defined as a subset of diag_code.
}

static const char* usage_of(const lexer_token_kind kind) {
    return (unsigned)kind < LEXER_TOKEN_KIND_COUNT ? usage_table[kind] : NULL;
}

void diag_list_init(diag_list* dl) {
    *dl = (diag_list){
        .items = NULL,
        .len = 0,
        .cap = 0,
        .error_count = 0,
        .warning_count = 0,
        .diag_out_of_memory = false,
    };
}

void diag_list_free(diag_list* dl) {
    free(dl->items);
    *dl = (diag_list){ 0 };
}

// Report functions (append on prev calls)
void diag_report(diag_list* dl, const diag_code code, const lexer_token* at) {
    push_tok(dl, code, at);
}

void diag_unexpected(diag_list* dl, const diag_expect expected, const lexer_token* got, const diag_stmt* stmt) {
    const diag_code code = code_for(expected, got);
    diag* d = push_tok(dl, code, got);
    if (!d)
        return; // If OOM

    if (stmt) {
        if (stmt->keyword)
            d->stmt_kind = stmt->keyword->kind;
        if (code == DIAG_UNEXPECTED_EOF && stmt->head) {
            set_note(d, stmt->head);
        }
    }
}

void diag_report_span(diag_list* dl, const diag_code code, const lexer_token* start, const uint32_t text_len) {
    push(dl, code, start->line, start->column, text_len, start->text);
}

void diag_report_note(diag_list* dl, const diag_code code, const lexer_token* at, const lexer_token* note_at) {
    diag* d = push_tok(dl, code, at);
    if (d) { // If not OOM
        set_note(d, note_at);
    }
}

// Outputs Helper

static int digit_count(uint32_t v) {
    int n = 1;
    for (; v >= 10; ++n)
        v /= 10;
    return n;
}

static bool truncates_lexeme(const diag_code code) {
    switch (code) {
        case DIAG_UNEXPECTED_CHARS:
        case DIAG_WORD_TOO_LONG:
        case DIAG_INT32_TOO_SMALL:
        case DIAG_INT32_TOO_LARGE:
        case DIAG_REGISTER_NEGATIVE:
        case DIAG_REGISTER_NUM_TOO_LARGE:
            return true;
        default:
            return false;
    }
}

static void print_fmt(FILE* stream, const char* fmt, const uint32_t len, const char* text, const bool truncate) {
    uint32_t shown = len;
    const char* dots = "";
    if (truncate && len > LEXEME_PREVIEW_MAX_LENGTH) {
        shown = LEXEME_PREVIEW_MAX_LENGTH;
        dots = "...";
    }

    fprintf(stream, fmt, (int)shown, text ? text : "", dots);
}

static void print_excerpt(FILE* stream, const uint32_t line, const uint32_t column, const uint32_t len, const char* text) {
    if (!text || column == 0)
        return;

    const char* start_line = text - (column - 1); // column is 1-based byte offset.
    const char* end_line = start_line;
    while (*end_line && *end_line != '\n')
        end_line++;
    size_t n = (size_t)(end_line - start_line);
    if (n > 0 && start_line[n - 1] == '\r') // This is here just because Windows uses /r/n FOR SOME FUCKING REASON. (Windows bashing mode activated!)
        n--;

    const int w = digit_count(line);
    fprintf(stream, " %*u | %.*s\n", w, line, (int)n, start_line);

    size_t off = column - 1;
    if (off > n)
        off = n;
    size_t span = len ? len : 1;
    if (off < n && off + span > n)
        span = n - off;

    fputc(' ', stream);
    for (int i = 0; i < w; ++i)
        fputc(' ', stream);
    fputs(" | ", stream);
    for (size_t i = 0; i < off; ++i) // keep tabs so the caret lines up
        fputc(start_line[i] == '\t' ? '\t' : ' ', stream);
    fputc('^', stream);
    for (size_t i = 1; i < span; ++i)
        fputc('~', stream);
    fputc('\n', stream);
}

void diag_render(const diag_list* dl, const diag_severity kinds, const char* source_filename, FILE* stream) {
    const char* file = source_filename ? source_filename : "<input>";
    size_t errors = 0, warnings = 0;

    for (size_t i = 0; i < dl->len; ++i) {
        const diag* d = &dl->items[i];
        if (!(d->severity & kinds))
            continue;

        const diag_info* info = info_of(d->code);
        const bool truncate = truncates_lexeme(d->code);
        const char* severity_name = "invalid";
        if (d->severity & DIAG_ERROR) {
            errors++;
            severity_name = "error";
        }
        else if (d->severity & DIAG_WARNING) {
            warnings++;
            severity_name = "warning";
        }

        fprintf(stream, "%s:%u:%u: %s: ", file, d->line, d->column, severity_name);
        print_fmt(stream, info->msg, d->len, d->text, truncate);
        fputc('\n', stream);
        print_excerpt(stream, d->line, d->column, d->len, d->text);

        if (d->note_text) {
            fprintf(stream, "%s:%u:%u: note: ", file, d->note_line, d->note_column);
            print_fmt(stream, info->note ? info->note : "see here", d->note_len, d->note_text, false);
            fputc('\n', stream);
            print_excerpt(stream, d->note_line, d->note_column, d->note_len, d->note_text);
        }

        if (info->help) {
            fputs("  help: ", stream);
            print_fmt(stream, info->help, d->len, d->text, truncate);
            fputc('\n', stream);
        }

        const char* usage = usage_of(d->stmt_kind);
        if (usage)
            fprintf(stream, "  usage: %s\n", usage);

        fputc('\n', stream);
    }

    if (errors)
        fprintf(stream, "%zu error%s", errors, errors == 1 ? "" : "s");
    if (errors && warnings)
        fputs(", ", stream);
    if (warnings)
        fprintf(stream, "%zu warning%s", warnings, warnings == 1 ? "" : "s");
    if (errors || warnings)
        fputc('\n', stream);

    if (dl->diag_out_of_memory)
        fputs("diagnostics ran out of memory; output may be incomplete\n", stream);
}

const char* diag_code_name(const diag_code code) {
    return info_of(code)->name;
}
