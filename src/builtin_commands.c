// builtin_commands.c

#include <stdint.h>
#include <stdbool.h>
#include "header/stdlib/string.h"
#include "header/filesystem/ext2.h"
#include "header/shell/builtin_commands.h"

extern void syscall(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx);

// Variabel untuk menyimpan direktori saat ini (simulasi)
char current_directory[MAX_PATH_LENGTH] = "/";

int8_t syscall_with_return(uint32_t call_num, uint32_t p1, uint32_t p2, uint32_t p3) {
    uint32_t result = 0;

    __asm__ volatile(
        "pushl %%ebx\n\t"
        "pushl %%ecx\n\t"
        "pushl %%edx\n\t"
        "movl %1, %%ebx\n\t"
        "movl %2, %%ecx\n\t"
        "movl %3, %%edx\n\t"
        "movl %4, %%eax\n\t"
        "int $0x30\n\t"
        "movl %%eax, %0\n\t"
        "popl %%edx\n\t"
        "popl %%ecx\n\t"
        "popl %%ebx"
        : "=m"(result)
        : "g"(p1), "g"(p2), "g"(p3), "g"(call_num)
        : "eax"
    );

    return (int8_t)(result & 0xFF);
}

void shell_print(const char* str, int* current_row) {
    syscall(6, (uint32_t)str, *current_row, 0);
    (*current_row)++;
}

// Fungsi untuk mengubah direktori
static void change_directory(const char* path, int* row) {
    if (strcmp(path, "..") == 0) {
        // Pergi ke direktori atas
        if (strcmp(current_directory, "/") != 0) {
            // Cari '/' terakhir dan potong string
            int last_slash = -1;
            for (int i = 0; current_directory[i] != '\0'; i++) {
                if (current_directory[i] == '/') {
                    last_slash = i;
                }
            }
            if (last_slash > 0) {
                current_directory[last_slash] = '\0';
            } else {
                strcpy(current_directory, "/");
            }
        } else {
            shell_print("Already at root directory.", row);
        }
    } else if (path[0] == '/') {
        // Path absolut
        if (strlen(path) < MAX_PATH_LENGTH) {
            strcpy(current_directory, path);
        } else {
            shell_print("Path too long.", row);
        }
    } else {
        // Path relatif
        if (strcmp(current_directory, "/") != 0) {
            if (strlen(current_directory) + 1 + strlen(path) < MAX_PATH_LENGTH) {
                strcat(current_directory, "/");
                strcat(current_directory, path);
            } else {
                shell_print("Path too long.", row);
            }
        } else {
            if (strlen(current_directory) + strlen(path) < MAX_PATH_LENGTH) {
                strcat(current_directory, path);
            } else {
                shell_print("Path too long.", row);
            }
        }
    }
}

// Command: cd - Change directory
int8_t cmd_cd(char* args[], int argc, int* current_row) {
    if (argc == 0) {
        // Jika tidak ada argumen, kembali ke root
        strcpy(current_directory, "/");
        return CMD_SUCCESS;
    } else if (argc == 1) {
        change_directory(args[0], current_row);
        return CMD_SUCCESS;
    } else {
        shell_print("cd: too many arguments", current_row);
        shell_print("Usage: cd [directory]", current_row);
        return CMD_ERROR_INVALID_ARGS;
    }
}

