//
// Created by erikd on 04.06.26.
//

#include "parser.h"

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    const char* label;
    size_t      target_index;
} label_entry;

typedef struct {
    const char* label;
    size_t      patch_index;
} patch_entry;

typedef struct {
    uint32_t* regs;
    size_t len;
    size_t cap;
} register_patch_vec;
#define REGISTER_PATCH_VEC_INIT_CAP 256

typedef struct {
    const token_vec*    vec;
    size_t              pos;
    ir_instruction*     ir;
    size_t              ir_idx;
    label_entry*        labels;
    size_t              label_idx;
    patch_entry*        patches;
    size_t              patch_idx;
    register_patch_vec  register_vec;
} parser_state;

static void emit_ir(parser_state* p, const ir_instruction inst) {
    p->ir[p->ir_idx++] = inst;
}

static void emit_label(parser_state* p, const char* label) {
    p->labels[p->label_idx++] = (label_entry){
        .label = label,
        .target_index = p->ir_idx,
    };
}

static void emit_patch(parser_state* p, const char* patch) {
    p->patches[p->patch_idx++] = (patch_entry){
        .label = patch,
        .patch_index = p->ir_idx,
    };
}

static uint32_t patch_reg(parser_state* p, const uint32_t reg) {
    register_patch_vec* vec = &p->register_vec;

    // Do not patch zero register
    if (reg == 0) {
        return 0;
    }

    // Do not add a reg twice
    for (size_t i = 0; i < vec->len; ++i) {
        if (vec->regs[i] == reg)
            return (uint32_t)(i + 1);
    }

    if (vec->len >= vec->cap) {
        vec->cap *= 2;
        vec->regs = realloc(vec->regs, vec->cap * sizeof(vec->regs[0]));
    }
    vec->regs[vec->len++] = reg;
    return (uint32_t)vec->len;
}

static token advance(parser_state* p) {
    return p->vec->tokens[++(p->pos)];
}

static token current(parser_state* p) {
    return p->vec->tokens[p->pos];
}

static token expect(parser_state* p, const token_type expected) {
    const token t = advance(p);
    if (p->pos >= p->vec->len) {
        fprintf(stderr, "Parse error: unexpected end of input\n");
        exit(EXIT_FAILURE);
    }

    if (t.type != expected) {
        fprintf(stderr, "ParseError (%d:%d): expected %s(%d) but got %s(%d)\n",
            t.line, t.column,
            token_names[expected], expected,
            token_names[t.type], t.type
        );
        exit(EXIT_FAILURE);
    }

    return t;
}

static ir_operation translate_operation(const char ch) {
    switch (ch) {
        case '+':
            return ADDITION;
        case '-':
            return SUBTRACTION;
        case '*':
            return MULTIPLICATION;
        case '/':
            return DIVISION;
        case '%':
            return MODULUS;
        default:
            break;
    }

    fprintf(stderr, "ParseError: unknown operator %c\n", ch);
    exit(EXIT_FAILURE);
}

static void parse_num(parser_state *p, const token t) {
    const uint32_t dest = t.reg;
    const token kw = advance(p);

    switch (kw.type) {
        case STORE_LIT: {
            const token val = expect(p, NUM);
            emit_ir(p, (ir_instruction){
                .type = INIT_REGISTER,
                .init_register.dest = patch_reg(p, dest),
                .init_register.literal = val.num,
            });
        } break;
        case STORE_CPY: {
            const token src = expect(p, NUM);
            emit_ir(p, (ir_instruction){
                .type = COPY_REGISTER,
                .copy_register.dest = patch_reg(p, dest),
                .copy_register.src = patch_reg(p, src.reg),
            });
        } break;
        case STORE_EXP: {
            const token src1 = expect(p, NUM);
            const token op   = expect(p, OP);
            const token src2 = expect(p, NUM);

            emit_ir(p, (ir_instruction){
                .type = CALC_REGISTER,
                .calc_register.dest = patch_reg(p, dest),
                .calc_register.src1 = patch_reg(p, src1.reg),
                .calc_register.src2 = patch_reg(p, src2.reg),
                .calc_register.op = translate_operation(op.op),
            });
        } break;
        default:
            fprintf(stderr, "Parse error(%d:%d): expected store keyword but got %s(%d)\n",
                kw.line, kw.column,
                token_names[kw.type], kw.type
            );
            exit(EXIT_FAILURE);
    }
}

