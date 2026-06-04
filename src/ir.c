//
// Created by erikd on 05.06.26.
//

#include <ir.h>
#include <stdlib.h>
#include <stdio.h>

void ir_arr_init(ir_arr* arr, const size_t length) {
    arr->instruction = malloc(sizeof(ir_instruction) * length);
    arr->length = length;
}

void ir_arr_free(ir_arr* arr) {
    free(arr->instruction);
    arr->length = 0;
}

static const char* ir_type_names[] = {
    [INIT_REGISTER] = "INIT_REGISTER",
    [COPY_REGISTER] = "COPY_REGISTER",
    [CALC_REGISTER] = "CALC_REGISTER",
    [JMP]           = "JMP",
    [COND_JMP]      = "COND_JMP",
    [IO_OUT]        = "IO_OUT",
    [IO_IN]         = "IO_IN",
    [IO_DOUT]       = "IO_DOUT",
};

static const char* op_names[] = {
    [ADDITION]       = "+",
    [SUBTRACTION]    = "-",
    [MULTIPLICATION] = "*",
    [DIVISION]       = "/",
    [MODULUS]        = "%",
};

void ir_dump(const ir_arr* arr) {
    printf("\n=== IR DUMP ===\n");

    for (size_t i = 0; i < arr->length; ++i) {
        const ir_instruction* inst = &arr->instruction[i];
        printf("%04zu | ", i);

        switch (inst->type) {
            case INIT_REGISTER:
                printf("%s r%u <- %d\n",
                       ir_type_names[inst->type],
                       inst->init_register.dest,
                       inst->init_register.literal);
                break;
            case COPY_REGISTER:
                printf("%s r%u <- r%u\n",
                       ir_type_names[inst->type],
                       inst->copy_register.dest,
                       inst->copy_register.src);
                break;
            case CALC_REGISTER:
                printf("%s r%u <- r%u %s r%u\n",
                       ir_type_names[inst->type],
                       inst->calc_register.dest,
                       inst->calc_register.src1,
                       op_names[inst->calc_register.op],
                       inst->calc_register.src2);
                break;
            case JMP:
                printf("%s -> %zu\n",
                       ir_type_names[inst->type],
                       inst->jmp.target);
                break;
            case COND_JMP:
                printf("%s r%u -> %zu\n",
                       ir_type_names[inst->type],
                       inst->jmp.cond,
                       inst->jmp.target);
                break;
            case IO_OUT:
            case IO_DOUT:
                printf("%s r%u\n",
                       ir_type_names[inst->type],
                       inst->io_out.src);
                break;
            case IO_IN:
                printf("%s r%u\n",
                       ir_type_names[inst->type],
                       inst->io_in.dest);
                break;
            default:
                printf("UNKNOWN\n");
                break;
        }
    }

    printf("=== END IR ===\n\n");
}
