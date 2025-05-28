// src/header/builtin_commands.h

#ifndef BUILTIN_COMMANDS_H
#define BUILTIN_COMMANDS_H

#include <stdint.h>
#include <stdbool.h>
#include "header/stdlib/string.h"
#include "header/filesystem/ext2.h"
#include "header/process/process.h"

extern char current_working_directory[256]; // Assuming MAX_PATH_LENGTH from user-shell.c
extern void user_syscall(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx);
// Function prototypes for built-in commands
void handle_cd(const char *path, int current_row);
void handle_ls(int current_row);
void handle_mkdir(const char *name, int current_row);
void handle_cat(const char *filename, int current_row);
void handle_cp(const char *source, const char *destination, int current_row);
void handle_rm(const char *path, int current_row);
void handle_mv(const char *source, const char *destination, int current_row);
void handle_find(const char *name, int current_row);

// Newly added built-in command prototypes
void handle_exec(const char *path, int current_row);
void handle_ps(int current_row);
void handle_kill(int pid, int current_row);

// Utility functions
void print_string(const char *str, int row, int col);
void print_char(char c, int row, int col);
void puts_integer_builtin(int number);

// Helper functions
int parse_path(const char *path, char names[][256], int max_parts);
uint32_t resolve_path(const char *path);
bool file_exists(const char *name, uint32_t parent_inode, bool *is_directory);

#endif // BUILTIN_COMMANDS_H