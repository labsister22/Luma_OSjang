// src/builtin_commands.c

#include "header/shell/builtin_commands.h"
#include "header/stdlib/string.h" // Now strictly adhering to provided functions only

// No custom string functions here, relying strictly on header/stdlib/string.h
extern char current_working_directory[256];
uint32_t current_inode = 2; // Default to root inode for directory operations
int current_output_row = 0; // Track the current output row for commands like ls
void print_string(const char* str, int row, int col) {
    syscall(6, (uint32_t)str, row, col);
}

void print_char(char c, int row, int col) {
    syscall(5, (uint32_t)&c, col, row);
}

void print_line(const char *str)
{
    if (current_output_row < 24) { // Basic check to prevent writing off common screen area
        print_string(str, current_output_row, 0);
    }
    current_output_row++;
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
            char slash[] = "/";
            strcat(temp_path, slash); // Use allowed strcat
        }
        strcat(temp_path, path); // Use allowed strcat
    }

    char normalized_path[256];
    char slash[] = "/";
    strcpy(normalized_path, slash); // Use allowed strcpy

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
    char resolved_path[256];
    resolve_path_display(resolved_path, path);
    strcpy(current_working_directory, resolved_path); // Update the global CWD
    print_string(resolved_path, current_row + 1, 0);
}
int handle_ls(int current_row) {
    current_output_row = current_row +1;

    print_line("name                type   size");
    print_line("================================");

    // Use syscall to get directory listing
    syscall(22, (uint32_t)current_output_row, current_inode, 0);

    // Since syscall returns void, just advance the prompt after headers
    return current_output_row + 2;
}

void handle_mkdir(const char* name, int current_row) {
    print_string("mkdir: Not implemented. No kernel syscall for creating directories.", current_row+1, 0);
    (void)name;
    (void)current_row;
}

void handle_cat(const char* filename, int current_row) {
    print_string("cat: Not implemented. No suitable kernel syscall for file reading.", current_row+1, 0);
    (void)filename;
    (void)current_row;
}

void handle_cp(const char* source, const char* destination, int current_row) {
    print_string("cp: requires two arguments, not supported with current string functions.", current_row+1, 0);
    print_string("cp: Not implemented. No kernel syscalls for file copying.", current_row + 2, 0);
    (void)source;
    (void)destination;
    (void)current_row;
}

void handle_rm(const char* path, int current_row) {
    print_string("rm: Not implemented. No kernel syscall for removing files/directories.", current_row+1, 0);
    (void)path;
    (void)current_row;
}

void handle_mv(const char* source, const char* destination, int current_row) {
    print_string("mv: requires two arguments, not supported with current string functions.", current_row+1, 0);
    print_string("mv: Not implemented. No kernel syscall for moving/renaming files.", current_row + 2, 0);
    (void)source;
    (void)destination;
    (void)current_row;
}

void handle_find(const char* name, int current_row) {
    print_string("find: Not implemented. No kernel syscall for recursive file search.", current_row+1, 0);
    (void)name;
    (void)current_row;
}