#pragma once
#include <stdbool.h>
#include "diagnostics.h"
#include "lexer.h"
#include "ir.h"

// The lexer result (and the source it points into) must outlive this call.
bool parse(const lexer_result* lexer, diagnostic_list* diagnostics, ir_arr* result);
