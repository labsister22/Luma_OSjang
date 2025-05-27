// src/builtin_commands.c

#include "header/shell/builtin_commands.h"

// No custom string functions here, relying strictly on header/stdlib/string.h
extern char current_working_directory[256];

void print_string(const char* str, int row, int col) {
    user_syscall(6, (uint32_t)str, row, col);
}

void print_char(char c, int row, int col) {
    user_syscall(5, (uint32_t)&c, col, row);
}

// --- Helper for path concatenation (Purely for shell display, not actual FS changes) ---
// This function combines current_working_directory with a given path
// and handles '..' and '.' for navigating within the *shell's display path*.
// It does NOT interact with the kernel's file system state.
void resolve_path_display(char* resolved_path, const char* path) {
    char temp_path[256];
    // Cannot use custom strcpy, assuming strcpy from string.h is available and defined
    strcpy(temp_path, ""); // Initialize with empty string

    char *segment_start;
    char *ptr;

    if (path[0] == '/') { // Absolute path
        strcpy(temp_path, path); // Use allowed strcpy
        resolved_path[0] = '/';
        resolved_path[1] = '\0';
    } else { // Relative path
        strcpy(temp_path, current_working_directory); // Use allowed strcpy
        if (strlen(temp_path) > 1 && temp_path[strlen(temp_path) - 1] != '/') {
            strcat(temp_path, "/"); // Use allowed strcat
        }
        strcat(temp_path, path); // Use allowed strcat
    }

    char normalized_path[256];
    strcpy(normalized_path, "/"); // Use allowed strcpy

    ptr = temp_path;
    while (*ptr) {
        if (*ptr == '/') {
            ptr++;
            continue;
        }

        segment_start = ptr;
        while (*ptr && *ptr != '/') {
            ptr++;
        }
        size_t len = (size_t)(ptr - segment_start); // Cast to size_t for strlen-like comparisons

        if (len == 0) continue;

        if (len == 2 && segment_start[0] == '.' && segment_start[1] == '.') { // ".."
            if (strcmp(normalized_path, "/") != 0) {
                // Cannot use strrchr, manual find last slash
                size_t norm_len = strlen(normalized_path);
                if (norm_len > 1) { // Not at root
                    size_t last_slash_idx = norm_len - 1;
                    while (last_slash_idx > 0 && normalized_path[last_slash_idx] != '/') {
                        last_slash_idx--;
                    }
                    if (last_slash_idx == 0 && normalized_path[0] == '/') { // If it's like /a -> /
                        normalized_path[1] = '\0'; // Stay at root
                    } else {
                        normalized_path[last_slash_idx] = '\0'; // Truncate
                    }
                } else { // Already at root
                    // Do nothing, stay at root
                }
            }
        } else if (len == 1 && segment_start[0] == '.') { // "."
            // Do nothing
        } else { // Regular segment
            if (strlen(normalized_path) > 1 || normalized_path[0] != '/') {
                strcat(normalized_path, "/"); // Use allowed strcat
            }
            // Manual copy to append, as strncat is not in your header
            size_t current_norm_len = strlen(normalized_path);
            for (size_t k = 0; k < len && current_norm_len + k < 255; k++) {
                normalized_path[current_norm_len + k] = segment_start[k];
            }
            normalized_path[current_norm_len + len] = '\0';
        }
    }

    if (strlen(normalized_path) == 0) {
        strcpy(resolved_path, "/"); // Use allowed strcpy
    } else {
        strcpy(resolved_path, normalized_path); // Use allowed strcpy
    }
}


// --- Built-in Command Implementations (Stubs for unsupported FS operations) ---

void handle_cd(const char* path, int current_row) {
    print_string("cd: Not implemented. No suitable kernel user_syscall for file reading.", current_row +1 , 0);
    (void)path;
    (void)current_row;
}

void handle_ls(int current_row) {
    print_string("ls: Not implemented. No kernel user_syscall for directory listing.", current_row+1, 0);
    (void)current_row;
}

void handle_mkdir(const char* name, int current_row) {
    print_string("mkdir: Not implemented. No kernel user_syscall for creating directories.", current_row+1, 0);
    (void)name;
    (void)current_row;
}

void handle_cat(const char* filename, int current_row) {
    print_string("cat: Not implemented. No suitable kernel user_syscall for file reading.", current_row+1, 0);
    (void)filename;
    (void)current_row;
}

void handle_cp(const char* source, const char* destination, int current_row) {
    print_string("cp: requires two arguments, not supported with current string functions.", current_row+1, 0);
    print_string("cp: Not implemented. No kernel user_syscalls for file copying.", current_row + 2, 0);
    (void)source;
    (void)destination;
    (void)current_row;
}

void handle_rm(const char* path, int current_row) {
    print_string("rm: Not implemented. No kernel user_syscall for removing files/directories.", current_row+1, 0);
    (void)path;
    (void)current_row;
}

void handle_mv(const char* source, const char* destination, int current_row) {
    print_string("mv: requires two arguments, not supported with current string functions.", current_row+1, 0);
    print_string("mv: Not implemented. No kernel user_syscall for moving/renaming files.", current_row + 2, 0);
    (void)source;
    (void)destination;
    (void)current_row;
}

void handle_find(const char* name, int current_row) {
    print_string("find: Not implemented. No kernel user_syscall for recursive file search.", current_row+1, 0);
    (void)name;
    (void)current_row;
}

// ...existing code...

// Helper function to convert string to integer (simple implementation)
int simple_atoi(const char* str) {
    int result = 0;
    int sign = 1;
    size_t i = 0;
    
    // Skip whitespace
    while (str[i] == ' ') i++;
    
    // Handle sign
    if (str[i] == '-') {
        sign = -1;
        i++;
    } else if (str[i] == '+') {
        i++;
    }
    
    // Convert digits
    while (str[i] >= '0' && str[i] <= '9') {
        result = result * 10 + (str[i] - '0');
        i++;
    }
    
    return sign * result;
}

