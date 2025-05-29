// src/builtin_commands.c

#include "header/shell/builtin_commands.h"
#include "header/stdlib/string.h" // Now strictly adhering to provided functions only
#include "header/filesystem/ext2.h" // Include for EXT2Inode and EXT2DirectoryEntry definitions

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

// void handle_find(const char* name, int current_row) {
//     print_string("find: Not implemented. No kernel syscall for recursive file search.", current_row+1, 0);
//     (void)name;
//     (void)current_row;
// }

void itoa(int value, char* str) {
    char buf[16];
    int i = 0, j = 0;
    if (value == 0) {
        str[0] = '0'; str[1] = '\0'; return;
    }
    if (value < 0) {
        str[j++] = '-';
        value = -value;
    }
    while (value > 0) {
        buf[i++] = (value % 10) + '0';
        value /= 10;
    }
    while (i > 0) str[j++] = buf[--i];
    str[j] = '\0';
}

void find_recursive(uint32_t curr_inode, const char* search_name, char* path, int* found, int* current_row) {
    // Enhanced safety checks
    if (*found || curr_inode == 0 || search_name == NULL || path == NULL || current_row == NULL) {
        return;
    }
    
    // Prevent deep recursion with static counter
    static int recursion_depth = 0;
    if (recursion_depth > 6) { // Reduce recursion depth further
        return;
    }
    recursion_depth++;

    // Validate search_name is not empty
    if (search_name[0] == '\0') {
        recursion_depth--;
        return;
    }

    struct EXT2Inode dir_inode;
    // Manual clear to avoid memset issues
    char* inode_ptr = (char*)&dir_inode;
    for (unsigned int i = 0; i < sizeof(struct EXT2Inode); i++) {
        inode_ptr[i] = 0;
    }
    
    // Use syscall to read inode safely
    syscall(21, (uint32_t)&dir_inode, curr_inode, 0);

    // Enhanced inode validation
    if (dir_inode.i_size == 0 || dir_inode.i_size > 65536 || 
        !(dir_inode.i_mode & 0x4000) || dir_inode.i_block[0] == 0) {
        recursion_depth--;
        return;
    }

    // Allocate buffer on stack with clear initialization
    uint8_t buf[512];
    for (unsigned int i = 0; i < 512; i++) {
        buf[i] = 0;
    }
    
    // Read directory block safely
    syscall(23, (uint32_t)buf, dir_inode.i_block[0], 1);

    uint32_t offset = 0;
    int entries_processed = 0;
    const int MAX_ENTRIES = 10; // Reduce max entries further
    
    while (offset < 500 && !(*found) && entries_processed < MAX_ENTRIES) { // Leave 12 bytes buffer
        // Bounds check before accessing
        if (offset + sizeof(struct EXT2DirectoryEntry) > 512) {
            break;
        }
        
        struct EXT2DirectoryEntry* entry = (struct EXT2DirectoryEntry*)(buf + offset);

        // Enhanced validation
        if (entry->rec_len == 0 || entry->rec_len < 8 || 
            entry->rec_len > 300 || offset + entry->rec_len > 512) {
            break;
        }
        
        if (entry->inode == 0 || entry->name_len == 0 || entry->name_len > 200) {
            offset += entry->rec_len;
            entries_processed++;
            continue;
        }

        // Safe name extraction with multiple bounds checks
        char entry_name[201]; // Reduced size for safety
        char* name_ptr = (char*)(entry + 1);
        unsigned int name_len = (unsigned int)entry->name_len;
        
        // Multiple safety checks
        if (name_len > 200) name_len = 200;
        if (offset + sizeof(struct EXT2DirectoryEntry) + name_len > 512) {
            name_len = 512 - offset - sizeof(struct EXT2DirectoryEntry);
            if (name_len > 200) name_len = 200;
        }
        
        // Manual copy with extra bounds checking
        for (unsigned int i = 0; i < name_len; i++) {
            if (offset + sizeof(struct EXT2DirectoryEntry) + i >= 512) {
                name_len = i;
                break;
            }
            if (i >= 200) {
                name_len = i;
                break;
            }
            entry_name[i] = name_ptr[i];
        }
        entry_name[name_len] = '\0';

        // Skip . and .. with exact comparison
        if ((name_len == 1 && entry_name[0] == '.') ||
            (name_len == 2 && entry_name[0] == '.' && entry_name[1] == '.')) {
            offset += entry->rec_len;
            entries_processed++;
            continue;
        }

        // Safe string comparison
        unsigned int search_len = 0;
        while (search_name[search_len] != '\0' && search_len < 200) {
            search_len++;
        }
        
        int match = (search_len == name_len && search_len > 0);
        for (unsigned int i = 0; match && i < search_len; i++) {
            if (search_name[i] != entry_name[i]) {
                match = 0;
            }
        }

        if (match) {
            // Build result path with safety
            char result_path[201];
            unsigned int path_pos = 0;
            
            // Copy current path with bounds check
            while (path[path_pos] != '\0' && path_pos < 150) {
                result_path[path_pos] = path[path_pos];
                path_pos++;
            }
            
            // Add separator if needed
            if (path_pos > 1 || path[0] != '/') {
                if (path_pos < 199) {
                    result_path[path_pos++] = '/';
                }
            }
            
            // Add entry name
            for (unsigned int i = 0; i < name_len && path_pos < 200; i++) {
                result_path[path_pos++] = entry_name[i];
            }
            result_path[path_pos] = '\0';

            // Safe output with screen bounds
            (*current_row)++;
            if (*current_row < 22) { // Leave more margin
                print_string("FOUND: ", *current_row, 0);
                print_string(result_path, *current_row, 7);
            }
            *found = 1;
            recursion_depth--;
            return;
        }

        // Safe recursion into subdirectories
        if (entry->inode != curr_inode && entry->inode < 100 && entry->inode > 1) { // Stricter limits
            struct EXT2Inode child_inode;
            char* child_ptr = (char*)&child_inode;
            for (unsigned int i = 0; i < sizeof(struct EXT2Inode); i++) {
                child_ptr[i] = 0;
            }
            
            syscall(21, (uint32_t)&child_inode, entry->inode, 0);
            
            // Enhanced child validation
            if ((child_inode.i_mode & 0x4000) == 0x4000 && 
                child_inode.i_size > 0 && child_inode.i_size < 65536 &&
                child_inode.i_block[0] != 0) {
                
                // Build new path for recursion with safety
                char new_path[201];
                unsigned int new_path_pos = 0;
                
                // Copy current path
                while (path[new_path_pos] != '\0' && new_path_pos < 150) {
                    new_path[new_path_pos] = path[new_path_pos];
                    new_path_pos++;
                }
                
                // Add separator if needed
                if (new_path_pos > 1 || path[0] != '/') {
                    if (new_path_pos < 199) {
                        new_path[new_path_pos++] = '/';
                    }
                }
                
                // Add entry name
                for (unsigned int i = 0; i < name_len && new_path_pos < 200; i++) {
                    new_path[new_path_pos++] = entry_name[i];
                }
                new_path[new_path_pos] = '\0';

                // Safe recursive call
                find_recursive(entry->inode, search_name, new_path, found, current_row);
            }
        }

        offset += entry->rec_len;
        entries_processed++;
    }
    
    recursion_depth--;
}

