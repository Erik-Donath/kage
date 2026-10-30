#include "parser.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char* name;   // points into source, NOT NUL-terminated
    size_t len;
    size_t target;
} label_entry;

typedef struct {
    const char* name;
    size_t len;
    size_t instruction;
    uint32_t line;
    uint32_t column;
} jump_patch;

typedef struct {
    const lexer_result* tokens;
    diagnostic_list* diagnostics;
    size_t pos;

    ir_instruction* instructions;
    size_t instruction_count;
    size_t instruction_capacity;

    label_entry* labels;
    size_t label_count;
    size_t label_capacity;

    jump_patch* patches;
    size_t patch_count;
    size_t patch_capacity;

    uint32_t* registers;
    size_t register_count;
    size_t register_capacity;
} parser;

static lexer_token current(const parser* p) {
    return p->tokens->tokens[p->pos];
}

static bool label_eq(const char* a, const size_t a_len, const char* b, const size_t b_len) {
    return a_len == b_len && memcmp(a, b, a_len) == 0;
}

static const char* expected_help(const lexer_token_kind kind) {
    switch (kind) {
        case TOK_NUM:
            return "Add a number or register address here (for example, 1).";
        case TOK_OP:
            return "Insert one of: +, -, *, /, %.";
        case TOK_LABEL:
            return "Define a label with hikari <name> or correct the jump target.";
        default:
            return "Check the instruction spelling and operand order.";
    }
}

static bool consume(parser* p, const lexer_token_kind expected, lexer_token* result) {
    const lexer_token t = current(p);
    if (t.kind != expected) {
        if (t.kind != TOK_ERROR) {   // lexer errors are already reported
            char message[192];
            snprintf(message, sizeof(message), "expected %s but got %s",
                     lexer_token_kind_name(expected), lexer_token_kind_name(t.kind));
            diagnostics_add(p->diagnostics, DIAGNOSTIC_PARSER,
                            t.line, t.column, message, expected_help(expected));
        }
        return false;
    }

    *result = t;
    if (p->pos + 1 < p->tokens->token_count)
        p->pos++;
    return true;
}

static bool normalize_register(parser* p, const lexer_token t, uint32_t* result) {
    if (t.num < 0) {
        diagnostics_add(p->diagnostics, DIAGNOSTIC_PARSER,
                        t.line, t.column, "register address must be non-negative",
                        "Use register 0 or a positive register address.");
        return false;
    }
    if (t.num == 0) {
        *result = 0;
        return true;
    }
    if (p->register_count >= UINT32_MAX) {
        diagnostics_add(p->diagnostics, DIAGNOSTIC_PARSER,
                        t.line, t.column, "program uses too many registers", NULL);
        return false;
    }

    for (size_t i = 0; i < p->register_count; ++i) {
        if (p->registers[i] == (uint32_t)t.num) {
            *result = (uint32_t)(i + 1);
            return true;
        }
    }

    if (p->register_count == p->register_capacity) {
        const size_t new_capacity = p->register_capacity == 0
            ? 16
            : p->register_capacity * 2;
        if (new_capacity < p->register_capacity ||
            new_capacity > SIZE_MAX / sizeof(*p->registers)) {
            diagnostics_add(p->diagnostics, DIAGNOSTIC_PARSER,
                            t.line, t.column, "program uses too many registers", NULL);
            return false;
        }

        uint32_t* registers = realloc(p->registers,
                                      new_capacity * sizeof(*p->registers));
        if (!registers) {
            p->diagnostics->allocation_failed = true;
            return false;
        }
        p->registers = registers;
        p->register_capacity = new_capacity;
    }

    p->registers[p->register_count] = (uint32_t)t.num;
    *result = (uint32_t)++p->register_count;
    return true;
}

static bool parse_register(parser* p, uint32_t* result) {
    lexer_token t;
    return consume(p, TOK_NUM, &t) && normalize_register(p, t, result);
}

static bool is_instruction_start(const lexer_token_kind kind) {
    return kind == TOK_NUM || kind == TOK_FLOW_LABEL || kind == TOK_FLOW_JUMP ||
           kind == TOK_FLOW_CJUMP || kind == TOK_IO_PUTC || kind == TOK_IO_GETC ||
           kind == TOK_IO_PUTI;
}

static void synchronize(parser* p, const size_t start) {
    if (p->pos == start && current(p).kind != TOK_EOF)
        p->pos++;

    while (current(p).kind != TOK_EOF &&
           !is_instruction_start(current(p).kind))
        p->pos++;
}

static bool append_instruction(parser* p, const ir_instruction instruction) {
    if (p->instruction_count >= p->instruction_capacity) {
        p->diagnostics->allocation_failed = true;
        return false;
    }
    p->instructions[p->instruction_count++] = instruction;
    return true;
}

