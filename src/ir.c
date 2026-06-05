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

const char* ir_type_names[] = {
    [INIT_REGISTER] = "INIT_REGISTER",
    [COPY_REGISTER] = "COPY_REGISTER",
    [CALC_REGISTER] = "CALC_REGISTER",
    [JMP]           = "JMP",
    [COND_JMP]      = "COND_JMP",
    [IO_OUT]        = "IO_OUT",
    [IO_IN]         = "IO_IN",
    [IO_DOUT]       = "IO_DOUT",
};

const char* op_names[] = {
    [ADDITION]       = "+",
    [SUBTRACTION]    = "-",
    [MULTIPLICATION] = "*",
    [DIVISION]       = "/",
    [MODULUS]        = "%",
};

void ir_dump(const ir_arr* arr) {
    printf("\n=== IR DUMP ===\n");
    printf("%-6s  %-14s  %s\n", "INDEX", "TYPE", "DATA");
    printf("------  --------------  ----\n");

    for (size_t i = 0; i < arr->length; ++i) {
        const ir_instruction* inst = &arr->instruction[i];

        printf("%04zu    %-14s  ", i, ir_type_names[inst->type]);

        switch (inst->type) {
            case INIT_REGISTER:
                printf("dest=r%u  val=%d",
                       inst->init_register.dest,
                       inst->init_register.literal);
                break;
            case COPY_REGISTER:
                printf("dest=r%u  src=r%u",
                       inst->copy_register.dest,
                       inst->copy_register.src);
                break;
            case CALC_REGISTER:
                printf("dest=r%u  src1=r%u  op=%s  src2=r%u",
                       inst->calc_register.dest,
                       inst->calc_register.src1,
                       op_names[inst->calc_register.op],
                       inst->calc_register.src2);
                break;
            case JMP:
                printf("target=%04zu",
                       inst->jmp.target);
                break;
            case COND_JMP:
                printf("cond=r%u  target=%04zu",
                       inst->jmp.cond,
                       inst->jmp.target);
                break;
            case IO_OUT:
                printf("src=r%u",
                    inst->io_out.src);
                if (inst->io_out.src == 0)
                    printf("  [HALT]");
                break;
            case IO_DOUT:
                printf("src=r%u",
                       inst->io_out.src);
                break;
            case IO_IN:
                printf("dest=r%u",
                       inst->io_in.dest);
                break;
            default:
                printf("UNKNOWN");
                break;
        }

        printf("\n");
    }

    printf("=== END IR ===\n\n");
}