ir_arr parse(const token_vec* vec) {
    // Pass 1: Pre-Scan
    size_t inst_count = 0;
    size_t label_count = 0;
    size_t patch_count = 0;

    for (size_t i = 0; i < vec->len; ++i) {
        if (vec->tokens[i].type < 100)
            inst_count++;
        if (vec->tokens[i].type == FLOW_LAB)
            label_count++;
        if (vec->tokens[i].type == FLOW_JMP ||
            vec->tokens[i].type == FLOW_COND_JMP)
            patch_count++;
    }

    // Pass 2: Code generation
    parser_state p = (parser_state){
        .vec = vec,
        .pos = 0,
        .ir = malloc(sizeof(ir_instruction) * inst_count),
        .ir_idx = 0,
        .labels = malloc(sizeof(label_entry) * label_count),
        .label_idx = 0,
        .patches = malloc(sizeof(patch_entry) * patch_count),
        .patch_idx = 0,
        .register_vec.len = 0,
        .register_vec.cap = REGISTER_PATCH_VEC_INIT_CAP,
        .register_vec.regs = malloc(sizeof(uint32_t) * REGISTER_PATCH_VEC_INIT_CAP),
    };

    while (p.pos < p.vec->len) {
        const token t = current(&p);
        switch (t.type) {
            case NUM:
                parse_num(&p, t);
                break;
            case FLOW_LAB: {
                const token label = expect(&p, LABEL);
                emit_label(&p, label.str);
            } break;
            case FLOW_JMP: {
                const token label = expect(&p, LABEL);

                emit_patch(&p, label.str);
                emit_ir(&p, (ir_instruction){
                    .type = JMP,
                    .jmp.target = 0,
                });
            } break;
            case FLOW_COND_JMP: {
                const token label = expect(&p, LABEL);
                const token cond  = expect(&p, NUM);

                emit_patch(&p, label.str);
                emit_ir(&p, (ir_instruction){
                    .type = COND_JMP,
                    .jmp.target = 0,
                    .jmp.cond = patch_reg(&p, cond.reg),
                });
            } break;
            case IO_READ: {
                const token dst = expect(&p, NUM);
                emit_ir(&p, (ir_instruction){
                    .type = IO_IN,
                    .io_in.dest = patch_reg(&p, dst.reg),
                });
            } break;
            case IO_PRINT: {
                const token src = expect(&p, NUM);
                emit_ir(&p, (ir_instruction){
                    .type = IO_OUT,
                    .io_out.src = patch_reg(&p, src.reg),
                });
            } break;
            case IO_DEC_PRINT: {
                const token src = expect(&p, NUM);
                emit_ir(&p, (ir_instruction){
                    .type = IO_DOUT,
                    .io_out.src = patch_reg(&p, src.reg),
                });
            } break;
            case EOF_TOKEN:
                break;
            default:
                fprintf(stderr, "Parse error: unexpected token type %d\n", t.type);
                exit(EXIT_FAILURE);
        }

        advance(&p);
    }

    // Pass 3: Backpatching
    for (size_t pi = 0; pi < patch_count; ++pi) {
        bool found = false;
        const patch_entry patch = p.patches[pi];
        for (size_t li = 0; li < label_count; ++li) {
            const label_entry label = p.labels[li];
            if (strcmp(patch.label, label.label) == 0) {
                p.ir[patch.patch_index].jmp.target = label.target_index;
                found = true;
                break;
            }
        }

        if (!found) {
            fprintf(stderr, "LabelError: undefined label %s\n", patch.label);
            exit(EXIT_FAILURE);
        }
    }

    // Return
    free(p.labels);
    free(p.patches);
    free(p.register_vec.regs);
    return (ir_arr){
        .instruction = p.ir,
        .length = inst_count,
        .register_count = p.register_vec.len,
    };
}
