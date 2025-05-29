// src/builtin_commands.c

#include "header/shell/builtin_commands.h"
#include "header/stdlib/string.h" // Now strictly adhering to provided functions only

// No custom string functions here, relying strictly on header/stdlib/string.h
extern char current_working_directory[256];
uint32_t current_inode = 2; // Default to root inode for directory operations
int current_output_row = 0; // Track the current output row for commands like ls
void print_string(const char* str, int* row_ptr, int* col_ptr) { // Terima pointer
    // Kirim NILAI ke syscall
    syscall(6, (uint32_t)str, (uint32_t)*row_ptr, (uint32_t)*col_ptr);

    // Syscall 6 (SYS_PUTS) di kernel mengupdate kursor dan mengembalikan baris/kolom.
    // Namun, karena SYS_PUTS hanya mencetak string dan mengupdate kursor framebuffer,
    // kita perlu secara manual memajukan row_ptr/col_ptr di userspace berdasarkan string.
    // Atau, lebih baik, minta syscall 6 mengembalikan posisi kursor akhir.
    // Jika syscall 6 tidak mengembalikan, lakukan perhitungan di userspace:

    size_t len = strlen(str);
    for (size_t i = 0; i < len; ++i) {
        if (str[i] == '\n') {
            (*row_ptr)++;
            (*col_ptr) = 0;
        } else {
            (*col_ptr)++;
            if ((*col_ptr) >= 80) { // Wrap around
                (*col_ptr) = 0;
                (*row_ptr)++;
            }
        }
    }
}

void print_char(char c, int* row_ptr, int* col_ptr) { // Terima pointer
    syscall(5, (uint32_t)&c, (uint32_t)*col_ptr, (uint32_t)*row_ptr);
    (*col_ptr)++; // Update kolom
    if ((*col_ptr) >= 80) {
        (*col_ptr) = 0;
        (*row_ptr)++;
    }
}

