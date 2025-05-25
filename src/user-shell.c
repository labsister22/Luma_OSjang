#include <stdint.h>
#include "header/filesystem/ext2.h"

#define BLOCK_COUNT 16

void syscall(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx)
{
    __asm__ volatile("mov %0, %%ebx" : /* <Empty> */ : "r"(ebx));
    __asm__ volatile("mov %0, %%ecx" : /* <Empty> */ : "r"(ecx));
    __asm__ volatile("mov %0, %%edx" : /* <Empty> */ : "r"(edx));
    __asm__ volatile("mov %0, %%eax" : /* <Empty> */ : "r"(eax));
    // Note : gcc usually use %eax as intermediate register,
    //        so it need to be the last one to mov
    __asm__ volatile("int $0x30");
}

int main(void)
{
    // Print welcome message
    syscall(6, (uint32_t)"Shell Started!\n", 15, 0xA); // Green text

    char buf;
    while (true)
    {
        // Print prompt
        syscall(6, (uint32_t)"> ", 2, 0xF); // White prompt

        // Read character
        syscall(4, (uint32_t)&buf, 0, 0);

        // Echo character
        syscall(5, (uint32_t)&buf, 0xF, 0);

        // Handle Enter key
        if (buf == '\n' || buf == '\r')
        {
            syscall(6, (uint32_t)"\n", 1, 0xF); // New line
        }
    }

    return 0;
}