static bool parse_numeric_instruction(parser* p, const lexer_token leading) {
    lexer_token keyword;
    if (p->pos + 1 >= p->tokens->token_count) {
        diagnostics_add(p->diagnostics, DIAGNOSTIC_PARSER,
                        leading.line, leading.column, "expected store keyword",
                        "Follow a register address with yami, Yami, or YAMI.");
        return false;
    }
    p->pos++;
    keyword = current(p);
    if (keyword.kind != TOK_STORE_LIT && keyword.kind != TOK_STORE_COPY &&
        keyword.kind != TOK_STORE_ARITH) {
        if (keyword.kind != TOK_ERROR)
            diagnostics_add(p->diagnostics, DIAGNOSTIC_PARSER,
                            keyword.line, keyword.column, "expected store keyword",
                            "Follow a register address with yami, Yami, or YAMI.");
        return false;
    }
    p->pos++;

    uint32_t dest;
    if (leading.num < 0) {
        diagnostics_add(p->diagnostics, DIAGNOSTIC_PARSER,
                        leading.line, leading.column,
                        "register address must be non-negative",
                        "Use register 0 or a positive register address.");
        return false;
    }
    if (!normalize_register(p, leading, &dest))
        return false;
    if (keyword.kind == TOK_STORE_LIT) {
        lexer_token literal;
        if (!consume(p, TOK_NUM, &literal))
            return false;
        return append_instruction(p, (ir_instruction){
            .type = INIT_REGISTER,
            .init_register = { .dest = dest, .literal = literal.num },
        });
    }

    uint32_t src1;
    if (!parse_register(p, &src1))
        return false;

    if (keyword.kind == TOK_STORE_COPY) {
        return append_instruction(p, (ir_instruction){
            .type = COPY_REGISTER,
            .copy_register = { .src = src1, .dest = dest },
        });
    }

    lexer_token op;
    if (!consume(p, TOK_OP, &op))
        return false;
    uint32_t src2;
    if (!parse_register(p, &src2))
        return false;

    ir_operation operation;
    switch (op.op) {
        case '+': operation = ADDITION; break;
        case '-': operation = SUBTRACTION; break;
        case '*': operation = MULTIPLICATION; break;
        case '/': operation = DIVISION; break;
        case '%': operation = MODULUS; break;
        default:
            diagnostics_add(p->diagnostics, DIAGNOSTIC_PARSER,
                            op.line, op.column, "unknown operator",
                            "Use one of: +, -, *, /, %.");
            return false;
    }

    return append_instruction(p, (ir_instruction){
        .type = CALC_REGISTER,
        .calc_register = {
            .src1 = src1, .src2 = src2, .dest = dest, .op = operation,
        },
    });
}

static bool add_label(parser* p, const lexer_token name) {
    for (size_t i = 0; i < p->label_count; ++i) {
        if (label_eq(p->labels[i].name, p->labels[i].len, name.text, name.len)) {
            char message[192];
            snprintf(message, sizeof(message), "duplicate label %.*s",
                     (int)name.len, name.text);
            diagnostics_add(p->diagnostics, DIAGNOSTIC_LABEL,
                            name.line, name.column, message,
                            "Rename this label or remove the earlier definition.");
            return false;
        }
    }
    if (p->label_count >= p->label_capacity) {
        p->diagnostics->allocation_failed = true;
        return false;
    }
    p->labels[p->label_count++] = (label_entry){
        .name = name.text,
        .len = name.len,
        .target = p->instruction_count,
    };
    return true;
}

static bool add_patch(parser* p, const lexer_token name) {
    if (p->patch_count >= p->patch_capacity) {
        p->diagnostics->allocation_failed = true;
        return false;
    }
    p->patches[p->patch_count++] = (jump_patch){
        .name = name.text,
        .len = name.len,
        .instruction = p->instruction_count,
        .line = name.line,
        .column = name.column,
    };
    return true;
}