void print_line(const char *str) // Tidak perlu argumen row karena menggunakan global current_output_row
{
    int temp_col = 0; // Mulai dari kolom 0
    if (current_output_row < 24) {
        // print_string ini harus mengupdate current_output_row dan temp_col
        print_string(str, &current_output_row, &temp_col);
    }
    // Pastikan baris benar-benar maju setelah satu baris penuh
    current_output_row++; // Ini sudah ada dan benar untuk memajukan baris setelah print_line
    // Jika string tidak diakhiri \n dan tidak mengisi penuh baris, ini akan memaksa baris baru.
}
uint32_t resolve_path(const char* path) {
    // Simple implementation - you'll need to enhance this
    if (strcmp(path, "/") == 0) {
        return 2; // Root inode
    }
    if (strcmp(path, "..") == 0) {
        return 2; // For now, go to root
    }
    // For other paths, return error
    return 9999; // Error indicator
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
// --- Built-in Command Implementations (Stubs for unsupported FS operations) ---
int handle_ls() { // Tidak perlu argumen row_ptr lagi
    // Header dicetak di userspace
    // print_line("name                type   size", &current_output_row);
    // print_line("================================", &current_output_row);

    // Panggil syscall, kirim ALAMAT dari global current_output_row
    // Agar kernel dapat membaca nilai awalnya dan menulis update-nya
    // current_output_row++;
    syscall(22, (uint32_t)&current_output_row, current_inode, 0);

    // Setelah syscall, `current_output_row` global sudah diupdate oleh kernel.
    return current_output_row; // Kembalikan nilai global yang sudah diupdate
}
void handle_clear() {
    // Clear screen using syscall
    syscall(8, 0, 0, 0);
    
    // Reset current output row to start
    current_output_row = 0;
     // Empty line before prompt
}
void handle_cd(const char* path) {

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
        print_line("Changed to root directory: /");
        return;
    }
    
    if (strcmp(path, "..") == 0 || strcmp(path, "../") == 0) {
        // Go to parent directory (simplified - for now just go to root)
        if (strcmp(current_working_directory, "/") != 0) {
            current_inode = 2; // Root inode
            strcpy(current_working_directory, "/");
            
            current_output_row++;
            print_line("Changed to root directory: /");
        } else {
            current_output_row++;
            print_line("Already at root directory");
        }
        return;
    }
    
    if (strcmp(path, ".") == 0) {
        // Stay in current directory
        int b = 0;
        print_string("Staying in current directory: ", &current_output_row, &b);
        print_string(current_working_directory, &current_output_row, &b);
        current_output_row++;
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
        int b = 0;
        print_string("cd: directory '", &current_output_row, &b);
        print_string(path, &current_output_row, &b);
        print_string("' not found", &current_output_row, &b);
        return;
    }
    
    // Check if target is actually a directory
    if (!is_directory) {
        current_output_row++;
        int b = 0;
        print_string("cd: '", &current_output_row, &b);
        print_string(path, &current_output_row, &b);
        print_string("' is not a directory", &current_output_row, &b);
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
    int b = 0;
    print_string("Changed directory to: ", &current_output_row, &b);
    print_string(current_working_directory, &current_output_row, &b);
    current_output_row++;
}

void handle_mkdir(const char* name) {
    if (!name || strlen(name) == 0) {
        print_line("mkdir: missing argument");
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

    int b = 0;
    if (result == 0) {
        print_string("Directory created: ", &current_output_row, &b);
        print_string(name, &current_output_row, &b);
        current_output_row++;
    } else if (result == 1) {
        print_string("mkdir: directory '", &current_output_row, &b);
        print_string(name, &current_output_row, &b);
        print_string("' already exists", &current_output_row, &b);
        current_output_row++;
    } else if (result == 2) {
        print_string("mkdir: parent is not a directory", &current_output_row, &b);
        current_output_row++;
    } else {
        print_string("mkdir: failed to create directory", &current_output_row, &b);
        current_output_row++;
    }
}


void handle_cat(const char* filename) {
    if (!filename || strlen(filename) == 0) {
        print_line("cat: missing argument");
        return;
    }

    // Siapkan request untuk membaca file
    struct EXT2DriverRequest request;
    memset(&request, 0, sizeof(request));
    
    request.parent_inode = current_inode;
    request.name_len = strlen(filename);
    
    // Copy nama file dengan bounds checking
    size_t copy_len = strlen(filename);
    if (copy_len >= sizeof(request.name)) {
        copy_len = sizeof(request.name) - 1;
    }
    for (size_t i = 0; i < copy_len; i++) {
        request.name[i] = filename[i];
    }
    request.name[copy_len] = '\0';
    
    // Alokasi buffer untuk membaca file
    // Maksimum ukuran file yang bisa ditampilkan (8KB untuk keamanan)
    const uint32_t max_file_size = 8192;
    static uint8_t file_buffer[8192];
    
    request.buf = file_buffer;
    request.buffer_size = max_file_size;
    request.is_directory = 0;
    
    // Panggil syscall untuk membaca file
    int8_t result;
    syscall(17, (uint32_t)&request, (uint32_t)&result, 0);
    
    int b = 0;
    
    if (result == 0) {
        // File berhasil dibaca, tampilkan isinya
        current_output_row++;
        
        // Dapatkan ukuran file yang sebenarnya
        // Kita perlu membaca inode untuk mendapatkan ukuran file
        uint32_t file_inode = 0;
        struct EXT2Inode parent_dir;
        read_inode(current_inode, &parent_dir);
        
        if (find_inode_in_dir(&parent_dir, filename, &file_inode)) {
            struct EXT2Inode file_node;
            read_inode(file_inode, &file_node);
            
            uint32_t file_size = file_node.i_size;
            if (file_size > max_file_size) {
                file_size = max_file_size;
            }
            
            // Tampilkan isi file karakter per karakter
            for (uint32_t i = 0; i < file_size; i++) {
                char c = file_buffer[i];
                
                if (c == '\n') {
                    // Newline - pindah ke baris baru
                    current_output_row++;
                    b = 0;  // Reset kolom
                    
                    // Cek apakah masih ada ruang di layar
                    if (current_output_row >= 23) {
                        print_string("-- More --", &current_output_row, &b);
                        // Bisa ditambahkan pause untuk user input
                        current_output_row++;
                        break;
                    }
                } else if (c == '\r') {
                    // Carriage return - abaikan (LF newline format)
                    continue;
                } else if (c >= 32 && c <= 126) {
                    // Karakter yang bisa ditampilkan
                    print_char(c, &current_output_row, &b);
                    
                    // Cek wrap around
                    if (b >= 80) {
                        current_output_row++;
                        b = 0;
                        
                        if (current_output_row >= 23) {
                            print_string("-- More --", &current_output_row, &b);
                            current_output_row++;
                            break;
                        }
                    }
                } else {
                    // Karakter kontrol lainnya - tampilkan sebagai '?'
                    print_char('?', &current_output_row, &b);
                    
                    if (b >= 80) {
                        current_output_row++;
                        b = 0;
                        
                        if (current_output_row >= 23) {
                            print_string("-- More --", &current_output_row, &b);
                            current_output_row++;
                            break;
                        }
                    }
                }
            }
            
            // Pastikan cursor berada di baris baru setelah selesai
            if (b > 0) {
                current_output_row++;
            }
            
        } else {
            print_string("cat: cannot determine file size", &current_output_row, &b);
            current_output_row++;
        }
        
    } else if (result == 1) {
        print_string("cat: '", &current_output_row, &b);
        print_string(filename, &current_output_row, &b);
        print_string("' is a directory", &current_output_row, &b);
        current_output_row++;
    } else if (result == 2) {
        print_string("cat: file '", &current_output_row, &b);
        print_string(filename, &current_output_row, &b);
        print_string("' is too large", &current_output_row, &b);
        current_output_row++;
    } else if (result == 3) {
        print_string("cat: '", &current_output_row, &b);
        print_string(filename, &current_output_row, &b);
        print_string("': No such file or directory", &current_output_row, &b);
        current_output_row++;
    } else {
        print_string("cat: error reading file '", &current_output_row, &b);
        print_string(filename, &current_output_row, &b);
        print_string("'", &current_output_row, &b);
        current_output_row++;
    }
}
void handle_help() {
    
    // Header
    print_line("LumaOS Shell - Available Commands:");
    // File system commands
    print_line("  ls                 - List directory contents");
    print_line("  cd <path>          - Change directory");
    print_line("  mkdir <name>       - Create directory");
    print_line("  cat <file>         - Display file contents");
    print_line("  cp <src> <dest>    - Copy file");
    print_line("  rm <file>          - Remove file");
    print_line("  mv <src> <dest>    - Move/rename file");
    print_line("  find <name>        - Search for file");
    
    // Process management commands
    print_line("  exec <file>        - Execute program");
    print_line("  ps                 - List running processes");
    print_line("  kill <pid>         - Terminate process by PID");
    
    // System commands
    print_line("  help               - Show this help message");
    print_line("  clear              - Clear the screen");
    print_line("  clock              - Enable clock display");
    print_line("  exit               - Exit the shell");
    
    // Audio commands
    print_line("  beep               - Play system beep");
    print_line("  stop_sound         - Stop audio playback");
    

}
void handle_cp(const char* source, const char* destination) {
    if (!source || strlen(source) == 0) {
        print_line("cp: missing source argument");
        return;
    }
    
    if (!destination || strlen(destination) == 0) {
        print_line("cp: missing destination argument");
        return;
    }

    // Prepare source request
    struct EXT2DriverRequest src_request;
    memset(&src_request, 0, sizeof(src_request));
    src_request.parent_inode = current_inode;
    src_request.name_len = strlen(source);
    src_request.is_directory = 0; // Assuming we're copying files, not directories
    src_request.buffer_size = 0; // Will be set by kernel based on file size
    src_request.buf = NULL; // Kernel will handle buffer allocation
    
    // Copy source name
    size_t src_copy_len = strlen(source);
    if (src_copy_len >= sizeof(src_request.name)) {
        src_copy_len = sizeof(src_request.name) - 1;
    }
    for (size_t i = 0; i < src_copy_len; i++) {
        src_request.name[i] = source[i];
    }
    src_request.name[src_copy_len] = '\0';

    // Prepare destination request
    struct EXT2DriverRequest dst_request;
    memset(&dst_request, 0, sizeof(dst_request));
    dst_request.parent_inode = current_inode;
    dst_request.name_len = strlen(destination);
    dst_request.is_directory = 0;
    dst_request.buffer_size = 0;
    dst_request.buf = NULL;
    
    // Copy destination name
    size_t dst_copy_len = strlen(destination);
    if (dst_copy_len >= sizeof(dst_request.name)) {
        dst_copy_len = sizeof(dst_request.name) - 1;
    }
    for (size_t i = 0; i < dst_copy_len; i++) {
        dst_request.name[i] = destination[i];
    }
    dst_request.name[dst_copy_len] = '\0';

    // Call copy file syscall (syscall 28)
    int8_t result;
    syscall(28, (uint32_t)&src_request, (uint32_t)&dst_request, (uint32_t)&result);

    // Handle result
    int col = 0;
    switch (result) {
        case 0:
            print_string("File copied successfully: ", &current_output_row, &col);
            print_string(source, &current_output_row, &col);
            print_string(" -> ", &current_output_row, &col);
            print_string(destination, &current_output_row, &col);
            current_output_row++;
            break;
        case -1:
            print_string("cp: source file '", &current_output_row, &col);
            print_string(source, &current_output_row, &col);
            print_string("' not found", &current_output_row, &col);
            current_output_row++;
            break;
        case -2:
            print_string("cp: destination file '", &current_output_row, &col);
            print_string(destination, &current_output_row, &col);
            print_string("' already exists", &current_output_row, &col);
            current_output_row++;
            break;
        case -3:
            print_string("cp: cannot copy directory '", &current_output_row, &col);
            print_string(source, &current_output_row, &col);
            print_string("' (use cp -r for directories)", &current_output_row, &col);
            current_output_row++;
            break;
        case -4:
            print_string("cp: failed to allocate inode for destination", &current_output_row, &col);
            current_output_row++;
            break;
        case -5:
            print_string("cp: failed to allocate blocks for destination", &current_output_row, &col);
            current_output_row++;
            break;
        case -6:
            print_string("cp: failed to read source file", &current_output_row, &col);
            current_output_row++;
            break;
        case -7:
            print_string("cp: failed to write destination file", &current_output_row, &col);
            current_output_row++;
            break;
        case -8:
            print_string("cp: parent directory is full", &current_output_row, &col);
            current_output_row++;
            break;
        default:
            print_string("cp: unknown error occurred", &current_output_row, &col);
            current_output_row++;
            break;
    }
}

void handle_rm(const char* path) {
    if (!path || strlen(path) == 0) {
        int b = 0;
        print_string("rm: missing file/directory name", &current_output_row, &b);
        current_output_row++;
        return;
    }

    // Special check: don't allow removal of . or ..
    if (strcmp(path, ".") == 0 || strcmp(path, "..") == 0 || strcmp(path, "../") == 0) {
        int b = 0;
        print_string("rm: cannot remove '.' or '..' or '../' directories", &current_output_row, &b);
        current_output_row++;
        return;
    }

    // Reset global variables
    rm_target_inode = 0;
    rm_target_found = 0;
    rm_is_directory = 0;

    // Use find_for_rm to locate the target in current directory
    find_for_rm(current_inode, path, &rm_target_found, &rm_is_directory, &rm_target_inode);
    
    if (!rm_target_found || rm_target_inode == 0) {
        int b = 0;
        print_string("rm: file/directory '", &current_output_row, &b);
        print_string(path, &current_output_row, &b);
        print_string("' not found", &current_output_row, &b);
        current_output_row++;
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
    
    // Report the result using global pointers
    int b = 0;
    if (result == 0) {
        print_string("rm: ", &current_output_row, &b);
        if (rm_is_directory) {
            print_string("directory '", &current_output_row, &b);
        } else {
            print_string("file '", &current_output_row, &b);
        }
        print_string(path, &current_output_row, &b);
        print_string("' removed successfully", &current_output_row, &b);
        current_output_row++;
    } else {
        print_string("rm: failed to remove ", &current_output_row, &b);
        if (rm_is_directory) {
            print_string("directory '", &current_output_row, &b);
        } else {
            print_string("file '", &current_output_row, &b);
        }
        print_string(path, &current_output_row, &b);
        
        // Provide more specific error messages based on result code
        switch (result) {
            case 1:
                print_string("' - already exists or permission denied", &current_output_row, &b);
                break;
            case 2:
                print_string("' - parent is not a directory", &current_output_row, &b);
                break;
            case 3:
                print_string("' - directory not empty", &current_output_row, &b);
                break;
            default:
                print_string("' - unknown error", &current_output_row, &b);
                break;
        }
        current_output_row++;
    }
}

void handle_mv(const char* source, const char* destination) {
    int b = 0; // Column pointer for print_string

    if (!source || strlen(source) == 0 || !destination || strlen(destination) == 0) {
        print_string("mv: missing source or destination argument (usage: mv source destination)", &current_output_row, &b);
        current_output_row++;
        return;
    }

    // Attempt to copy the source to the destination
    // Note: This only works for files. Moving directories is much more complex
    // and would require recursive operations and potentially changes to parent directory entries.
    print_string("mv: Attempting to move file (copy and then delete)...", &current_output_row, &b);
    current_output_row++;

    // Use handle_cp, which in turn calls SYS_COPY_FILE
    handle_cp(source, destination);

    print_string("Attempting to remove source file: ", &current_output_row, &b);
    print_string(source, &current_output_row, &b);
    current_output_row++;

    // Use handle_rm, which in turn calls SYS_DELETE_FILE or SYS_DELETE_DIR
    handle_rm(source);
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

void find_recursive(uint32_t curr_inode, const char* search_name, char* path, int* found) {
    // Enhanced safety checks
    if (*found || curr_inode == 0 || search_name == NULL || path == NULL) {
        return;
    }
    
    // Prevent deep recursion with static counter
    static int recursion_depth = 0;
    if (recursion_depth > 10) {
        return;
    }
    recursion_depth++;

    // Validate search_name is not empty
    if (search_name[0] == '\0') {
        recursion_depth--;
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
        recursion_depth--;
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

        if ((name_len == 1 && entry_name[0] == '.') ||
            (name_len == 2 && entry_name[0] == '.' && entry_name[1] == '.')) {
            offset += entry->rec_len;
            entries_processed++;
            continue;
        }

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
            char result_path[512];
            unsigned int path_pos = 0;
            
            while (path[path_pos] != '\0' && path_pos < 400) {
                result_path[path_pos] = path[path_pos];
                path_pos++;
            }
            
            if (path_pos > 1 || path[0] != '/') {
                if (path_pos < 510) {
                    result_path[path_pos++] = '/';
                }
            }
            
            for (unsigned int i = 0; i < name_len && path_pos < 511; i++) {
                result_path[path_pos++] = entry_name[i];
            }
            result_path[path_pos] = '\0';

            struct EXT2Inode found_inode;
            char* found_ptr = (char*)&found_inode;
            for (unsigned int i = 0; i < sizeof(struct EXT2Inode); i++) {
                found_ptr[i] = 0;
            }
            syscall(21, (uint32_t)&found_inode, entry->inode, 0);
            
            // ✅ FIXED: Use global pointer variables
            int b = 0;
            if (current_output_row < 22) {
                if (found_inode.i_mode & 0x4000) {
                    print_string("FOUND DIR: ", &current_output_row, &b);
                    print_string(result_path, &current_output_row, &b);
                    current_output_row++;
                } else {
                    print_string("FOUND FILE: ", &current_output_row, &b);
                    print_string(result_path, &current_output_row, &b);
                    current_output_row++;
                }
            }
            *found = 1;
            recursion_depth--;
            return;
        }

        if (entry->inode != curr_inode && entry->inode > 1) {
            struct EXT2Inode child_inode;
            char* child_ptr = (char*)&child_inode;
            for (unsigned int i = 0; i < sizeof(struct EXT2Inode); i++) {
                child_ptr[i] = 0;
            }
            
            syscall(21, (uint32_t)&child_inode, entry->inode, 0);
            
            if ((child_inode.i_mode & 0x4000) == 0x4000 && 
                child_inode.i_size > 0 && child_inode.i_size < 65536 &&
                child_inode.i_block[0] != 0) {
                
                char new_path[512];
                unsigned int new_path_pos = 0;
                
                while (path[new_path_pos] != '\0' && new_path_pos < 400) {
                    new_path[new_path_pos] = path[new_path_pos];
                    new_path_pos++;
                }
                
                if (new_path_pos > 1 || path[0] != '/') {
                    if (new_path_pos < 510) {
                        new_path[new_path_pos++] = '/';
                    }
                }
                
                for (unsigned int i = 0; i < name_len && new_path_pos < 511; i++) {
                    new_path[new_path_pos++] = entry_name[i];
                }
                new_path[new_path_pos] = '\0';

                find_recursive(entry->inode, search_name, new_path, found);
            }
        }

        offset += entry->rec_len;
        entries_processed++;
    }
    
    recursion_depth--;
}

void handle_find(const char* filename) {
    if (filename == NULL || filename[0] == '\0') {
        int b = 0;
        print_string("find: usage: find <filename>", &current_output_row, &b);
        current_output_row++;
        return;
    }
    
    unsigned int filename_len = 0;
    while (filename[filename_len] != '\0' && filename_len < 255) {
        char c = filename[filename_len];
        if (c < 32 || c > 126) {
            int b = 0;
            print_string("find: invalid character in filename", &current_output_row, &b);
            current_output_row++;
            return;
        }
        filename_len++;
    }
    
    if (filename_len == 0 || filename_len > 255) {
        int b = 0;
        print_string("find: invalid filename length", &current_output_row, &b);
        current_output_row++;
        return;
    }
    
    if (current_output_row >= 18) {
        int b = 0;
        print_string("find: insufficient screen space", &current_output_row, &b);
        current_output_row++;
        return;
    }
    
    int b = 0;
    print_string("Searching for: ", &current_output_row, &b);
    print_string(filename, &current_output_row, &b);
    current_output_row++;

    char root_path[3] = {'/', '\0', '\0'};
    int found = 0;
    
    find_recursive(2, filename, root_path, &found);

    if (!found) {
        b = 0;
        print_string("File or directory not found", &current_output_row, &b);
        current_output_row++;
    }
}