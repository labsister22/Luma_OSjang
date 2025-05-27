#include "command-shell.h"

#define DIRECTORY_BUFFER_SIZE 4096

// Global variables for current directory tracking
uint32_t current_dir_inode = 2; // Start at root
char current_path[512] = "/";

// Utility function for output using syscall 6 (print string)
void puts_shell(char* val, uint32_t color) {
    (void)color; // Ignore color parameter for now
    
    // Print character by character using syscall 5 (putchar)
    for (int i = 0; val[i] != '\0'; i++) {
        if (val[i] == '\n') {
            // Handle newline - move to next line
            syscall(17, 0, 0, 0); // Use syscall 17 for newline if available
        } else {
            // Print character at current cursor position
            syscall(5, (uint32_t)&val[i], 0, 0);
        }
    }
}

// Count number of words in a string (space-separated)
uint16_t countWords(char* str) {
    if (!str || str[0] == '\0') return 0;
    
    uint16_t count = 0;
    bool in_word = false;
    
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] != ' ' && str[i] != '\t' && str[i] != '\n') {
            if (!in_word) {
                count++;
                in_word = true;
            }
        } else {
            in_word = false;
        }
    }
    
    return count;
}

// Extract a specific word from a string (0-indexed)
void getWord(char* str, uint16_t word_idx, char* result) {
    if (!str || !result) {
        if (result) result[0] = '\0';
        return;
    }
    
    uint16_t current_word = 0;
    int i = 0, j = 0;
    bool in_word = false;
    bool found = false;
    
    // Skip leading spaces
    while (str[i] == ' ' || str[i] == '\t' || str[i] == '\n') i++;
    
    while (str[i] != '\0' && !found) {
        if (str[i] != ' ' && str[i] != '\t' && str[i] != '\n') {
            if (!in_word) {
                in_word = true;
                if (current_word == word_idx) {
                    // Start copying this word
                    j = 0;
                }
            }
            
            if (current_word == word_idx) {
                result[j++] = str[i];
            }
        } else {
            if (in_word) {
                if (current_word == word_idx) {
                    found = true;
                }
                current_word++;
                in_word = false;
            }
        }
        i++;
    }
    
    if (current_word == word_idx && in_word) {
        found = true;
    }
    
    result[j] = '\0';
}