static bool parse_statement(parser* p) {
    const lexer_token first = current(p);
    if (first.kind == TOK_NUM)
        return parse_numeric_instruction(p, first);

    if (first.kind == TOK_FLOW_LABEL) {
        p->pos++;
        lexer_token name;
        if (!consume(p, TOK_LABEL, &name))
            return false;
        add_label(p, name);
        return true;
    }

    if (first.kind == TOK_FLOW_JUMP || first.kind == TOK_FLOW_CJUMP) {
        p->pos++;
        lexer_token name;
        if (!consume(p, TOK_LABEL, &name))
            return false;

        uint32_t condition = 0;
        if (first.kind == TOK_FLOW_CJUMP && !parse_register(p, &condition))
            return false;

        if (!add_patch(p, name))
            return false;
        return append_instruction(p, (ir_instruction){
            .type = first.kind == TOK_FLOW_JUMP ? JMP : COND_JMP,
            .jmp = { .target = 0, .cond = condition },
        });
    }

    if (first.kind == TOK_IO_GETC || first.kind == TOK_IO_PUTC ||
        first.kind == TOK_IO_PUTI) {
        p->pos++;
        uint32_t reg;
        if (!parse_register(p, &reg))
            return false;

        if (first.kind == TOK_IO_GETC)
            return append_instruction(p, (ir_instruction){
                .type = IO_IN, .io_in = { .dest = reg },
            });
        return append_instruction(p, (ir_instruction){
            .type = first.kind == TOK_IO_PUTC ? IO_OUT : IO_DOUT,
            .io_out = { .src = reg },
        });
    }

    diagnostics_add(p->diagnostics, DIAGNOSTIC_PARSER,
                    first.line, first.column, "unexpected token",
                    "Start an instruction with a register address or Kage keyword.");
    return false;
}

static void resolve_jumps(parser* p) {
    for (size_t i = 0; i < p->patch_count; ++i) {
        const jump_patch patch = p->patches[i];
        bool found = false;
        for (size_t j = 0; j < p->label_count; ++j) {
            if (label_eq(patch.name, patch.len, p->labels[j].name, p->labels[j].len)) {
                p->instructions[patch.instruction].jmp.target = p->labels[j].target;
                found = true;
                break;
            }
        }
        if (!found) {
            char message[192];
            snprintf(message, sizeof(message), "undefined label %.*s",
                     (int)patch.len, patch.name);
            diagnostics_add(p->diagnostics, DIAGNOSTIC_LABEL,
                            patch.line, patch.column, message,
                            "Define this label with hikari before jumping to it.");
        }
    }
}

static void* allocate_array(const size_t count, const size_t element_size) {
    if (count == 0)
        return NULL;
    if (count > SIZE_MAX / element_size)
        return NULL;
    return calloc(count, element_size);
}

bool parse(const lexer_result* lexer, diagnostic_list* diagnostics, ir_arr* result) {
    *result = (ir_arr){0};
    if (!lexer || lexer->token_count == 0 || !lexer->tokens) {
        diagnostics_add(diagnostics, DIAGNOSTIC_PARSER, 1, 1,
                        "token stream is empty", NULL);
        return false;
    }

    size_t instruction_capacity = 0;
    size_t label_capacity = 0;
    size_t patch_capacity = 0;
    for (size_t i = 0; i < lexer->token_count; ++i) {
        const lexer_token t = lexer->tokens[i];

        // The lexer no longer reports diagnostics itself, so forward its errors here.
        if (t.kind == TOK_ERROR)
            diagnostics_add(diagnostics, DIAGNOSTIC_LEXER,
                            t.line, t.column, t.error, NULL);

        if (t.kind == TOK_NUM || t.kind == TOK_FLOW_JUMP || t.kind == TOK_FLOW_CJUMP ||
            t.kind == TOK_IO_PUTC || t.kind == TOK_IO_GETC || t.kind == TOK_IO_PUTI)
            instruction_capacity++;
        if (t.kind == TOK_FLOW_LABEL)
            label_capacity++;
        if (t.kind == TOK_FLOW_JUMP || t.kind == TOK_FLOW_CJUMP)
            patch_capacity++;
    }

    parser p = {
        .tokens = lexer,
        .diagnostics = diagnostics,
        .instruction_capacity = instruction_capacity,
        .label_capacity = label_capacity,
        .patch_capacity = patch_capacity,
    };
    p.instructions = allocate_array(instruction_capacity, sizeof(*p.instructions));
    p.labels = allocate_array(label_capacity, sizeof(*p.labels));
    p.patches = allocate_array(patch_capacity, sizeof(*p.patches));
    if ((instruction_capacity && !p.instructions) ||
        (label_capacity && !p.labels) || (patch_capacity && !p.patches)) {
        diagnostics->allocation_failed = true;
        goto cleanup;
    }

    while (current(&p).kind != TOK_EOF && !diagnostics->allocation_failed) {
        const size_t start = p.pos;
        if (current(&p).kind == TOK_ERROR) {
            p.pos++;
            synchronize(&p, start);
            continue;
        }
        if (!parse_statement(&p))
            synchronize(&p, start);
    }

    resolve_jumps(&p);
    if (diagnostics->len == 0 && !diagnostics->allocation_failed) {
        result->instruction = p.instructions;
        result->length = p.instruction_count;
        result->register_count = p.register_count;
        p.instructions = NULL;
    }

cleanup:
    free(p.instructions);
    free(p.labels);
    free(p.patches);
    free(p.registers);
    return result->instruction != NULL || (diagnostics->len == 0 && !diagnostics->allocation_failed);
}
