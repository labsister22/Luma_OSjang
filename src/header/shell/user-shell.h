#ifndef USER_SHELL_H
#define USER_SHELL_H

#include <stdint.h>
#include <stdbool.h>
#include "header/filesystem/ext2.h"

#define BLOCK_COUNT 16

// syscall wrapper
void syscall(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx);

// main function
int main(void);

#endif // USER_SHELL_H
