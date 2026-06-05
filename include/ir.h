//
// Created by erikd on 04.06.26.
//

#pragma once
#include <stddef.h>
#include <stdint.h>

typedef enum {
    INIT_REGISTER,
    COPY_REGISTER,
    CALC_REGISTER,

    // Branches use the index of the instruction to jump to
    JMP,
    COND_JMP,

    IO_OUT,
    IO_IN,
    IO_DOUT,
} ir_type;

typedef enum {
    ADDITION,
    SUBTRACTION,
    MULTIPLICATION,
    DIVISION,
    MODULUS,
} ir_operation;

typedef struct {
    ir_type type;

    union {
        struct {
            uint32_t dest;
            int32_t literal;
        } init_register;
        struct {
            uint32_t src;
            uint32_t dest;
        } copy_register;
        struct {
            uint32_t src1;
            uint32_t src2;
            uint32_t dest;
            ir_operation op;
        } calc_register;
        struct {
            size_t target;
            uint32_t cond;
        } jmp;
        struct {
            uint32_t src;
        } io_out; // Used by IO_OUT and IO_DOUT
        struct {
            uint32_t dest;
        } io_in;
    };
} ir_instruction;

typedef struct {
    ir_instruction* instruction;
    size_t length;
    size_t register_count;
} ir_arr;

extern const char* ir_type_names[];
extern const char* op_names[];

void ir_arr_init(ir_arr* arr, size_t length);
void ir_arr_free(ir_arr* arr);
void ir_dump(const ir_arr* arr);