// Command: cp - Copy files or directories
int8_t cmd_cp(char* args[], int argc, int* current_row) {
    if (argc < 2) {
        shell_print("cp: missing file operand", current_row);
        shell_print("Usage: cp [-r] <source> <destination>", current_row);
        return CMD_ERROR_INVALID_ARGS;
    }

    char* source = args[0];
    char* dest = args[1];
    bool recursive = false;

    // Check for -r flag
    if (argc >= 3 && strcmp(args[0], "-r") == 0) {
        recursive = true;
        source = args[1];
        dest = args[2];
    }

    // Use syscall untuk copy
    int8_t result;
    if (recursive) {
        result = syscall_with_return(12, (uint32_t)source, (uint32_t)dest, 2); // Copy directory
    } else {
        result = syscall_with_return(11, (uint32_t)source, (uint32_t)dest, 2); // Copy file
    }

    if (result == 0) {
        shell_print("File/directory copied successfully", current_row);
    } else {
        switch (result) {
            case 3:
                shell_print("cp: source file not found", current_row);
                break;
            case 4:
                shell_print("cp: source is a directory (use cp -r)", current_row);
                break;
            case 5:
                shell_print("cp: destination already exists", current_row);
                break;
            default:
                shell_print("cp: unknown error", current_row);
                break;
        }
    }

    return result;
}

// Command: mv - Move/rename files or directories
int8_t cmd_mv(char* args[], int argc, int* current_row) {
    (void)args; (void)argc;
    shell_print("mv: not implemented yet", current_row);
    return CMD_ERROR_INVALID_ARGS;
}
// Command: rm - Remove files
int8_t cmd_rm(char* args[], int argc, int* current_row) {
    (void)args; (void)argc;
    shell_print("rm: not implemented yet", current_row);
    return CMD_ERROR_INVALID_ARGS;
}
// Command: mkdir - Create directories
int8_t cmd_mkdir(char* args[], int argc, int* current_row) {
    (void)args; (void)argc;
    shell_print("mkdir: not implemented yet", current_row);
    return CMD_ERROR_INVALID_ARGS;
}
// Command: rmdir - Remove empty directories
int8_t cmd_rmdir(char* args[], int argc, int* current_row) {
    (void)args; (void)argc;
    shell_print("rmdir: not implemented yet", current_row);
    return CMD_ERROR_INVALID_ARGS;
}
// Command: ls - List directory contents
int8_t cmd_ls(char* args[], int argc, int* current_row) {
    (void)args; (void)argc;
    shell_print("ls: not implemented yet", current_row);
    return CMD_ERROR_INVALID_ARGS;
}
// Command: cat - Display file contents
int8_t cmd_cat(char* args[], int argc, int* current_row) {
    (void)args; (void)argc;
    shell_print("cat: not implemented yet", current_row);
    return CMD_ERROR_INVALID_ARGS;
}
// Command: touch - Create empty files
int8_t cmd_touch(char* args[], int argc, int* current_row) {
    (void)args; (void)argc;
    shell_print("touch: not implemented yet", current_row);
    return CMD_ERROR_INVALID_ARGS;
}
// Command: pwd - Print working directory
int8_t cmd_pwd(char* args[], int argc, int* current_row) {
    shell_print(current_directory, current_row);
    return CMD_SUCCESS;
    (void)args;
    (void)argc;
}
// Command: help - Display help information
int8_t cmd_help(char* args[], int argc, int* current_row) {
    (void)args;
    (void)argc;

    shell_print("Available commands:", current_row);
    shell_print("  cp [-r] <src> <dest> - Copy files/directories", current_row);
    shell_print("  mv <src> <dest>       - Move/rename files", current_row);
    shell_print("  rm <file>             - Remove files", current_row);
    shell_print("  mkdir <dir>           - Create directory", current_row);
    shell_print("  rmdir <dir>           - Remove directory", current_row);
    shell_print("  ls                    - List directory contents", current_row);
    shell_print("  cat <file>            - Display file contents", current_row);
    shell_print("  touch <file>          - Create empty file", current_row);
    shell_print("  pwd                   - Print working directory", current_row);
    shell_print("  cd [dir]              - Change directory", current_row);
    shell_print("  help                  - Show this help", current_row);
    shell_print("  clear/cls             - Clear screen", current_row);
    shell_print("  clock                 - Enable clock display", current_row);
    shell_print("  exit                  - Exit shell", current_row);

    return CMD_SUCCESS;
}

