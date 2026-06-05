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
    size_t target_index;
} label_entry;

typedef struct {
    const char* label;
    size_t patch_index;
} patch_entry;

static token expect(const token_vec* vec, size_t* i, const token_type expected) {
    (*i)++;
    if (*i >= vec->len) {
        fprintf(stderr, "ParseError: unexpected end of input\n");
        exit(EXIT_FAILURE);
    }

    const token t = vec->tokens[*i];
    if (expected != 0 && t.type != expected) {
        fprintf(stderr, "ParseError (%d:%d): expected %s(%d) but got %s(%d)\n",
            t.line, t.column, token_names[expected], expected, token_names[t.type], t.type);
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
    ir_instruction* inst = malloc(sizeof(ir_instruction) * inst_count);
    label_entry* labels = malloc(sizeof(label_entry) * label_count);
    patch_entry* patches = malloc(sizeof(patch_entry) * patch_count);

    size_t ir_idx = 0;
    size_t label_idx = 0;
    size_t patch_idx = 0;

    #define emit(x) (inst[ir_idx++] = (x))
    #define emit_label(x) (labels[label_idx++]  = (label_entry){ .label = (x), .target_index = ir_idx })
    #define emit_patch(x) (patches[patch_idx++] = (patch_entry){ .label = (x), .patch_index  = ir_idx })

    for (size_t i = 0; i < vec->len; ++i) {
        const token t = vec->tokens[i];
        const token_type T = t.type;

        if (T == EOF_TOKEN)
            break;

        if (T == NUM) {
            uint32_t dest = t.reg;
            const token kw = expect(vec, &i, NONE);

            if (kw.type == STORE_LIT) {
                const token val = expect(vec, &i, NUM);
                const ir_instruction instruction = (ir_instruction){
                    .type = INIT_REGISTER,
                    .init_register.dest = dest,
                    .init_register.literal = val.num,
                };

                emit(instruction);
                continue;
            }
            if (kw.type == STORE_CPY) {
                const token src = expect(vec, &i, NUM);
                const ir_instruction instruction = (ir_instruction){
                    .type = COPY_REGISTER,
                    .copy_register.src = src.reg,
                    .copy_register.dest = dest,
                };

                emit(instruction);
                continue;
            }
            if (kw.type == STORE_EXP) {
                const token src1 = expect(vec, &i, NUM);
                const token op   = expect(vec, &i, OP);
                const token src2 = expect(vec, &i, NUM);

                const ir_instruction instruction = (ir_instruction){
                    .type = CALC_REGISTER,
                    .calc_register.src1 = src1.reg,
                    .calc_register.src2 = src2.reg,
                    .calc_register.dest = dest,
                    .calc_register.op = translate_operation(op.op),
                };
                emit(instruction);
                continue;
            }

            fprintf(stderr, "ParseErro(%d:%d): expected store keyword\n", kw.line, kw.column);
            exit(EXIT_FAILURE);
        }

        if (T == FLOW_LAB) {
            const token label = expect(vec, &i, LABEL);
            emit_label(label.str);
            continue;
        }

        if (T == FLOW_JMP) {
            const token label = expect(vec, &i, LABEL);
            const ir_instruction instruction = (ir_instruction){
                .type = JMP,
                .jmp.target = 0,
            };

            emit_patch(label.str);
            emit(instruction);
            continue;
        }

        if (T == FLOW_COND_JMP) {
            const token label = expect(vec, &i, LABEL);
            const token cond = expect(vec, &i, NUM);
            const ir_instruction instruction = (ir_instruction){
                .type = COND_JMP,
                .jmp.target = 0,
                .jmp.cond = cond.reg,
            };
            emit_patch(label.str);
            emit(instruction);
            continue;
        }

        if (T == IO_READ) {
            const token dst = expect(vec, &i, NUM);
            const ir_instruction instruction = (ir_instruction){
                .type = IO_IN,
                .io_in.dest = dst.reg,
            };
            emit(instruction);
            continue;
        }
        if (T == IO_PRINT || T == IO_DEC_PRINT) {
            const token src = expect(vec, &i, NUM);
            const ir_instruction instruction = (ir_instruction){
                .type = (T == IO_PRINT) ? IO_OUT : IO_DOUT,
                .io_out.src = src.reg,
            };
            emit(instruction);
            continue;
        }

        fprintf(stderr, "ParseError (%d:%d): unexpected token\n", t.line, t.column);
        exit(EXIT_FAILURE);
    }

    #undef emit
    #undef emit_label
    #undef emit_patch

    // Pass 3: Backpatching
    for (size_t p = 0; p < patch_count; ++p) {
        bool found = false;
        const patch_entry patch = patches[p];
        for (size_t l = 0; l < label_count; ++l) {
            const label_entry label = labels[l];
            if (strcmp(patch.label, label.label) == 0) {
                inst[patch.patch_index].jmp.target = label.target_index;
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
    free(labels);
    free(patches);

    return (ir_arr){
        .instruction = inst,
        .length = inst_count,
    };
}
