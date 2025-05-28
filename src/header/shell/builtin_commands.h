// src/header/builtin_commands.h

#ifndef BUILTIN_COMMANDS_H
#define BUILTIN_COMMANDS_H

#include <stdint.h>
#include "header/stdlib/string.h"

extern char current_working_directory[32]; // Assuming MAX_PATH_LENGTH from user-shell.c
extern void syscall(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx);
// Function prototypes for built-in commands
void handle_cd(const char* path, int current_row);
void handle_ls(int current_row);
void handle_mkdir(const char* name, int current_row);
void handle_cat(const char* filename, int current_row);
void handle_cp(const char* source, const char* destination, int current_row);
void handle_rm(const char* path, int current_row);
void handle_mv(const char* source, const char* destination, int current_row);
void handle_find(const char* name, int current_row);

// Utility functions (from user-shell.c / shared)
void print_string(const char* str, int row, int col);
void print_char(char c, int row, int col);

#endif // BUILTIN_COMMANDS_H