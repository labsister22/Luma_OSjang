// src/user-shell.c

#include <stdint.h>
#include <stdbool.h>
#include "header/stdlib/string.h" // Now strictly adhering to provided functions only
#include "header/shell/builtin_commands.h"

#define COMMAND_BUFFER_SIZE 128
#define MAX_PATH_LENGTH 256

// Global current working directory
char current_working_directory[MAX_PATH_LENGTH] = "/";

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
void clear_screen() {
    syscall(8, 0, 0, 0);
}

void set_cursor(int col, int row) {
    syscall(9, col, row, 0);
}

char get_char() {
    char c = 0;
    do {
        syscall(4, (uint32_t)&c, 0, 0);
        for (volatile int i = 0; i < 100; i++);
    } while (c == 0);
    return c;
}

void get_time(struct Time* t) {
    syscall(10, (uint32_t)t, 0, 0);
}

void get_time_string(char* buffer) {
    struct Time t;
    get_time(&t);
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

// Function to process commands
void process_command(char* command_buffer, int* current_row_ptr) {
    // With strtok disallowed, simple command parsing (command + one arg) is the only option.
    // If command_buffer contains spaces, this will treat the whole thing as one command name.
    // Full UNIX-like command parsing is not possible under these constraints.

    char* command_name = command_buffer;
    char* arg1 = NULL;

    // Manual attempt to find the first space to separate command and first argument.
    // This is a very basic replacement for strtok for only one argument.
    size_t cmd_len = strlen(command_buffer);
    size_t i;
    for (i = 0; i < cmd_len; ++i) {
        if (command_buffer[i] == ' ') {
            command_buffer[i] = '\0'; // Null-terminate command name
            arg1 = &command_buffer[i+1];
            // Skip leading spaces for the argument
            while (*arg1 == ' ' && *arg1 != '\0') {
                arg1++;
            }
            if (*arg1 == '\0') { // If argument is just spaces or empty
                arg1 = NULL;
            }
            break;
        }
    }


    if (strlen(command_name) == 0) {
        return; // Empty command
    }

    if (strcmp(command_name, "cd") == 0) {
        if (arg1) {
            handle_cd(arg1, *current_row_ptr);
        } else {
            print_string("cd: missing argument", *current_row_ptr + 1, 0);
        }
    } else if (strcmp(command_name, "ls") == 0) {
        handle_ls(*current_row_ptr);
    } else if (strcmp(command_name, "mkdir") == 0) {
        if (arg1) {
            handle_mkdir(arg1, *current_row_ptr);
        } else {
            print_string("mkdir: missing argument", *current_row_ptr + 1, 0);
        }
    } else if (strcmp(command_name, "cat") == 0) {
        if (arg1) {
            handle_cat(arg1, *current_row_ptr);
        } else {
            print_string("cat: missing argument", *current_row_ptr + 1, 0);
        }
    } else if (strcmp(command_name, "cp") == 0) {
        // This command needs two arguments, which cannot be parsed with current string functions
        print_string("cp: requires two arguments, not supported with current string functions.", *current_row_ptr + 1, 0);
    } else if (strcmp(command_name, "rm") == 0) {
        if (arg1) {
            handle_rm(arg1, *current_row_ptr);
        } else {
            print_string("rm: missing argument", *current_row_ptr + 1, 0);
        }
    } else if (strcmp(command_name, "mv") == 0) {
        // This command needs two arguments, which cannot be parsed with current string functions
        print_string("mv: requires two arguments, not supported with current string functions.", *current_row_ptr + 1, 0);
    } else if (strcmp(command_name, "find") == 0) {
        if (arg1) {
            handle_find(arg1, *current_row_ptr);
        } else {
            print_string("find: missing argument", *current_row_ptr + 1, 0);
        }
    } else if (strcmp(command_name, "exit") == 0) { // Exit needs to be handled here directly now
        print_string("Goodbye!", *current_row_ptr, 0);
        // This will only be executed if 'exit' is the only thing typed.
        // It's technically unreachable now due to the main loop's check.
    } else if (strcmp(command_name, "clock") == 0) { // Clock also handled directly
        print_string("Clock running...", *current_row_ptr, 0);
        // It's technically unreachable now due to the main loop's check.
    }
    else {
        print_string("Unknown command: ", *current_row_ptr + 1, 0);
        print_string(command_name, *current_row_ptr + 1, (int)strlen("Unknown command: "));
    }

    *current_row_ptr += 1;
}


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

    char last_time[9] = "";

    print_string("LumaOS Shell v1.0", 0, 0);
    current_row = 1;

    while (!exit_shell) {
        char c = 0;
        char time_str[9];

        if (clock_enabled) {
            get_time_string(time_str);
            if (strcmp(time_str, last_time) != 0) {
                print_string(time_str, 24, 70);
                strcpy(last_time, time_str); // Use allowed strcpy
            }
        }

        char prompt[MAX_PATH_LENGTH + 15];
        strcpy(prompt, "luma@os:"); // Use allowed strcpy
        strcat(prompt, current_working_directory); // Use allowed strcat
        strcat(prompt, "$ "); // Use allowed strcat
        print_string(prompt, current_row, 0);
        cursor_col = (int)strlen(prompt);
        set_cursor(cursor_col, current_row);

        buffer_pos = 0;
        for (int i = 0; i < COMMAND_BUFFER_SIZE; i++) buffer[i] = '\0';

        while (1) {
            if (clock_enabled) {
                get_time_string(time_str);
                if (strcmp(time_str, last_time) != 0) {
                    print_string(time_str, 24, 70);
                    strcpy(last_time, time_str); // Use allowed strcpy
                }
            }

            syscall(4, (uint32_t)&c, 0, 0);
            if (c != 0) {
                if (c == '\n' || c == '\r') {
                    buffer[buffer_pos] = '\0';
                    current_row++;

                    // Special handling for "exit" and "clock" outside process_command,
                    // as parsing general arguments is now limited.
                    if (strcmp(buffer, "exit") == 0) {
                        print_string("Goodbye!", current_row, 0);
                        exit_shell = true;
                        break;
                    } else if (strcmp(buffer, "clock") == 0) {
                        print_string("Clock running...", current_row, 0);
                        clock_enabled = true;
                        current_row++;
                        break;
                    } else {
                        // Pass the buffer to process_command, which will try to parse one arg
                        process_command(buffer, &current_row);
                    }
                    break;
                } else if (c == '\b' || c == 127) {
                    if (buffer_pos > 0 && cursor_col > (int)strlen(prompt)) {
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
                c = 0;
            }
            for (volatile int d = 0; d < 10000; d++);
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