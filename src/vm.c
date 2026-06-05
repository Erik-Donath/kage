//
// Created by erikd on 05.06.26.
//

#include <vm.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct {
    const ir_arr* ir;
    size_t pos;
    int32_t* regs;
} vm_state;

inline static int32_t run_operator(const int32_t src1, const int32_t src2, const ir_operation op) {
    switch (op) {
        case ADDITION:
            return src1 + src2;
        case SUBTRACTION:
            return src1 - src2;
        case MULTIPLICATION:
            return src1 * src2;
        case DIVISION:
            if (src2 == 0)
                return 0;
            return src1 / src2;
        case MODULUS:
            if (src2 == 0)
                return 0;
            return src1 % src2;
    }
    fprintf(stderr, "unknown operator %d\n", op);
    exit(EXIT_FAILURE);
}

inline static int32_t set_register(const vm_state* state, const size_t reg, const int32_t value) {
    if (reg == 0)
        return 0;
    return (state->regs[reg - 1] = value);
}

inline static int32_t get_register(const vm_state* state, const size_t reg) {
    if (reg == 0)
        return 0;
    return state->regs[reg - 1];
}

void run_vm(const ir_arr* ir) {
    vm_state vm = (vm_state) {
        .ir = ir,
        .pos = 0,
        .regs = calloc(ir->register_count, sizeof(int32_t)),
    };

    while (vm.pos < vm.ir->length) {
        const ir_instruction inst = vm.ir->instruction[vm.pos];
        switch (inst.type) {
            case INIT_REGISTER: {
                const int32_t value = inst.init_register.literal;
                set_register(&vm, inst.init_register.dest, value);
            } break;
            case COPY_REGISTER: {
                const int32_t value = get_register(&vm, inst.copy_register.src);
                set_register(&vm, inst.copy_register.dest, value);
            } break;
            case CALC_REGISTER: {
                const int32_t value1 = get_register(&vm, inst.calc_register.src1);
                const int32_t value2 = get_register(&vm, inst.calc_register.src2);
                set_register(&vm, inst.calc_register.dest, run_operator(value1, value2, inst.calc_register.op));
            } break;
            case JMP:
                vm.pos = inst.jmp.target;
                continue;
            case COND_JMP:
                if (get_register(&vm, inst.jmp.cond) != 0) {
                    vm.pos = inst.jmp.target;
                    continue;
                }
                break;
            case IO_OUT: {
                const int32_t c = get_register(&vm, inst.io_out.src);
                if (c == 0)
                    vm.pos = vm.ir->length;
                else
                    printf("%c", c);
            } break;
            case IO_DOUT:
                printf("%d", get_register(&vm, inst.io_out.src));
                break;
            case IO_IN: {
                const int c = getchar();
                set_register(&vm, inst.io_in.dest, c == EOF ? 0 : (int32_t)c);
            } break;

        }
        vm.pos++;
    }

    free(vm.regs);
}
