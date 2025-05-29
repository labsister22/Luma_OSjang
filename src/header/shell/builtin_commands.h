// src/header/builtin_commands.h

#ifndef BUILTIN_COMMANDS_H
#define BUILTIN_COMMANDS_H

#include <stdint.h>
#include "header/stdlib/string.h"
#include "header/filesystem/ext2.h"
#include "header/process/process.h"
#include "header/driver/speaker.h"

extern char current_working_directory[256]; // Assuming MAX_PATH_LENGTH from user-shell.c
extern void user_syscall(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx);
// Function prototypes for built-in commands
void handle_cd(const char* path, int current_row);
int handle_ls(int current_row);
void handle_mkdir(const char* name, int current_row);
void handle_cat(const char* filename, int current_row);
void handle_cp(const char* source, const char* destination, int current_row);
void handle_rm(const char *path, int current_row);
void handle_mv(const char* source, const char* destination);
void handle_find(const char* name, int* current_row);
void handle_exec(const char* filename);
void handle_ps();
void handle_kill(const char* pid_str);

// Utility functions (from user-shell.c / shared)
void print_string(const char* str, int row, int col);
void print_char(char c, int row, int col);
void print_line(const char *str);
int string_to_int(const char* str);
void int_to_string(int value, char* str);

#endif // BUILTIN_COMMANDS_H