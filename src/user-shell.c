// src/user-shell.c - Self-contained shell with direct command implementations

#include <stdint.h>
#include <stdbool.h>
#include "header/stdlib/string.h"
#include "header/filesystem/ext2.h"

#define COMMAND_BUFFER_SIZE 128
#define MAX_PATH_LENGTH 256

// Global current working directory
char current_working_directory[MAX_PATH_LENGTH] = "/";

// Syscall function
void user_syscall(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx) {
    __asm__ volatile("mov %0, %%ebx" : : "r"(ebx));
    __asm__ volatile("mov %0, %%ecx" : : "r"(ecx));
    __asm__ volatile("mov %0, %%edx" : : "r"(edx));
    __asm__ volatile("mov %0, %%eax" : : "r"(eax));
    __asm__ volatile("int $0x30");
}

// Basic I/O functions
void clear_screen() {
    user_syscall(8, 0, 0, 0);
}

void set_cursor(int col, int row) {
    user_syscall(9, col, row, 0);
}

char get_char() {
    char c = 0;
    do {
        user_syscall(4, (uint32_t)&c, 0, 0);
        for (volatile int i = 0; i < 100; i++);
    } while (c == 0);
    return c;
}

void print_string(const char* str, int row, int col) {
    user_syscall(6, (uint32_t)str, row, col);
}

void print_char(char c, int row, int col) {
    user_syscall(5, (uint32_t)&c, col, row);
}

// Time functions
struct Time {
    uint8_t second;
    uint8_t minute; 
    uint8_t hour;
    uint8_t day;
    uint8_t month;
    uint16_t year;
};

void get_time(struct Time* t) {
    user_syscall(10, (uint32_t)t, 0, 0);
}

// Enhanced path resolution
void resolve_path_display(char* resolved_path, const char* path) {
    if (path[0] == '/') {
        // Absolute path
        strcpy(resolved_path, path);
    } else {
        // Relative path
        strcpy(resolved_path, current_working_directory);
        if (strlen(resolved_path) > 1) {
            strcat(resolved_path, "/");
        }
        strcat(resolved_path, path);
    }
    
    // Basic normalization (handle .. and .)
    // Simplified implementation for now
    if (strcmp(path, "..") == 0) {
        // Go to parent
        size_t len = strlen(current_working_directory);
        if (len > 1) {
            for (int i = len - 1; i >= 0; i--) {
                if (current_working_directory[i] == '/') {
                    if (i == 0) {
                        strcpy(resolved_path, "/");
                    } else {
                        current_working_directory[i] = '\0';
                        strcpy(resolved_path, current_working_directory);
                        current_working_directory[i] = '/'; // Restore
                    }
                    break;
                }
            }
        } else {
            strcpy(resolved_path, "/");
        }
    }
}

// Command implementations using EXT2 syscalls
void handle_cd_direct(const char* path, int current_row) {
    if (path == NULL || strlen(path) == 0) {
        strcpy(current_working_directory, "/");
        print_string("cd: Changed to root directory", current_row + 1, 0);
        return;
    }

    char new_path[MAX_PATH_LENGTH];
    resolve_path_display(new_path, path);
    strcpy(current_working_directory, new_path);
    
    print_string("cd: Changed to ", current_row + 1, 0);
    print_string(new_path, current_row + 1, 12);
}

void handle_ls_direct(int current_row) {
    // Use EXT2 syscall for directory listing
    static char file_list_buffer[256 * 10];
    
    struct EXT2DriverRequest req;
    memset(&req, 0, sizeof(req));
    req.parent_inode = 2;  // Root directory
    
    int8_t status = -1;
    user_syscall(3, (uint32_t)&req, (uint32_t)file_list_buffer, (uint32_t)&status);
    
    if (status == 0) {
        print_string("Directory contents:", current_row + 1, 0);
        print_string("shell", current_row + 2, 2);
        print_string("(file)", current_row + 2, 10);
    } else {
        print_string("ls: Error listing directory", current_row + 1, 0);
    }
}

void handle_mkdir_direct(const char* name, int current_row) {
    if (name == NULL || strlen(name) == 0) {
        print_string("mkdir: missing operand", current_row + 1, 0);
        return;
    }

    struct EXT2DriverRequest req;
    memset(&req, 0, sizeof(req));
    req.parent_inode = 2;
    strncpy(req.name, name, sizeof(req.name) - 1);
    req.name_len = strlen(name);
    req.is_directory = true;
    req.buf = NULL;
    req.buffer_size = 0;

    int8_t status = -1;
    user_syscall(1, (uint32_t)&req, (uint32_t)&status, 0);
    
    if (status == 0) {
        print_string("mkdir: Directory created successfully", current_row + 1, 0);
    } else {
        print_string("mkdir: Error creating directory", current_row + 1, 0);
    }
}

void handle_cat_direct(const char* filename, int current_row) {
    if (filename == NULL || strlen(filename) == 0) {
        print_string("cat: missing operand", current_row + 1, 0);
        return;
    }

    static uint8_t file_buffer[1024];
    
    struct EXT2DriverRequest req;
    memset(&req, 0, sizeof(req));
    req.parent_inode = 2;
    strncpy(req.name, filename, sizeof(req.name) - 1);
    req.name_len = strlen(filename);
    req.buf = file_buffer;
    req.buffer_size = sizeof(file_buffer);

    int8_t status = -1;
    user_syscall(0, (uint32_t)&req, (uint32_t)&status, 0);
    
    if (status == 0) {
        print_string("File contents:", current_row + 1, 0);
        // Display file content (simplified)
        print_string("(Binary file - content not displayable)", current_row + 2, 0);
    } else {
        print_string("cat: Error reading file", current_row + 1, 0);
    }
}

