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

// ...existing code...

uint32_t find_inode_by_name(uint32_t parent_inode, const char* name) {
    struct EXT2Inode dir_inode;
    read_inode(parent_inode, &dir_inode);
    
    if (!(dir_inode.i_mode & EXT2_S_IFDIR)) {
        return 0; // Not a directory
    }
    
    // PERBAIKAN: Gunakan logical block iteration dengan indirection support
    uint32_t name_len = strlen(name);
    uint8_t block_buffer[BLOCK_SIZE];
    
    // Iterate through ALL blocks (dengan indirection support)
    uint32_t total_blocks = (dir_inode.i_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    
    for (uint32_t logical_block = 0; logical_block < total_blocks; logical_block++) {
        // PERBAIKAN: Gunakan get_physical_block_from_logical untuk indirection
        uint32_t physical_block = get_physical_block_from_logical(&dir_inode, logical_block);
        
        if (physical_block == 0) continue; // Sparse block atau tidak dialokasi
        
        read_blocks(block_buffer, physical_block, 1);
        
        uint32_t offset = 0;
        while (offset < BLOCK_SIZE) {
            struct EXT2DirectoryEntry *entry = (struct EXT2DirectoryEntry *)(block_buffer + offset);
            
            // PERBAIKAN: Better validation
            if (entry->rec_len == 0 || entry->rec_len < sizeof(struct EXT2DirectoryEntry)) {
                break; // Invalid entry
            }
            
            if (offset + entry->rec_len > BLOCK_SIZE) {
                break; // Entry extends beyond block
            }
            
            // Skip deleted entries
            if (entry->inode != 0 && entry->name_len > 0) {
                // PERBAIKAN: Compare name length first
                if (entry->name_len == name_len) {
                    const char* entry_name = (const char*)(entry + 1);
                    
                    // PERBAIKAN: Bounds check untuk name comparison
                    if (offset + sizeof(struct EXT2DirectoryEntry) + entry->name_len <= BLOCK_SIZE) {
                        int match = 1;
                        for (uint32_t k = 0; k < name_len; k++) {
                            if (entry_name[k] != name[k]) {
                                match = 0;
                                break;
                            }
                        }
                        if (match) {
                            return entry->inode;
                        }
                    }
                }
            }
            
            offset += entry->rec_len;
        }
    }
    
    return 0; // Not found
}
// Global variables for rm operation
static uint32_t rm_target_inode = 0;
static int rm_target_found = 0;
static int rm_is_directory = 0;

// Modified find_recursive for rm - finds file/directory in current directory only
void find_for_rm(uint32_t curr_inode, const char* search_name, int* found, int* is_dir, uint32_t* target_inode) {
    if (*found || curr_inode == 0 || search_name == NULL) {
        return;
    }

    // Validate search_name is not empty
    if (search_name[0] == '\0') {
        return;
    }

    struct EXT2Inode dir_inode;
    char* inode_ptr = (char*)&dir_inode;
    for (unsigned int i = 0; i < sizeof(struct EXT2Inode); i++) {
        inode_ptr[i] = 0;
    }
    
    syscall(21, (uint32_t)&dir_inode, curr_inode, 0);

    if (dir_inode.i_size == 0 || dir_inode.i_size > 65536 || 
        !(dir_inode.i_mode & 0x4000) || dir_inode.i_block[0] == 0) {
        return;
    }

    uint8_t buf[1024];
    for (unsigned int i = 0; i < 1024; i++) {
        buf[i] = 0;
    }
    
    syscall(23, (uint32_t)buf, dir_inode.i_block[0], 1);

    uint32_t offset = 0;
    int entries_processed = 0;
    const int MAX_ENTRIES = 50;
    
    while (offset < 1000 && !(*found) && entries_processed < MAX_ENTRIES) {
        if (offset + sizeof(struct EXT2DirectoryEntry) > 1024) {
            break;
        }
        
        struct EXT2DirectoryEntry* entry = (struct EXT2DirectoryEntry*)(buf + offset);

        if (entry->rec_len == 0 || entry->rec_len < 8 || 
            entry->rec_len > 500 || offset + entry->rec_len > 1024) {
            break;
        }
        
        if (entry->inode == 0 || entry->name_len == 0 || entry->name_len > 255) {
            offset += entry->rec_len;
            entries_processed++;
            continue;
        }

        char entry_name[256];
        char* name_ptr = (char*)(entry + 1);
        unsigned int name_len = (unsigned int)entry->name_len;
        
        if (name_len > 255) name_len = 255;
        if (offset + sizeof(struct EXT2DirectoryEntry) + name_len > 1024) {
            name_len = 1024 - offset - sizeof(struct EXT2DirectoryEntry);
            if (name_len > 255) name_len = 255;
        }
        
        for (unsigned int i = 0; i < name_len; i++) {
            if (offset + sizeof(struct EXT2DirectoryEntry) + i >= 1024) {
                name_len = i;
                break;
            }
            if (i >= 255) {
                name_len = i;
                break;
            }
            entry_name[i] = name_ptr[i];
        }
        entry_name[name_len] = '\0';

        // Compare with search name
        unsigned int search_len = 0;
        while (search_name[search_len] != '\0' && search_len < 255) {
            search_len++;
        }
        
        int match = (search_len == name_len && search_len > 0);
        for (unsigned int i = 0; match && i < search_len; i++) {
            if (search_name[i] != entry_name[i]) {
                match = 0;
            }
        }

        if (match) {
            // Found the target - get its type
            struct EXT2Inode found_inode;
            char* found_ptr = (char*)&found_inode;
            for (unsigned int i = 0; i < sizeof(struct EXT2Inode); i++) {
                found_ptr[i] = 0;
            }
            syscall(21, (uint32_t)&found_inode, entry->inode, 0);
            
            *found = 1;
            *target_inode = entry->inode;
            *is_dir = (found_inode.i_mode & 0x4000) ? 1 : 0;
            return;
        }

        offset += entry->rec_len;
        entries_processed++;
    }
}

// Perbaiki handle_cd menggunakan find_for_rm
void handle_cd(const char* path, int current_row) {
    current_output_row = current_row;

    if (!path || strlen(path) == 0) {
        print_line("Error: Missing argument");
        return;
    }

    // Handle special cases
    if (strcmp(path, "/") == 0) {
        // Go to root directory
        current_inode = 2; // Root inode
        strcpy(current_working_directory, "/");
        
        current_output_row++;
        print_string("Changed to root directory: /", current_output_row, 0);
        return;
    }
    
    if (strcmp(path, "..") == 0 || strcmp(path, "../") == 0) {
        // Go to parent directory (simplified - for now just go to root)
        if (strcmp(current_working_directory, "/") != 0) {
            current_inode = 2; // Root inode
            strcpy(current_working_directory, "/");
            
            current_output_row++;
            print_string("Changed to parent directory: /", current_output_row, 0);
        } else {
            current_output_row++;
            print_string("Already at root directory", current_output_row, 0);
        }
        return;
    }
    
    if (strcmp(path, ".") == 0) {
        // Stay in current directory
        current_output_row++;
        print_string("Staying in current directory: ", current_output_row, 0);
        print_string(current_working_directory, current_output_row, 30);
        return;
    }

    // PERBAIKAN: Gunakan find_for_rm untuk mencari directory
    int target_found = 0;
    int is_directory = 0;
    uint32_t target_inode = 0;
    
    // Use find_for_rm to locate the target directory
    find_for_rm(current_inode, path, &target_found, &is_directory, &target_inode);
    
    if (!target_found || target_inode == 0) {
        current_output_row++;
        print_string("cd: directory '", current_output_row, 0);
        print_string(path, current_output_row, 15);
        print_string("' not found", current_output_row, 15 + strlen(path));
        return;
    }
    
    // Check if target is actually a directory
    if (!is_directory) {
        current_output_row++;
        print_string("cd: '", current_output_row, 0);
        print_string(path, current_output_row, 5);
        print_string("' is not a directory", current_output_row, 5 + strlen(path));
        return;
    }
    
    // Success! Update current directory
    current_inode = target_inode;
    
    // Update working directory path
    if (path[0] == '/') {
        // Absolute path
        strcpy(current_working_directory, path);
    } else {
        // Relative path - append to current
        if (strcmp(current_working_directory, "/") != 0) {
            strcat(current_working_directory, "/");
        }
        strcat(current_working_directory, path);
    }
    
    // Optional: gunakan syscall jika ada implementasi cd di kernel
    syscall(18, current_inode, (uint32_t)path, 0);
    
    // Success message
    current_output_row++;
    print_string("Changed directory to: ", current_output_row, 0);
    print_string(current_working_directory, current_output_row, 22);
}
// ...existing code...

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




// Fixed handle_rm function - uses find_for_rm to locate files/directories
void handle_rm(const char* path, int current_row) {
    if (!path || strlen(path) == 0) {
        print_string("rm: missing file/directory name", current_row + 1, 0);
        return;
    }
    
    // Special check: don't allow removal of . or ..
    if (strcmp(path, ".") == 0 || strcmp(path, "..") == 0 || strcmp(path, "../") == 0) {
        print_string("rm: cannot remove '.' or '..' or '../' directories", current_row + 1, 0);
        return;
    }
    
    // Reset global variables
    rm_target_inode = 0;
    rm_target_found = 0;
    rm_is_directory = 0;
    
    // Use find_for_rm to locate the target in current directory
    find_for_rm(current_inode, path, &rm_target_found, &rm_is_directory, &rm_target_inode);
    
    if (!rm_target_found || rm_target_inode == 0) {
        print_string("rm: file/directory '", current_row + 1, 0);
        print_string(path, current_row + 1, 20);
        print_string("' not found", current_row + 1, 20 + strlen(path));
        return;
    }
    
    // Prepare the deletion request
    struct EXT2DriverRequest request;
    memset(&request, 0, sizeof(request));
    
    // Set up the request parameters
    request.parent_inode = current_inode;
    request.buffer_size = 0;
    request.buf = NULL;
    request.is_directory = rm_is_directory;
    
    // Copy the name safely
    size_t path_len = strlen(path);
    size_t max_copy = sizeof(request.name) - 1;
    if (path_len < max_copy) {
        max_copy = path_len;
    }
    
    for (size_t i = 0; i < max_copy; i++) {
        request.name[i] = path[i];
    }
    request.name[max_copy] = '\0';
    request.name_len = path_len;
    
    // Call the appropriate syscall based on type
    int8_t result;
    if (rm_is_directory) {
        // Use syscall 26 for directory deletion
        syscall(26, (uint32_t)&request, (uint32_t)&result, 0);
    } else {
        // Use syscall 25 for file deletion
        syscall(25, (uint32_t)&request, (uint32_t)&result, 0);
    }
    
    // Report the result
    if (result == 0) {
        print_string("rm: ", current_row + 1, 0);
        if (rm_is_directory) {
            print_string("directory '", current_row + 1, 4);
        } else {
            print_string("file '", current_row + 1, 4);
        }
        print_string(path, current_row + 1, rm_is_directory ? 15 : 10);
        print_string("' removed successfully", current_row + 1, (rm_is_directory ? 15 : 10) + strlen(path));
    } else {
        print_string("rm: failed to remove ", current_row + 1, 0);
        if (rm_is_directory) {
            print_string("directory '", current_row + 1, 21);
        } else {
            print_string("file '", current_row + 1, 21);
        }
        print_string(path, current_row + 1, rm_is_directory ? 32 : 27);
        
        // Provide more specific error messages based on result code
        switch (result) {
            case 1:
                print_string("' - already exists or permission denied", current_row + 1, (rm_is_directory ? 32 : 27) + strlen(path));
                break;
            case 2:
                print_string("' - parent is not a directory", current_row + 1, (rm_is_directory ? 32 : 27) + strlen(path));
                break;
            case 3:
                print_string("' - directory not empty", current_row + 1, (rm_is_directory ? 32 : 27) + strlen(path));
                break;
            default:
                print_string("' - unknown error", current_row + 1, (rm_is_directory ? 32 : 27) + strlen(path));
                break;
        }
    }
}

void handle_mv(const char* source, const char* destination, int current_row) {
    print_string("mv: requires two arguments, not supported with current string functions.", current_row+1, 0);
    print_string("mv: Not implemented. No kernel syscall for moving/renaming files.", current_row + 2, 0);
    (void)source;
    (void)destination;
    (void)current_row;
}

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
    if (recursion_depth > 10) { // Increased recursion depth for deeper directories
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

    // Enhanced inode validation - Check if it's a directory
    if (dir_inode.i_size == 0 || dir_inode.i_size > 65536 || 
        !(dir_inode.i_mode & 0x4000) || dir_inode.i_block[0] == 0) {
        recursion_depth--;
        return;
    }

    // Allocate buffer on stack with clear initialization
    uint8_t buf[1024]; // Increased buffer size for larger directories
    for (unsigned int i = 0; i < 1024; i++) {
        buf[i] = 0;
    }
    
    // Read directory block safely
    syscall(23, (uint32_t)buf, dir_inode.i_block[0], 1);

    uint32_t offset = 0;
    int entries_processed = 0;
    const int MAX_ENTRIES = 50; // Increased max entries
    
    while (offset < 1000 && !(*found) && entries_processed < MAX_ENTRIES) { // Leave 24 bytes buffer
        // Bounds check before accessing
        if (offset + sizeof(struct EXT2DirectoryEntry) > 1024) {
            break;
        }
        
        struct EXT2DirectoryEntry* entry = (struct EXT2DirectoryEntry*)(buf + offset);

        // Enhanced validation
        if (entry->rec_len == 0 || entry->rec_len < 8 || 
            entry->rec_len > 500 || offset + entry->rec_len > 1024) {
            break;
        }
        
        if (entry->inode == 0 || entry->name_len == 0 || entry->name_len > 255) {
            offset += entry->rec_len;
            entries_processed++;
            continue;
        }

        // Safe name extraction with multiple bounds checks
        char entry_name[256]; // Standard EXT2 filename max length
        char* name_ptr = (char*)(entry + 1);
        unsigned int name_len = (unsigned int)entry->name_len;
        
        // Multiple safety checks
        if (name_len > 255) name_len = 255;
        if (offset + sizeof(struct EXT2DirectoryEntry) + name_len > 1024) {
            name_len = 1024 - offset - sizeof(struct EXT2DirectoryEntry);
            if (name_len > 255) name_len = 255;
        }
        
        // Manual copy with extra bounds checking
        for (unsigned int i = 0; i < name_len; i++) {
            if (offset + sizeof(struct EXT2DirectoryEntry) + i >= 1024) {
                name_len = i;
                break;
            }
            if (i >= 255) {
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
        while (search_name[search_len] != '\0' && search_len < 255) {
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
            char result_path[512];
            unsigned int path_pos = 0;
            
            // Copy current path with bounds check
            while (path[path_pos] != '\0' && path_pos < 400) {
                result_path[path_pos] = path[path_pos];
                path_pos++;
            }
            
            // Add separator if needed
            if (path_pos > 1 || path[0] != '/') {
                if (path_pos < 510) {
                    result_path[path_pos++] = '/';
                }
            }
            
            // Add entry name
            for (unsigned int i = 0; i < name_len && path_pos < 511; i++) {
                result_path[path_pos++] = entry_name[i];
            }
            result_path[path_pos] = '\0';

            // Check if it's a directory or file and display accordingly
            struct EXT2Inode found_inode;
            char* found_ptr = (char*)&found_inode;
            for (unsigned int i = 0; i < sizeof(struct EXT2Inode); i++) {
                found_ptr[i] = 0;
            }
            syscall(21, (uint32_t)&found_inode, entry->inode, 0);
            
            // Safe output with screen bounds
            (*current_row)++;
            if (*current_row < 22) { // Leave more margin
                if (found_inode.i_mode & 0x4000) {
                    print_string("FOUND DIR: ", *current_row, 0);
                    print_string(result_path, *current_row, 11);
                } else {
                    print_string("FOUND FILE: ", *current_row, 0);
                    print_string(result_path, *current_row, 12);
                }
            }
            *found = 1;
            recursion_depth--;
            return;
        }

        // Safe recursion into subdirectories - REMOVED inode number restrictions
        if (entry->inode != curr_inode && entry->inode > 1) { // Only check if not current dir and valid
            struct EXT2Inode child_inode;
            char* child_ptr = (char*)&child_inode;
            for (unsigned int i = 0; i < sizeof(struct EXT2Inode); i++) {
                child_ptr[i] = 0;
            }
            
            syscall(21, (uint32_t)&child_inode, entry->inode, 0);
            
            // Enhanced child validation - Check if it's a directory
            if ((child_inode.i_mode & 0x4000) == 0x4000 && 
                child_inode.i_size > 0 && child_inode.i_size < 65536 &&
                child_inode.i_block[0] != 0) {
                
                // Build new path for recursion with safety
                char new_path[512];
                unsigned int new_path_pos = 0;
                
                // Copy current path
                while (path[new_path_pos] != '\0' && new_path_pos < 400) {
                    new_path[new_path_pos] = path[new_path_pos];
                    new_path_pos++;
                }
                
                // Add separator if needed
                if (new_path_pos > 1 || path[0] != '/') {
                    if (new_path_pos < 510) {
                        new_path[new_path_pos++] = '/';
                    }
                }
                
                // Add entry name
                for (unsigned int i = 0; i < name_len && new_path_pos < 511; i++) {
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

void handle_find(const char* filename, int* current_row) {
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
    while (filename[filename_len] != '\0' && filename_len < 255) {
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
    
    if (filename_len == 0 || filename_len > 255) {
        (*current_row)++;
        if (*current_row < 23) {
            print_string("find: invalid filename length", *current_row, 0);
        }
        return;
    }
    
    // Check for screen space
    if (*current_row >= 18) { // Leave more room for results
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
            print_string("File or directory not found", *current_row, 0);
        }
    }
}