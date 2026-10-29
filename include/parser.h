//
// Created by erikd on 04.06.26.
//

#pragma once
#include "diagnostics.h"
#include "tokens.h"
#include "ir.h"

bool parse(const token_vec* vec, diagnostic_list* diagnostics, ir_arr* result);