void cd(char* command) {
    uint16_t n_words = countWords(command);
    if (n_words != 2) {
        puts_shell("Error: Invalid syntax\n", 0x04);
        puts_shell("Usage: cd <directory>\n", 0x07);
        return;
    }

    char target[256];
    getWord(command, 1, target);

    // Handle special cases
    if (strcmp(target, ".") == 0) {
        return; // Stay in current directory
    }
    
    if (strcmp(target, "/") == 0) {
        current_dir_inode = 2;
        strcpy(current_path, "/");
        puts_shell("Changed to root directory\n", 0x0A);
        return;
    }

    if (strcmp(target, "..") == 0) {
        if (current_dir_inode == 2) {
            puts_shell("Error: Already at root directory\n", 0x04);
            return;
        }
        
        // Find parent directory by looking for ".." entry
        struct EXT2DriverRequest request;
        char dir_buffer[DIRECTORY_BUFFER_SIZE];
        
        memset(&request, 0, sizeof(request));
        request.buf = dir_buffer;
        request.parent_inode = current_dir_inode;
        request.buffer_size = DIRECTORY_BUFFER_SIZE;
        request.name[0] = '.';
        request.name_len = 1;
        request.is_directory = 1;

        int8_t result;
        syscall(12, (uint32_t)&request, (uint32_t)&result, 0); // SYS_READ_DIRECTORY

        if (result != 0) {
            puts_shell("Error: Cannot access current directory\n", 0x04);
            return;
        }

        // Look for ".." entry
        uint32_t offset = 0;
        uint32_t parent_inode = 2; // Default to root
        
        while (offset < DIRECTORY_BUFFER_SIZE) {
            struct EXT2DirectoryEntry* entry = (struct EXT2DirectoryEntry*)(dir_buffer + offset);
            
            if (entry->inode == 0 || entry->rec_len == 0) {
                break;
            }

            char* entry_name = (char*)(entry + 1);
            
            if (entry->name_len == 2 && 
                entry_name[0] == '.' && entry_name[1] == '.' &&
                entry->file_type == 2) {
                parent_inode = entry->inode;
                break;
            }

            offset += entry->rec_len;
        }

        current_dir_inode = parent_inode;
        
        // Update path - remove last directory
        int len = strlen(current_path);
        if (len > 1) { // Not root
            len--; // Remove trailing slash if present
            while (len > 0 && current_path[len] != '/') {
                len--;
            }
            if (len == 0) len = 1; // Keep root slash
            current_path[len] = '\0';
        }
        
        puts_shell("Changed to parent directory\n", 0x0A);
        return;
    }

    // Try to find the target directory
    struct EXT2DriverRequest request;
    char dir_buffer[DIRECTORY_BUFFER_SIZE];
    
    memset(&request, 0, sizeof(request));
    request.buf = dir_buffer;
    request.parent_inode = current_dir_inode;
    request.buffer_size = DIRECTORY_BUFFER_SIZE;
    request.name[0] = '.';
    request.name_len = 1;
    request.is_directory = 1;

    int8_t result;
    syscall(12, (uint32_t)&request, (uint32_t)&result, 0); // SYS_READ_DIRECTORY

    if (result != 0) {
        puts_shell("Error: Cannot access current directory\n", 0x04);
        return;
    }

    // Look for target directory
    uint32_t offset = 0;
    bool found = false;
    uint32_t target_inode = 0;
    
    while (offset < DIRECTORY_BUFFER_SIZE && !found) {
        struct EXT2DirectoryEntry* entry = (struct EXT2DirectoryEntry*)(dir_buffer + offset);
        
        if (entry->inode == 0 || entry->rec_len == 0) {
            break;
        }

        char* entry_name = (char*)(entry + 1);
        char temp_name[257];
        memcpy(temp_name, entry_name, entry->name_len);
        temp_name[entry->name_len] = '\0';

        if (strcmp(temp_name, target) == 0 && entry->file_type == 2) {
            found = true;
            target_inode = entry->inode;
        }

        offset += entry->rec_len;
    }

    if (found) {
        current_dir_inode = target_inode;
        
        // Update path
        if (strcmp(current_path, "/") != 0) {
            int len = strlen(current_path);
            current_path[len] = '/';
            current_path[len + 1] = '\0';
        }
        
        // Add directory name to path
        int path_len = strlen(current_path);
        int target_len = strlen(target);
        for (int i = 0; i < target_len; i++) {
            current_path[path_len + i] = target[i];
        }
        current_path[path_len + target_len] = '\0';
        
        puts_shell("Changed directory to: ", 0x0A);
        puts_shell(current_path, 0x0F);
        puts_shell("\n", 0x0F);
    } else {
        puts_shell("Error: Directory '", 0x04);
        puts_shell(target, 0x04);
        puts_shell("' not found\n", 0x04);
    }
}

// Other command implementations (placeholders for now)
void ls() {
    puts_shell("ls command not implemented yet\n", 0x0E);
}

void pwd() {
    puts_shell(current_path, 0x0F);
    puts_shell("\n", 0x0F);
}

void mkdir_cmd(char* command) {
    (void)command; // Suppress unused parameter warning
    puts_shell("mkdir command not implemented yet\n", 0x0E);
}

void cat(char* command) {
    (void)command; // Suppress unused parameter warning
    puts_shell("cat command not implemented yet\n", 0x0E);
}

void clear_cmd() {
    syscall(8, 0, 0, 0);
}
