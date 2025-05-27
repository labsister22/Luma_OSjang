#ifndef _COMMAND_SHELL_H
#define _COMMAND_SHELL_H

#include "../filesystem/fat32.h"
#include "../process/process.h"
#include "../stdlib/string.h"
#include <stdint.h>

void syscall(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx);

void puts(char* val, uint32_t color);

void puts_integer(int number);

void remove(char* name, char* ext, uint32_t parent_number);

void copy(char* src_name, char* src_ext, uint32_t src_parent_number, char* target_name, char* target_ext, uint32_t target_parent_number);

void cp(char* command);

void rm(char* command);

void mkdir(char *command);

void mv(char* command);

void ls();

void findShell(char* command);
void cd(char *command);

void cat(char* command);

void exec(char* command);

void ps();

void kill(char* command);

void copyPath(char* command);

void mvPath(char* command);

#endif