// Utility function: Print error message
void print_error(const char* message) {
    // This would need to integrate with your display system
    // For now, placeholder implementation
    (void)message;
}

// Utility function: Print success message
void print_success(const char* message) {
    // This would need to integrate with your display system
    // For now, placeholder implementation
    (void)message;
}

// Utility function: Check if file exists
bool file_exists(const char* path) {
    // This would need to integrate with your filesystem
    // For now, placeholder implementation
    (void)path;
    return true; // Assume file exists for testing
}

// Utility function: Check if path is a directory
bool is_directory_path(const char* path) {
    // This would need to integrate with your filesystem
    // For now, placeholder implementation
    (void)path;
    return true; // Assume it's a directory for testing
}

int parse_command(const char* cmd_line, char* cmd, char* args[], int* argc) {
    int i = 0;
    int arg_idx = 0;
    *argc = 0;

    // Skip leading spaces
    while (cmd_line[i] == ' ') i++;

    // Parse command name
    int cmd_idx = 0;
    while (cmd_line[i] && cmd_line[i] != ' ' && cmd_idx < 63) {
        cmd[cmd_idx++] = cmd_line[i++];
    }
    cmd[cmd_idx] = '\0';

    // Parse arguments
    while (cmd_line[i] && *argc < 10) { // Max 10 arguments
        // Skip spaces
        while (cmd_line[i] == ' ') i++;

        if (!cmd_line[i]) break;

        // Parse argument
        arg_idx = 0;
        while (cmd_line[i] && cmd_line[i] != ' ' && arg_idx < 127) {
            args[*argc][arg_idx++] = cmd_line[i++];
        }
        args[*argc][arg_idx] = '\0';
        (*argc)++;
    }

    return 0;
}

// Main command executor
int8_t execute_command(const char* cmd_line, int* current_row) {
    char cmd[64];
    static char arg_buffers[10][128]; // Static buffers for arguments
    char* args[10];
    int argc;

    // Setup argument pointers
    for (int i = 0; i < 10; i++) {
        args[i] = arg_buffers[i];
    }

    // Parse command line
    parse_command(cmd_line, cmd, args, &argc);

    if (strlen(cmd) == 0) {
        return CMD_SUCCESS; // Empty command
    }

    // Execute built-in commands
    if (strcmp(cmd, "cp") == 0) {
        return cmd_cp(args, argc, current_row);
    } else if (strcmp(cmd, "mv") == 0) {
        return cmd_mv(args, argc, current_row);
    } else if (strcmp(cmd, "rm") == 0) {
        return cmd_rm(args, argc, current_row);
    } else if (strcmp(cmd, "mkdir") == 0) {
        return cmd_mkdir(args, argc, current_row);
    } else if (strcmp(cmd, "rmdir") == 0) {
        return cmd_rmdir(args, argc, current_row);
    } else if (strcmp(cmd, "ls") == 0) {
        return cmd_ls(args, argc, current_row);
    } else if (strcmp(cmd, "cat") == 0) {
        return cmd_cat(args, argc, current_row);
    } else if (strcmp(cmd, "touch") == 0) {
        return cmd_touch(args, argc, current_row);
    } else if (strcmp(cmd, "pwd") == 0) {
        return cmd_pwd(args, argc, current_row);
    } else if (strcmp(cmd, "cd") == 0) {
        return cmd_cd(args, argc, current_row);
    } else if (strcmp(cmd, "help") == 0) {
        return cmd_help(args, argc, current_row);
    } else if (strcmp(cmd, "clear") == 0 || strcmp(cmd, "cls") == 0) {
        syscall(8, 0, 0, 0); // Clear screen
        *current_row = 0;
        shell_print("LumaOS Shell v1.0", current_row);
        return CMD_SUCCESS;
    } else {
        shell_print("Unknown command: ", current_row);
        shell_print(cmd, current_row);
        shell_print("Type 'help' for available commands", current_row);
        return CMD_ERROR_INVALID_ARGS;
    }
}