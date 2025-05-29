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


// --- Built-in Command Implementations (Stubs for unsupported FS operations) ---

void handle_cd(const char *path) {
    if (!path || strlen(path) == 0) {
        int b = 0;
        print_string("Error: Missing argument", &current_output_row, &b);
        current_output_row++;
        return;
    }

    uint32_t target_inode = resolve_path(path);

    if (target_inode == 9999) {
        int b = 0;
        print_string("Error: Path not found", &current_output_row, &b);
        current_output_row++;
        return;
    }

    // Update current directory
    current_inode = target_inode;
    syscall(18, current_inode, (uint32_t)path, 1);

    // Update working directory string using resolve_path_display
    resolve_path_display(current_working_directory, path);
    
    int b = 0;
    print_string("Directory changed", &current_output_row, &b);
    current_output_row++;
}
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

void handle_mkdir(const char* name) {
    // print_string("mkdir: Not implemented. No kernel syscall for creating directories.", current_row+1, 0);
    (void)name;
}

void handle_cat(const char* filename) {
    // print_string("cat: Not implemented. No suitable kernel syscall for file reading.", current_row+1, 0);
    (void)filename;
}

void handle_cp(const char* source, const char* destination) {
    // print_string("cp: requires two arguments, not supported with current string functions.", current_row+1, 0);
    // print_string("cp: Not implemented. No kernel syscalls for file copying.", current_row + 2, 0);
    (void)source;
    (void)destination;
}

void handle_rm(const char* path) {
    // print_string("rm: Not implemented. No kernel syscall for removing files/directories.", current_row+1, 0);
    (void)path;
}

void handle_mv(const char* source, const char* destination) {
    // print_string("mv: requires two arguments, not supported with current string functions.", current_row+1, 0);
    // print_string("mv: Not implemented. No kernel syscall for moving/renaming files.", current_row + 2, 0);
    (void)source;
    (void)destination;
}

void handle_find(const char* name) {
    // print_string("find: Not implemented. No kernel syscall for recursive file search.", current_row+1, 0);
    (void)name;
}