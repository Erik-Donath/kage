//
// Created by erikd on 05.06.26.
//

#include "vm.h"

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef bool (*read_fn)(void* ctx, int32_t* value);
typedef bool (*write_fn)(void* ctx, int32_t value);

typedef struct {
    const ir_arr* ir;
    size_t pos;
    size_t steps;
    int32_t* regs;

    read_fn read;
    void* read_ctx;
    write_fn write;
    void* write_ctx;
} vm_state;

static bool read_stdin(const void * ctx, int32_t* value) {
    (void)ctx;
    const int c = getchar();
    if (c == EOF) {
        if (ferror(stdin))
            return false;
        *value = 0;
        return true;
    }

    *value = (unsigned char)c;
    return true;
}

static bool write_stdout(const void * ctx, const int32_t value) {
    (void)ctx;
    return putchar((unsigned char)value) != EOF;
}

static bool read_file(void* ctx, int32_t* value) {
    FILE* file = ctx;
    const int c = fgetc(file);
    if (c == EOF) {
        if (ferror(file))
            return false;
        *value = 0;
        return true;
    }

    *value = (unsigned char)c;
    return true;
}

static bool write_file(void* ctx, const int32_t value) {
    FILE* file = ctx;
    return fputc((unsigned char)value, file) != EOF;
}

static bool write_number(const vm_state* vm, const int32_t value) {
    int64_t magnitude = value;
    if (value < 0) {
        if (!vm->write(vm->write_ctx, '-'))
            return false;
        magnitude = -magnitude;
    }

    if (magnitude == 0)
        return vm->write(vm->write_ctx, '0');

    char buf[10];
    size_t i = 0;
    while (magnitude > 0) {
        buf[i++] = (char)('0' + magnitude % 10);
        magnitude /= 10;
    }

    while (i > 0) {
        if (!vm->write(vm->write_ctx, buf[--i]))
            return false;
    }
    return true;
}

static void set_register(const vm_state* state, const size_t reg, const int32_t value) {
    if (reg != 0)
        state->regs[reg - 1] = value;
}

static int32_t get_register(const vm_state* state, const size_t reg) {
    return reg == 0 ? 0 : state->regs[reg - 1];
}

static int32_t wrap_signed(uint32_t bits) {
    if (bits <= INT32_MAX)
        return (int32_t)bits;
    return (int32_t)((int64_t)bits - INT64_C(4294967296));
}

static bool calculate(const int32_t a, const int32_t b, const ir_operation op,
                      int32_t* result) {
    switch (op) {
        case ADDITION:
            *result = wrap_signed((uint32_t)a + (uint32_t)b);
            return true;
        case SUBTRACTION:
            *result = wrap_signed((uint32_t)a - (uint32_t)b);
            return true;
        case MULTIPLICATION:
            *result = wrap_signed((uint32_t)a * (uint32_t)b);
            return true;
        case DIVISION:
            *result = b == 0 ? 0 : (a == INT32_MIN && b == -1 ? INT32_MIN : a / b);
            return true;
        case MODULUS:
            *result = b == 0 || (a == INT32_MIN && b == -1) ? 0 : a % b;
            return true;
    }

    return false;
}

