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
// ...existing code...

// Perbaiki function signature untuk resolve_path_display
uint32_t resolve_path_display(char* resolved_path, const char* path) {
    char temp_path[256];
    strcpy(temp_path, ""); // Initialize with empty string

    char *segment_start;
    char *ptr;

    if (path[0] == '/') { // Absolute path
        strcpy(temp_path, path);
        strcpy(resolved_path, "/");
    } else { // Relative path
        strcpy(temp_path, current_working_directory);
        if (strlen(temp_path) > 1 && temp_path[strlen(temp_path) - 1] != '/') {
            char slash[] = "/";
            strcat(temp_path, slash);
        }
        strcat(temp_path, path);
        strcpy(resolved_path, temp_path); // Copy to resolved_path
    }

    char normalized_path[256];
    char slash[] = "/";
    strcpy(normalized_path, slash);

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
        size_t len = (size_t)(ptr - segment_start);

        if (len == 0) continue;

        if (len == 2 && segment_start[0] == '.' && segment_start[1] == '.') { // ".."
            if (strcmp(normalized_path, "/") != 0) {
                size_t norm_len = strlen(normalized_path);
                if (norm_len > 1) {
                    size_t last_slash_idx = norm_len - 1;
                    while (last_slash_idx > 0 && normalized_path[last_slash_idx] != '/') {
                        last_slash_idx--;
                    }
                    if (last_slash_idx == 0 && normalized_path[0] == '/') {
                        normalized_path[1] = '\0';
                    } else {
                        normalized_path[last_slash_idx] = '\0';
                    }
                }
            }
        } else if (len == 1 && segment_start[0] == '.') { // "."
            // Do nothing
        } else { // Regular segment
            if (strlen(normalized_path) > 1 || normalized_path[0] != '/') {
                strcat(normalized_path, "/");
            }
            size_t current_norm_len = strlen(normalized_path);
            for (size_t k = 0; k < len && current_norm_len + k < 255; k++) {
                normalized_path[current_norm_len + k] = segment_start[k];
            }
            normalized_path[current_norm_len + len] = '\0';
        }
    }

    if (strlen(normalized_path) == 0) {
        strcpy(resolved_path, "/");
    } else {
        strcpy(resolved_path, normalized_path);
    }

    // Return a valid inode number (simplified logic)
    // Untuk sekarang, assume semua path valid dan return root inode
    // Nanti bisa diimprove dengan actual filesystem lookup
    return 2; // Root inode
}

uint32_t find_inode_by_name(uint32_t parent_inode, const char* name) {
    struct EXT2Inode dir_inode;
    read_inode(parent_inode, &dir_inode);
    
    if (!(dir_inode.i_mode & EXT2_S_IFDIR)) {
        return 0; // Not a directory
    }
    
    uint8_t block_buffer[BLOCK_SIZE];
    for (uint32_t i = 0; i < dir_inode.i_blocks && i < 12; i++) {
        if (dir_inode.i_block[i] == 0) continue;
        
        read_blocks(block_buffer, dir_inode.i_block[i], 1);
        
        uint32_t offset = 0;
        while (offset < BLOCK_SIZE) {
            struct EXT2DirectoryEntry *entry = (struct EXT2DirectoryEntry *)(block_buffer + offset);
            
            if (entry->inode == 0 || entry->rec_len == 0) break;
            if (offset + entry->rec_len > BLOCK_SIZE) break;
            
            const char* entry_name = (const char*)(entry + 1);
            
            // Compare names
            if (entry->name_len == strlen(name)) {
                int match = 1;
                for (uint8_t k = 0; k < entry->name_len; k++) {
                    if (entry_name[k] != name[k]) {
                        match = 0;
                        break;
                    }
                }
                if (match) {
                    return entry->inode;
                }
            }
            
            offset += entry->rec_len;
        }
    }
    
    return 0; // Not found
}

// Perbaiki handle_cd
void handle_cd(const char* path, int current_row) {
    current_output_row = current_row;

    if (!path || strlen(path) == 0) {
        print_line("Error: Missing argument");
        return;
    }

    char resolved_path[256];
    uint32_t target_inode = resolve_path_display(resolved_path, path);

    // Check jika path tidak valid (simplified check)
    if (target_inode == 0) { // 0 = invalid inode
        print_line("Error: Path not found");
        return;
    }

    // Update current directory
    current_inode = target_inode;
    
    // Optional: gunakan syscall jika ada implementasi cd di kernel
    syscall(18, current_inode, (uint32_t)path, 0);

    // Update working directory string dengan resolved path
    strcpy(current_working_directory, resolved_path);
    
    // Success message (optional)
    current_output_row++;
    print_string("Changed directory to: ", current_output_row, 0);
    print_string(resolved_path, current_output_row, 22);
}
// ...existing code...
int handle_ls(int current_row) {
    current_output_row = current_row +1;

    print_line("name                type   size");
    print_line("================================");

    // Use syscall to get directory listing
    syscall(22, (uint32_t)current_output_row, current_inode, 0);

    // Since syscall returns void, just advance the prompt after headers
    return current_output_row + 2;
}

// ...existing code...

void handle_mkdir(const char* name, int current_row) {
    if (!name || strlen(name) == 0) {
        print_string("mkdir: missing argument\n", current_row + 1, 0);
        return;
    }
    
    struct EXT2DriverRequest request;
    memset(&request, 0, sizeof(request));
    
    request.parent_inode = current_inode;
    request.name_len = strlen(name);
    request.is_directory = 1;
    request.buffer_size = 0;
    request.buf = NULL;
    
    // Copy nama
    size_t copy_len = strlen(name);
    if (copy_len >= sizeof(request.name)) {
        copy_len = sizeof(request.name) - 1;
    }
    for (size_t i = 0; i < copy_len; i++) {
        request.name[i] = name[i];
    }
    request.name[copy_len] = '\0';
    
    int8_t result;
    syscall(24, (uint32_t)&request, (uint32_t)&result, 0);
    
    // Better error messages
    if (result == 0) {
        print_string("Directory created: ", current_row + 1, 0);
        print_string(name, current_row + 1, 19);
        print_string("\n", current_row + 1, 19 + strlen(name));
    } else if (result == 1) {
        print_string("mkdir: directory '", current_row + 1, 0);
        print_string(name, current_row + 1, 18);
        print_string("' already exists\n", current_row + 1, 18 + strlen(name));
    } else if (result == 2) {
        print_string("mkdir: parent is not a directory\n", current_row + 1, 0);
    } else {
        print_string("mkdir: failed to create directory\n", current_row + 1, 0);
    }
}
// ...existing code...

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