// Helper function to convert integer to string
void simple_itoa(int num, char* str) {
    if (num == 0) {
        str[0] = '0';
        str[1] = '\0';
        return;
    }
    
    int i = 0;
    int is_negative = 0;
    
    if (num < 0) {
        is_negative = 1;
        num = -num;
    }
    
    while (num > 0) {
        str[i++] = (num % 10) + '0';
        num /= 10;
    }
    
    if (is_negative) {
        str[i++] = '-';
    }
    
    str[i] = '\0';
    
    // Reverse string
    int start = 0;
    int end = i - 1;
    while (start < end) {
        char temp = str[start];
        str[start] = str[end];
        str[end] = temp;
        start++;
        end--;
    }
}

// Process user_syscall wrapper implementations
int32_t create_process(struct EXT2DriverRequest request) {
    int32_t result;
    user_syscall(11, (uint32_t)&request, (uint32_t)&result, 0);
    return result;
}

bool terminate_process(uint32_t pid) {
    bool result;
    user_syscall(12, pid, (uint32_t)&result, 0);
    return result;
}

void get_process_info(struct ProcessControlBlock* buffer, int* count) {
    user_syscall(13, (uint32_t)buffer, (uint32_t)count, 0);
}

// Command handler implementations
void handle_exec(const char* program_path, int current_row) {
    // Create EXT2DriverRequest for the program
    struct EXT2DriverRequest request;
    
    // Set up request parameters
    request.parent_inode = 2; // Root directory inode
    request.buffer_size = 1024 * 8; // 8KB buffer for executable
    request.is_directory = false;
    
    // Copy program name to request (limit to fit in name array)
    size_t path_len = strlen(program_path);
    if (path_len > 255) path_len = 255;
    
    for (size_t i = 0; i < path_len; i++) {
        request.name[i] = program_path[i];
    }
    request.name_len = (uint8_t)path_len;
    
    // Allocate buffer for executable
    static char exec_buffer[1024 * 8];
    request.buf = exec_buffer;
    
    // Try to create the process
    int32_t result = create_process(request);
    
    switch (result) {
        case 0:
            print_string("Process created successfully", current_row + 1, 0);
            break;
        case 1:
            print_string("exec: Maximum number of processes exceeded", current_row + 1, 0);
            break;
        case 2:
            print_string("exec: Invalid entrypoint", current_row + 1, 0);
            break;
        case 3:
            print_string("exec: Not enough memory", current_row + 1, 0);
            break;
        case 4:
            print_string("exec: File not found or read failure", current_row + 1, 0);
            break;
        default:
            print_string("exec: Unknown error", current_row + 1, 0);
            break;
    }
}

void handle_ps(int current_row) {
    static struct ProcessControlBlock processes[16]; // PROCESS_COUNT_MAX
    int count = 0;

    // Initialize the buffer
    for (int i = 0; i < 16; i++) {
        processes[i].metadata.pid = 0;
        processes[i].metadata.active = false;
        for (int j = 0; j < 32; j++) {
            processes[i].metadata.name[j] = '\0';
        }
    }
    
    // Get process information
    get_process_info(processes, &count);
    
    if (count == 0) {
        print_string("No active processes", current_row + 1, 0);
        return;
    }
    
    // Print header
    print_string("PID    NAME     STATE", current_row + 1, 0);
    print_string("----   -------- -----", current_row + 2, 0);
    
    int display_row = current_row + 3;
    for (int i = 0; i < count && display_row < 24; i++) {
        char line[80];
        char pid_str[16];
        char state_str[16];
        
        // Convert PID to string
        simple_itoa(processes[i].metadata.pid, pid_str);
        
        // Convert state to string
        switch (processes[i].metadata.cur_state) {
            case 0:
                strcpy(state_str, "READY");
                break;
            case 1:
                strcpy(state_str, "RUN");
                break;
            case 2:
                strcpy(state_str, "BLOCK");
                break;
            default:
                strcpy(state_str, "UNK");
                break;
        }
        
        // Format line: PID (6 chars) + Name (8 chars) + State (5 chars)
        strcpy(line, "");
        
        // Add PID (left-aligned in 6 characters)
        strcat(line, pid_str);
        for (size_t j = strlen(pid_str); j < 6; j++) {
            strcat(line, " ");
        }
        
        // Add process name manually (since strncat is not available)
        size_t current_len = strlen(line);
        size_t name_len = strlen(processes[i].metadata.name);
        if (name_len > 8) name_len = 8; // Max 8 characters
        
        for (size_t j = 0; j < name_len; j++) {
            line[current_len + j] = processes[i].metadata.name[j];
        }
        line[current_len + name_len] = '\0';
        
        // Pad with spaces to 8 characters
        for (size_t j = name_len; j < 8; j++) {
            strcat(line, " ");
        }
        strcat(line, " ");
        
        // Add state
        strcat(line, state_str);
        
        print_string(line, display_row, 0);
        display_row++;
    }
}

void handle_kill(const char* pid_str, int current_row) {
    // Convert string to PID
    int pid = simple_atoi(pid_str);
    
    if (pid <= 0) {
        print_string("kill: Invalid PID", current_row + 1, 0);
        return;
    }
    
    // Try to terminate the process
    bool result = terminate_process((uint32_t)pid);
    
    if (result) {
        print_string("Process terminated successfully", current_row + 1, 0);
    } else {
        print_string("kill: Process not found or cannot be terminated", current_row + 1, 0);
    }
}