#include <stdint.h>
#include "header/shell/user-shell.h"
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
    struct BlockBuffer bl[BLOCK_COUNT] = {0};
    struct EXT2DriverRequest request = {
        .buf = &bl,
        .name = "shell",
        .name_len = 5,
        .parent_inode = 1, // Root directory inode
        .buffer_size = BLOCK_SIZE * BLOCK_COUNT,
        .is_directory = false};
    int32_t retcode;
    syscall(0, (uint32_t)&request, (uint32_t)&retcode, 0);
    __asm__("int3");
    if (retcode == 0)
        __asm__("int3");
    syscall(6, (uint32_t)"owo\n", 4, 0xF);

    char buf;
    syscall(7, 0, 0, 0);
    while (true)
    {
        syscall(4, (uint32_t)&buf, 0, 0);
        syscall(5, (uint32_t)&buf, 0xF, 0);
    }

    return 0;
}