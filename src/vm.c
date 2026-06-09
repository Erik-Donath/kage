//
// Created by erikd on 05.06.26.
//

#include "vm.h"
#include <stdlib.h>
#include <stdio.h>

typedef int32_t (*read_fn)(const void* ctx);
typedef void (*write_fn)(const void* ctx, int32_t value);

typedef struct {
    const ir_arr* ir;
    size_t        pos;
    size_t        steps;
    int32_t*      regs;

    read_fn       read;
    void*         read_ctx;
    write_fn      write;
    void*         write_ctx;
} vm_state;

static int32_t read_stdin(const void* ctx) {
    (void)ctx;
    const int c = getchar();
    return c == EOF ? 0 : (int32_t)c;
}

static void write_stdout(const void* ctx, const int32_t value) {
    (void)ctx;
    putchar(value);
}

static int32_t read_file(const void* ctx) {
    FILE* f = (FILE*)ctx;
    const int c = fgetc(f);
    return c == EOF ? 0 : (int32_t)c;
}

static void write_file(const void* ctx, const int32_t value) {
    FILE* f = (FILE*)ctx;
    fputc((int)value, f);
}

static void write_number(const vm_state* vm, int32_t value) {
    if (value < 0) {
        vm->write(vm->write_ctx, '-');
        value = -value;
    }

    if (value == 0) {
        vm->write(vm->write_ctx, '0');
        return;
    }

    char buf[20];
    size_t i = 0;

    while (value) {
        buf[i++] = (char)('0' + value % 10);
        value /= 10;
    }

    while (i-- > 0)
        vm->write(vm->write_ctx, buf[i]);
}

static int32_t set(const vm_state* state, const size_t reg, const int32_t value) {
    if (reg == 0) return 0;
    return state->regs[reg - 1] = value;
}

static int32_t get(const vm_state* state, const size_t reg) {
    if (reg == 0) return 0;
    return state->regs[reg - 1];
}

static int32_t calc(const int32_t a, const int32_t b, const ir_operation op) {
    switch (op) {
        case ADDITION:          return a + b;
        case SUBTRACTION:       return a - b;
        case MULTIPLICATION:    return a * b;
        case DIVISION:          return b == 0 ? 0 : a / b;
        case MODULUS:           return b == 0 ? 0 : a % b;
    }

    fprintf(stderr, "unknown operator %d\n", op);
    exit(EXIT_FAILURE);
}

static void exec(vm_state* vm) {
    vm->steps++;

    const ir_instruction inst = vm->ir->instruction[vm->pos];
    switch (inst.type) {
        case INIT_REGISTER: {
            const int32_t value = inst.init_register.literal;
            set(vm, inst.init_register.dest, value);
        } break;
        case COPY_REGISTER: {
            const int32_t value = get(vm, inst.copy_register.src);
            set(vm, inst.copy_register.dest, value);
        } break;
        case CALC_REGISTER: {
            const int32_t value1 = get(vm, inst.calc_register.src1);
            const int32_t value2 = get(vm, inst.calc_register.src2);
            set(vm, inst.calc_register.dest, calc(value1, value2, inst.calc_register.op));
        } break;
        case JMP: {
            vm->pos = inst.jmp.target;
            return;
        }
        case COND_JMP: {
            if (get(vm, inst.jmp.cond) != 0) {
                vm->pos = inst.jmp.target;
                return;
            }
        } break;
        case IO_OUT: {
            const int32_t c = get(vm, inst.io_out.src);
            if (c == 0) {
                vm->pos = vm->ir->length;
                return;
            }
            vm->write(vm->write_ctx, c);
        } break;
        case IO_DOUT: {
            const int32_t val = get(vm, inst.io_out.src);
            write_number(vm, val);
        } break;
        case IO_IN: {
            const int32_t c = vm->read(vm->read_ctx);
            set(vm, inst.io_in.dest, c);
        } break;
    }

    vm->pos++;
}

void run_vm(const ir_arr* ir, const args* a) {
    FILE* input_file = NULL;
    FILE* output_file = NULL;

    if (a->flags & INPUT) {
        input_file = fopen(a->input, "rb");
        if (!input_file) {
            fprintf(stderr, "Error: cannot open input file %s\n", a->input);
            return;
        }
    }

    if (a->flags & OUTPUT) {
        output_file = fopen(a->output, "wb");
        if (!output_file) {
            fprintf(stderr, "Error: cannot open output file %s\n", a->output);
            return;
        }
    }

    vm_state vm = (vm_state) {
        .ir = ir,
        .pos = 0,
        .steps = 0,
        .regs = calloc(ir->register_count, sizeof(int32_t)),
        .read = a->flags & INPUT ? read_file : read_stdin,
        .read_ctx = (void*)input_file,
        .write = a->flags & OUTPUT ? write_file : write_stdout,
        .write_ctx = (void*)output_file,
    };

    while (vm.pos < vm.ir->length && !(a->flags & MAX_STEPS && vm.steps >= a->max_steps))
        exec(&vm);

    free(vm.regs);
    if (input_file) fclose(input_file);
    if (output_file) fclose(output_file);
}
