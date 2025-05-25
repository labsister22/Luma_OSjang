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
void print_char(char c, int row, int col) {
    syscall(5, (uint32_t)&c, col, row);  // Note: col, row order
}
// void print_prompt(int row) {
//     print_string("$ ", row, 0);
// }

void clear_screen() {
    syscall(8, 0, 0, 0);
}
void set_cursor(int col, int row) {
    syscall(9, col, row, 0);  // col, row order
}

char get_char() {
    char c = 0;
    do {
        syscall(4, (uint32_t)&c, 0, 0);
        // Add small delay to prevent busy waiting
        for (volatile int i = 0; i < 100; i++);
    } while (c == 0);
    return c;
}

int main(void)
{
    // syscall(6, (uint32_t)"LumaOS CLI started\n", 0, 0); // Print initial message
    // return 0;
    char buffer[COMMAND_BUFFER_SIZE];
    int current_row = 0;
    int buffer_pos = 0;
    int cursor_col = 0;
    bool exit_shell = false;

    syscall(7, 0, 0, 0); // Activate keyboard
    clear_screen();
    // print_string("Welcome-to-LumaOS-CLI\n", 0, 0);

    while (!exit_shell) {
        print_string("luma@os:~$ ", current_row, 0);
        cursor_col = 11; // Length of prompt
        set_cursor(cursor_col, current_row);

        buffer_pos = 0;
        for (int i = 0; i < COMMAND_BUFFER_SIZE; i++) {
            buffer[i] = '\0';
        }
        while (1) {
            char c = get_char();
            if (c == '\n' || c == '\r') {
                // Enter pressed - process command
                buffer[buffer_pos] = '\0';
                
                // Check for exit command before processing
                if (buffer[0] == 'e' && buffer[1] == 'x' && buffer[2] == 'i' && buffer[3] == 't' && buffer[4] == '\0') {
                    print_string("Goodbye!", current_row, 0);
                    exit_shell = true;
                }
                
                if (!exit_shell) {
                    // process_command(buffer, &current_row);
                }
                current_row++;
                break;
                
            } else if (c == '\b' || c == 127) {
                // Backspace
                if (buffer_pos > 0 && cursor_col > 11) {
                    buffer_pos--;
                    cursor_col--;
                    buffer[buffer_pos] = '\0';
                    
                    // Clear character on screen
                    print_char(' ', current_row, cursor_col);
                    set_cursor(cursor_col, current_row);
                }
                
            } else if (c >= 32 && c <= 126 && buffer_pos < COMMAND_BUFFER_SIZE - 1) {
                // Regular printable character
                buffer[buffer_pos] = c;
                buffer_pos++;
                
                // Display character
                print_char(c, current_row, cursor_col);
                cursor_col++;
                set_cursor(cursor_col, current_row);
                
                // Handle line wrap
                if (cursor_col >= 80) {
                    current_row++;
                    cursor_col = 0;
                    set_cursor(cursor_col, current_row);
                }
            }
        }
        if (current_row >= 24) {
            clear_screen();
            current_row = 2;
            print_string("LumaOS Shell v1.0", 0, 0);
            print_string("--- Screen cleared due to overflow ---", 1, 0);
        }
    }
    
    return 0;
}