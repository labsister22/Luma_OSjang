#include <stdint.h>
#include <stdbool.h>
#include "header/stdlib/string.h"

#define COMMAND_BUFFER_SIZE 128

void syscall(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx)
{
    __asm__ volatile("mov %0, %%ebx" : : "r"(ebx));
    __asm__ volatile("mov %0, %%ecx" : : "r"(ecx));
    __asm__ volatile("mov %0, %%edx" : : "r"(edx));
    __asm__ volatile("mov %0, %%eax" : : "r"(eax));
    __asm__ volatile("int $0x30");
}

void print_string(const char* str, int row, int col) {
    syscall(6, (uint32_t)str, row, col);
}

void print_prompt(int row) {
    print_string("$ ", row, 0);
}

void clear_screen() {
    syscall(8, 0, 0, 0);
}

int main(void)
{
    // syscall(6, (uint32_t)"LumaOS CLI started\n", 0, 0); // Print initial message
    // return 0;
    char buffer[64];
    int row = 1;

    syscall(7, 0, 0, 0); // Activate keyboard
    clear_screen();
    print_string("Welcome to LumaOS CLI!\n", 0, 0);

    while (1) {
        print_prompt(row);
        int pos = 0;
        for (int i = 0; i < 64; i++) buffer[i] = 0;
        int col = 2;
        while (1) {
            char c = 0;
            syscall(4, (uint32_t)&c, 0, 0);
            if (c == '\n' || c == '\r') break;
            if ((c == 8 || c == 127) && pos > 0) {
                pos--;
                col--;
                buffer[pos] = 0;
                syscall(5, (uint32_t)" ", row, col);
            } else if (c >= 32 && c <= 126 && pos < 63) {
                buffer[pos++] = c;
                syscall(5, (uint32_t)&c, row, col);
                col++;
            }
        }
        buffer[pos] = 0;
        row++;
        print_string("You typed: ", row, 0);
        print_string(buffer, row, 10);
        row++;
        if (row > 23) {
            clear_screen();
            row = 1;
        }
    }
    return 0;
}