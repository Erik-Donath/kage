//
// Created by erikd on 04.06.26.
//

#pragma once
#include "diagnostics.h"
#include "tokens.h"

token_vec lexer(const char* file_content, diagnostic_list* diagnostics);