void find_command(const char* filename, int* current_row) {
    // Enhanced input validation
    if (filename == NULL || filename[0] == '\0' || current_row == NULL) {
        if (current_row != NULL) {
            (*current_row)++;
            if (*current_row < 23) {
                print_string("find: usage: find <filename>", *current_row, 0);
            }
        }
        return;
    }
    
    // Validate filename length and content
    unsigned int filename_len = 0;
    while (filename[filename_len] != '\0' && filename_len < 200) {
        // Check for valid filename characters
        char c = filename[filename_len];
        if (c < 32 || c > 126) {
            (*current_row)++;
            if (*current_row < 23) {
                print_string("find: invalid character in filename", *current_row, 0);
            }
            return;
        }
        filename_len++;
    }
    
    if (filename_len == 0 || filename_len > 200) {
        (*current_row)++;
        if (*current_row < 23) {
            print_string("find: invalid filename length", *current_row, 0);
        }
        return;
    }
    
    // Check for screen space
    if (*current_row >= 20) {
        (*current_row)++;
        if (*current_row < 23) {
            print_string("find: insufficient screen space", *current_row, 0);
        }
        return;
    }
    
    (*current_row)++;
    if (*current_row < 23) {
        print_string("Searching for: ", *current_row, 0);
        print_string(filename, *current_row, 15);
    }

    char root_path[3] = {'/', '\0', '\0'}; // Extra null for safety
    int found = 0;
    
    // Safe call to find_recursive
    find_recursive(2, filename, root_path, &found, current_row);

    if (!found) {
        (*current_row)++;
        if (*current_row < 23) {
            print_string("File not found", *current_row, 0);
        }
    }
}