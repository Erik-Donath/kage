//
// Created by erikd on 04.06.26.
//

#include <main.h>
#include <tokens.h>
#include <lexer.h>
#include <stdlib.h>
#include <stdio.h>

int main(const int argc, const char *argv[]) {
    if (argc < 2) {
        printf("Kage Help: \n Use %s <FILE PATH> to run a kage file.\n\r", argv[0]);
        return 1;
    }

    const char* file_contents = read_file(argv[1]);
    if (!file_contents) {
        printf("Failed to open File %s\n", argv[2]);
        return 1;
    }

    const token_vec vec = lexer(file_contents);
    token_dump(&vec);

    return 0;
}

const char* read_file(const char* path) {
    FILE *file = fopen(path, "r");
    if (!file)
        return NULL;

    fseek(file, 0, SEEK_END);
    const size_t size = (size_t)ftell(file);
    rewind(file);

    char *buffer = malloc(size + 1);
    fread(buffer, 1, size, file);
    buffer[size] = '\0';

    fclose(file);
    return buffer;
}
