// src/header/builtin_commands.h

#ifndef BUILTIN_COMMANDS_H
#define BUILTIN_COMMANDS_H

#include <stdint.h>
#include "header/stdlib/string.h"
#include "header/process/process.h"
#include "header/filesystem/ext2.h"
#include <string.h>

extern char current_working_directory[256]; // Assuming MAX_PATH_LENGTH from user-shell.c
extern void user_syscall(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx);
// Function prototypes for built-in commands
void handle_cd(const char* path, int current_row);
void handle_ls(int current_row);
void handle_mkdir(const char* name, int current_row);
void handle_cat(const char* filename, int current_row);
void handle_cp(const char* source, const char* destination, int current_row);
void handle_rm(const char* path, int current_row);
void handle_mv(const char* source, const char* destination, int current_row);
void handle_find(const char* name, int current_row);

// New function declarations
void handle_exec(const char* program_path, int current_row);
void handle_ps(int current_row);
void handle_kill(const char* pid_str, int current_row);

// Utility functions (from user-shell.c / shared)
void print_string(const char* str, int row, int col);
void print_char(char c, int row, int col);

// Process user_syscall wrappers
int32_t create_process(struct EXT2DriverRequest request);
bool terminate_process(uint32_t pid);
void get_process_info(struct ProcessControlBlock* buffer, int* count);


#endif // BUILTIN_COMMANDS_H