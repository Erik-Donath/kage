//
// Created by erikd on 04.06.26.
//

#include "parser.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char* name;
    size_t target;
} label_entry;

typedef struct {
    const char* name;
    size_t instruction;
    int line;
    int column;
} jump_patch;

typedef struct {
    const token_vec* tokens;
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

static token current(const parser* p) {
    return p->tokens->tokens[p->pos];
}

static const char* expected_help(const token_type type) {
    switch (type) {
        case NUM:
            return "Add a number or register address here (for example, 1).";
        case OP:
            return "Insert one of: +, -, *, /, %.";
        case LABEL:
            return "Define a label with hikari <name> or correct the jump target.";
        default:
            return "Check the instruction spelling and operand order.";
    }
}

static bool consume(parser* p, const token_type expected, token* result) {
    const token t = current(p);
    if (t.type != expected) {
        if (t.type != INVALID) {
            char message[192];
            snprintf(message, sizeof(message), "expected %s(%d) but got %s(%d)",
                     token_names[expected], expected, token_names[t.type], t.type);
            diagnostics_add(p->diagnostics, DIAGNOSTIC_PARSER,
                            t.line, t.column, message, expected_help(expected));
        }
        return false;
    }

    *result = t;
    if (p->pos + 1 < p->tokens->len)
        p->pos++;
    return true;
}

static bool normalize_register(parser* p, const token t, uint32_t* result) {
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
    token t;
    return consume(p, NUM, &t) && normalize_register(p, t, result);
}

static bool is_instruction_start(const token_type type) {
    return type == NUM || type == FLOW_LAB || type == FLOW_JMP ||
           type == FLOW_COND_JMP || type == IO_PRINT || type == IO_READ ||
           type == IO_DEC_PRINT;
}

static void synchronize(parser* p, const size_t start) {
    if (p->pos == start && current(p).type != EOF_TOKEN)
        p->pos++;

    while (current(p).type != EOF_TOKEN &&
           !is_instruction_start(current(p).type))
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

static bool parse_numeric_instruction(parser* p, const token leading) {
    token keyword;
    if (p->pos + 1 >= p->tokens->len) {
        diagnostics_add(p->diagnostics, DIAGNOSTIC_PARSER,
                        leading.line, leading.column, "expected store keyword",
                        "Follow a register address with yami, Yami, or YAMI.");
        return false;
    }
    p->pos++;
    keyword = current(p);
    if (keyword.type != STORE_LIT && keyword.type != STORE_CPY &&
        keyword.type != STORE_EXP) {
        if (keyword.type != INVALID)
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
    if (keyword.type == STORE_LIT) {
        token literal;
        if (!consume(p, NUM, &literal))
            return false;
        return append_instruction(p, (ir_instruction){
            .type = INIT_REGISTER,
            .init_register = { .dest = dest, .literal = literal.num },
        });
    }

    uint32_t src1;
    if (!parse_register(p, &src1))
        return false;

    if (keyword.type == STORE_CPY) {
        return append_instruction(p, (ir_instruction){
            .type = COPY_REGISTER,
            .copy_register = { .src = src1, .dest = dest },
        });
    }

    token op;
    if (!consume(p, OP, &op))
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

static bool add_label(parser* p, const token name) {
    for (size_t i = 0; i < p->label_count; ++i) {
        if (strcmp(p->labels[i].name, name.str) == 0) {
            char message[192];
            snprintf(message, sizeof(message), "duplicate label %s", name.str);
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
        .name = name.str,
        .target = p->instruction_count,
    };
    return true;
}

static bool add_patch(parser* p, const token name) {
    if (p->patch_count >= p->patch_capacity) {
        p->diagnostics->allocation_failed = true;
        return false;
    }
    p->patches[p->patch_count++] = (jump_patch){
        .name = name.str,
        .instruction = p->instruction_count,
        .line = name.line,
        .column = name.column,
    };
    return true;
}

static bool parse_statement(parser* p) {
    const token first = current(p);
    if (first.type == NUM)
        return parse_numeric_instruction(p, first);

    if (first.type == FLOW_LAB) {
        p->pos++;
        token name;
        if (!consume(p, LABEL, &name))
            return false;
        add_label(p, name);
        return true;
    }

    if (first.type == FLOW_JMP || first.type == FLOW_COND_JMP) {
        p->pos++;
        token name;
        if (!consume(p, LABEL, &name))
            return false;

        uint32_t condition = 0;
        if (first.type == FLOW_COND_JMP && !parse_register(p, &condition))
            return false;

        if (!add_patch(p, name))
            return false;
        return append_instruction(p, (ir_instruction){
            .type = first.type == FLOW_JMP ? JMP : COND_JMP,
            .jmp = { .target = 0, .cond = condition },
        });
    }

    if (first.type == IO_READ || first.type == IO_PRINT ||
        first.type == IO_DEC_PRINT) {
        p->pos++;
        uint32_t reg;
        if (!parse_register(p, &reg))
            return false;

        if (first.type == IO_READ)
            return append_instruction(p, (ir_instruction){
                .type = IO_IN, .io_in = { .dest = reg },
            });
        return append_instruction(p, (ir_instruction){
            .type = first.type == IO_PRINT ? IO_OUT : IO_DOUT,
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
            if (strcmp(patch.name, p->labels[j].name) == 0) {
                p->instructions[patch.instruction].jmp.target = p->labels[j].target;
                found = true;
                break;
            }
        }
        if (!found) {
            char message[192];
            snprintf(message, sizeof(message), "undefined label %s", patch.name);
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

bool parse(const token_vec* vec, diagnostic_list* diagnostics, ir_arr* result) {
    *result = (ir_arr){0};
    if (!vec || vec->len == 0 || !vec->tokens) {
        diagnostics_add(diagnostics, DIAGNOSTIC_PARSER, 1, 1,
                        "token stream is empty", NULL);
        return false;
    }

    size_t instruction_capacity = 0;
    size_t label_capacity = 0;
    size_t patch_capacity = 0;
    for (size_t i = 0; i < vec->len; ++i) {
        const token_type type = vec->tokens[i].type;
        if (type == NUM || type == FLOW_JMP || type == FLOW_COND_JMP ||
            type == IO_PRINT || type == IO_READ || type == IO_DEC_PRINT)
            instruction_capacity++;
        if (type == FLOW_LAB)
            label_capacity++;
        if (type == FLOW_JMP || type == FLOW_COND_JMP)
            patch_capacity++;
    }

    parser p = {
        .tokens = vec,
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

    while (current(&p).type != EOF_TOKEN && !diagnostics->allocation_failed) {
        const size_t start = p.pos;
        if (current(&p).type == INVALID) {
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
