#ifndef _COMMAND_SHELL_H
#define _COMMAND_SHELL_H

#include "header/filesystem/ext2.h"
#include "header/stdlib/string.h"
#include <stdint.h>
#include <stdbool.h>

// Syscall function - will be defined in user-shell.c
void syscall(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx);

// Utility functions
void puts_shell(char* val, uint32_t color);
uint16_t countWords(char* str);
void getWord(char* str, uint16_t word_idx, char* result);

// Command functions
void cd(char* command);
void ls();
void pwd();
void mkdir_cmd(char* command);
void cat(char* command);
void clear_cmd();

// Global variables for current directory tracking
extern uint32_t current_dir_inode;
extern char current_path[512];

#endif
