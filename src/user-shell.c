// user-shell.c
#include <stdint.h>
#include <stdbool.h>
#include "header/stdlib/string.h"
#include "header/shell/builtin_commands.h" // Include header for commands

#define COMMAND_BUFFER_SIZE 128

struct Time {
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
};

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
void get_time(struct Time* t) {
    syscall(10, (uint32_t)t, 0, 0);  // Ganti 10 dengan syscall ID yang benar untuk waktu
}

void get_time_string(char* buffer) {
    struct Time t;
    get_time(&t);
    // Format: HH:MM:SS (e.g., 09:25:01)
    buffer[0] = '0' + (t.hour / 10);
    buffer[1] = '0' + (t.hour % 10);
    buffer[2] = ':';
    buffer[3] = '0' + (t.minute / 10);
    buffer[4] = '0' + (t.minute % 10);
    buffer[5] = ':';
    buffer[6] = '0' + (t.second / 10);
    buffer[7] = '0' + (t.second % 10);
    buffer[8] = '\0';
}

// Deklarasi fungsi execute_command dari builtin_commands.c
int8_t execute_command(const char* cmd_line, int* current_row);
extern char current_directory[256]; // Deklarasi variabel global current_directory

int main(void)
{
    char buffer[COMMAND_BUFFER_SIZE];
    int current_row = 0;
    int buffer_pos = 0;
    int cursor_col = 0;
    bool exit_shell = false;
    bool clock_enabled = false;
    syscall(7, 0, 0, 0); // Activate keyboard
    clear_screen();
    print_string("LumaOS Shell v1.0", 0, 0);

    char last_time[9] = "";
    while (!exit_shell) {
        char time_str[9];
        get_time_string(time_str);
        if (clock_enabled) {
            if (strcmp(time_str, last_time) != 0) {
                print_string(time_str, 24, 70);
                for (int i = 0; i < 9; i++) last_time[i] = time_str[i];
            }
        }
        print_string("luma@os:", current_row, 0);
        print_string(current_directory, current_row, 9);
        print_string("$ ", current_row, 9 + strlen(current_directory));
        cursor_col = 11 + strlen(current_directory);
        set_cursor(cursor_col, current_row);
        buffer_pos = 0;
        for (int i = 0; i < COMMAND_BUFFER_SIZE; i++) buffer[i] = '\0';
        char c;
        while (1) {
            if (clock_enabled) {
                get_time_string(time_str);
                if (strcmp(time_str, last_time) != 0) {
                    print_string(time_str, 24, 70);
                    for (int i = 0; i < 9; i++) last_time[i] = time_str[i];
                }
            }
            c = get_char();
            if (c == '\n' || c == '\r') {
                buffer[buffer_pos] = '\0';
                if (!exit_shell) {
                    if (strcmp(buffer, "exit") == 0) {
                        print_string("Goodbye!", current_row, 0);
                        exit_shell = true;
                    } else if (strcmp(buffer, "clock") == 0) {
                        clock_enabled = true;
                        current_row++;
                        print_string("Clock running...", current_row, 0);
                        current_row++;
                    } else {
                        execute_command(buffer, &current_row);
                    }
                }
                current_row++;
                break;
            } else if (c == '\b' || c == 127) {
                if ((size_t)buffer_pos > 0 && (size_t)cursor_col > (10 + strlen(current_directory) + 1)) {
                    buffer_pos--;
                    cursor_col--;
                    buffer[buffer_pos] = '\0';
                    print_char(' ', current_row, cursor_col);
                    set_cursor(cursor_col, current_row);
                }
            } else if (c >= 32 && c <= 126 && buffer_pos < COMMAND_BUFFER_SIZE - 1) {
                buffer[buffer_pos] = c;
                buffer_pos++;
                print_char(c, current_row, cursor_col);
                cursor_col++;
                set_cursor(cursor_col, current_row);
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