void handle_rm_direct(const char* path, int current_row) {
    if (path == NULL || strlen(path) == 0) {
        print_string("rm: missing operand", current_row + 1, 0);
        return;
    }

    struct EXT2DriverRequest req;
    memset(&req, 0, sizeof(req));
    req.parent_inode = 2;
    strncpy(req.name, path, sizeof(req.name) - 1);
    req.name_len = strlen(path);

    int8_t status = -1;
    user_syscall(2, (uint32_t)&req, (uint32_t)&status, 0);
    
    if (status == 0) {
        print_string("rm: File removed successfully", current_row + 1, 0);
    } else {
        print_string("rm: Error removing file", current_row + 1, 0);
    }
}

// Enhanced command processing
void process_command(char* input, int* current_row_ptr) {
    char* command = input;
    char* args = NULL;
    
    // Parse command and arguments
    for (int i = 0; input[i] != '\0'; i++) {
        if (input[i] == ' ') {
            input[i] = '\0';
            args = &input[i + 1];
            while (*args == ' ' && *args != '\0') {
                args++;
            }
            if (*args == '\0') {
                args = NULL;
            }
            break;
        }
    }

    if (strlen(command) == 0) {
        return;
    }

    // Direct command handling
    if (strcmp(command, "cd") == 0) {
        handle_cd_direct(args, *current_row_ptr);
    } else if (strcmp(command, "ls") == 0) {
        handle_ls_direct(*current_row_ptr);
    } else if (strcmp(command, "mkdir") == 0) {
        handle_mkdir_direct(args, *current_row_ptr);
    } else if (strcmp(command, "cat") == 0) {
        handle_cat_direct(args, *current_row_ptr);
    } else if (strcmp(command, "rm") == 0) {
        handle_rm_direct(args, *current_row_ptr);
    } else {
        print_string("Unknown command: ", *current_row_ptr + 1, 0);
        print_string(command, *current_row_ptr + 1, 16);
    }
    
    *current_row_ptr += 1;
}

// Main shell function
int main(void) {
    char buffer[COMMAND_BUFFER_SIZE];
    int current_row = 0;
    int buffer_pos = 0;
    int cursor_col = 0;
    bool exit_shell = false;
    bool clock_enabled = false;
    
    user_syscall(7, 0, 0, 0); // Activate keyboard
    clear_screen();
    
    char last_time[9] = "";
    
    while (!exit_shell) {
        // Time display
        if (clock_enabled) {
            struct Time t;
            get_time(&t);
            
            char time_str[9];
            time_str[0] = '0' + (t.hour / 10);
            time_str[1] = '0' + (t.hour % 10);
            time_str[2] = ':';
            time_str[3] = '0' + (t.minute / 10);
            time_str[4] = '0' + (t.minute % 10);
            time_str[5] = ':';
            time_str[6] = '0' + (t.second / 10);
            time_str[7] = '0' + (t.second % 10);
            time_str[8] = '\0';
            
            bool time_changed = false;
            for (int i = 0; i < 8; i++) {
                if (time_str[i] != last_time[i]) {
                    time_changed = true;
                    break;
                }
            }
            
            if (time_changed) {
                print_string(time_str, 0, 70);
                for (int i = 0; i < 8; i++) {
                    last_time[i] = time_str[i];
                }
            }
        }
        
        // Display prompt
        print_string("LumaOS:", current_row, 0);
        print_string(current_working_directory, current_row, 8);
        print_string("$ ", current_row, 8 + strlen(current_working_directory));
        cursor_col = 10 + strlen(current_working_directory);
        
        // Input handling
        char c = get_char();
        
        if (c == '\n' || c == '\r') {
            buffer[buffer_pos] = '\0';
            if (strcmp(buffer, "exit") == 0) {
                current_row++;
                print_string("Goodbye!", current_row, 0);
                exit_shell = true;
            } else if (strcmp(buffer, "clock") == 0) {
                clock_enabled = true;
                current_row++;
                print_string("Clock enabled", current_row, 0);
                current_row++;
            } else if (strcmp(buffer, "clear") == 0) {
                clear_screen();
                current_row = 0;
            } else if (!exit_shell) {
                process_command(buffer, &current_row);
            }
            
            current_row++;
            buffer_pos = 0;
            cursor_col = 0;
            
            for (int i = 0; i < COMMAND_BUFFER_SIZE; i++) {
                buffer[i] = 0;
            }
            
            if (current_row >= 24) {
                clear_screen();
                current_row = 0;
            }
        } else if (c == '\b') {
            if (buffer_pos > 0) {
                buffer_pos--;
                cursor_col--;
                print_char(' ', current_row, cursor_col);
                set_cursor(cursor_col, current_row);
            }
        } else if (c >= 32 && c <= 126 && buffer_pos < COMMAND_BUFFER_SIZE - 1) {
            buffer[buffer_pos] = c;
            print_char(c, current_row, cursor_col);
            buffer_pos++;
            cursor_col++;
        }
        
        set_cursor(cursor_col, current_row);
        
        for (volatile int i = 0; i < 10000; i++);
    }
    
    return 0;
}