static bool execute(vm_state* vm, const ir_instruction inst) {
    switch (inst.type) {
        case INIT_REGISTER:
            set_register(vm, inst.init_register.dest, inst.init_register.literal);
            break;
        case COPY_REGISTER:
            set_register(vm, inst.copy_register.dest,
                         get_register(vm, inst.copy_register.src));
            break;
        case CALC_REGISTER: {
            int32_t result;
            if (!calculate(get_register(vm, inst.calc_register.src1),
                           get_register(vm, inst.calc_register.src2),
                           inst.calc_register.op, &result)) {
                fprintf(stderr, "RuntimeError: invalid arithmetic operation\n");
                return false;
            }
            set_register(vm, inst.calc_register.dest, result);
        } break;
        case JMP:
            vm->pos = inst.jmp.target;
            return true;
        case COND_JMP:
            if (get_register(vm, inst.jmp.cond) != 0) {
                vm->pos = inst.jmp.target;
                return true;
            }
            break;
        case IO_OUT: {
            const int32_t value = get_register(vm, inst.io_out.src);
            if (value == 0) {
                vm->pos = vm->ir->length;
                return true;
            }
            if (!vm->write(vm->write_ctx, value)) {
                fprintf(stderr, "RuntimeError: output write failed\n");
                return false;
            }
        } break;
        case IO_DOUT:
            if (!write_number(vm, get_register(vm, inst.io_out.src))) {
                fprintf(stderr, "RuntimeError: output write failed\n");
                return false;
            }
            break;
        case IO_IN: {
            int32_t value;
            if (!vm->read(vm->read_ctx, &value)) {
                fprintf(stderr, "RuntimeError: input read failed\n");
                return false;
            }
            set_register(vm, inst.io_in.dest, value);
        } break;
        default:
            fprintf(stderr, "RuntimeError: invalid instruction\n");
            return false;
    }

    vm->pos++;
    return true;
}

int run_vm(const ir_arr* ir, const args* a) {
    bool success = true;
    FILE* input_file = NULL;
    FILE* output_file = NULL;
    int32_t* regs = NULL;

    if (a->flags & INPUT) {
        input_file = fopen(a->input, "rb");
        if (!input_file) {
            fprintf(stderr, "Error: cannot open input file %s: %s\n",
                    a->input, strerror(errno));
            success = false;
            goto cleanup;
        }
    }

    if ((a->flags & INPUT) && (a->flags & OUTPUT) &&
        strcmp(a->input, a->output) == 0) {
        fprintf(stderr, "Error: input and output paths must be different\n");
        success = false;
        goto cleanup;
    }

    if (a->flags & OUTPUT) {
        output_file = fopen(a->output, "wb");
        if (!output_file) {
            fprintf(stderr, "Error: cannot open output file %s: %s\n",
                    a->output, strerror(errno));
            success = false;
            goto cleanup;
        }
    }

    if (ir->register_count > SIZE_MAX / sizeof(*regs)) {
        fprintf(stderr, "RuntimeError: program uses too many registers\n");
        success = false;
        goto cleanup;
    }

    regs = calloc(ir->register_count == 0 ? 1 : ir->register_count, sizeof(*regs));
    if (!regs) {
        fprintf(stderr, "RuntimeError: out of memory\n");
        success = false;
        goto cleanup;
    }

    vm_state vm = {
        .ir = ir,
        .pos = 0,
        .steps = 0,
        .regs = regs,
        .read = input_file ? read_file : read_stdin,
        .read_ctx = input_file,
        .write = output_file ? write_file : write_stdout,
        .write_ctx = output_file,
    };

    while (vm.pos < vm.ir->length) {
        if (a->max_steps != 0 && vm.steps >= a->max_steps) {
            fprintf(stderr, "RuntimeError: maximum instruction count (%zu) reached\n",
                    a->max_steps);
            success = false;
            break;
        }

        const ir_instruction inst = vm.ir->instruction[vm.pos];
        vm.steps++;
        if (!execute(&vm, inst)) {
            success = false;
            break;
        }
    }

    if (output_file) {
        if (fclose(output_file) != 0) {
            fprintf(stderr, "RuntimeError: failed to close output file %s\n", a->output);
            success = false;
        }
        output_file = NULL;
    } else if (fflush(stdout) == EOF) {
        fprintf(stderr, "RuntimeError: failed to flush standard output\n");
        success = false;
    }

cleanup:
    free(regs);
    if (input_file && fclose(input_file) != 0) {
        fprintf(stderr, "RuntimeError: failed to close input file %s\n", a->input);
        success = false;
    }
    if (output_file)
        fclose(output_file);
    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
