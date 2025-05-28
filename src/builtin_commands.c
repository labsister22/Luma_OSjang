// src/builtin_commands.c

#include "header/shell/builtin_commands.h"
#include "header/stdlib/string.h" // Now strictly adhering to provided functions only
#include "header/filesystem/ext2.h" // For EXT2 structures
#include "header/shell/file_operations.h" // For file operations

// No custom string functions here, relying strictly on header/stdlib/string.h
extern char current_working_directory[256];

// Buffer statis untuk operasi filesystem - mengganti malloc
//static char fs_buffer[4096];

// Forward declaration untuk fungsi yang dibutuhkan
uint32_t get_inode_by_path(const char* path);

// Implementation of get_inode_by_path using syscalls only
uint32_t get_inode_by_path(const char* path) {
    if (!path || strlen(path) == 0) {
        return 2; // Root inode
    }
    
    // If path is just "/", return root
    if (strcmp(path, "/") == 0) {
        return 2;
    }
    
    // For simplicity in user space, we'll assume the current working directory
    // inode is available through the shell state. In a full implementation,
    // we'd need additional syscalls to navigate the directory tree.
    // For now, just return root inode 2 as a safe default.
    return 2;
}
void print_string(const char* str, int row, int col) {
    syscall(6, (uint32_t)str, row, col);
}

void print_char(char c, int row, int col) {
    syscall(5, (uint32_t)&c, col, row);
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

// --- Built-in Command Implementations ---

void handle_cd(const char* path, int current_row) {
    char resolved_path[256];
    resolve_path_display(resolved_path, path);
    strcpy(current_working_directory, resolved_path); // Update the global CWD
    print_string(resolved_path, current_row + 1, 0);
}

void handle_ls(int current_row) {
    // print_string("Listing:", current_row + 1, 0);
    // // `list_directory_contents` sudah mencetak outputnya sendiri.
    // // Argument `NULL` berarti list CWD (atau root jika CWD di kernel belum diimplementasikan).
    // //list_directory_contents(NULL);
    // current_row++; // Pindah ke baris berikutnya untuk output pertama
    // print_string("Listing /:", current_row, 0);
    // current_row++;

    // struct EXT2Inode root_inode;
    
    // // Mengambil inode untuk root direktori (inode 2)
    // syscall(21, (uint32_t)&root_inode, 2, 0); // Syscall 21: SYS_READ_INODE
    // char debug_msg[50];
    // itoa(root_inode.i_mode, debug_msg);
    // print_string("Debug i_mode: ", current_row, 0);
    // print_string(debug_msg, current_row, 15);
    // current_row++;

    // itoa(root_inode.i_size, debug_msg);
    // print_string("Debug i_size: ", current_row, 0);
    // print_string(debug_msg, current_row, 15);
    // current_row++;
    // if (root_inode.i_mode == 0) {
    //     print_string("Error: Could not read root inode.", current_row, 0);
    //     current_row++;
    //     return;
    // }

    // // Periksa apakah ini direktori (mode direktori EXT2 adalah 0x4000)
    // if (!((root_inode.i_mode & 0xF000) == 0x4000)) {
    //     print_string("Error: Root inode is not a directory.", current_row, 0);
    //     current_row++;
    //     return;
    // }

    // if (root_inode.i_size == 0) {
    //     print_string("Root directory is empty.", current_row, 0);
    //     current_row++;
    //     return;
    // }

    // // **PENTING**: Sesuaikan FS_BLOCK_SIZE dengan ukuran blok filesystem Anda (umumnya 1024 untuk EXT2)
    // // Buffer `buf` di kode Anda adalah 512. Jika syscall(23, ..., 1) membaca 1 blok FS penuh
    // // yang lebih besar dari 512, akan terjadi buffer overflow.
    // // Untuk sementara, kita gunakan 512 sesuai buffer Anda, tapi ini berisiko jika blok FS lebih besar.
    // #define LS_BUFFER_BLOCK_SIZE 512
    // uint8_t block_buffer[LS_BUFFER_BLOCK_SIZE]; // Buffer sesuai dengan yang Anda gunakan
    // char entry_name_buffer[256 + 1]; // Max name_len + null terminator (name_len Anda uint16_t, tapi kita batasi praktis)

    // itoa(root_inode.i_block[0], debug_msg);
    // print_string("Debug i_block[0]: ", current_row, 0);
    // print_string(debug_msg, current_row, 18);
    // current_row++;
    // // Iterasi melalui direct blocks (i_block[0] hingga i_block[11])
    // for (int i = 0; i < 12; ++i) {
    //     if (root_inode.i_block[i] == 0) {
    //         continue; // Blok tidak digunakan
    //     }

    //     // Baca satu "unit" blok (sesuai syscall 23 Anda, yang mungkin 512 byte jika count=1)
    //     syscall(23, (uint32_t)block_buffer, root_inode.i_block[i], 1);
        
    //     // Safely check if block was read successfully
    //     if (i == 0) {
    //         // Simple validation - check if first 4 bytes are reasonable for an inode number
    //         uint32_t* first_inode = (uint32_t*)block_buffer;
    //         if (*first_inode == 0) {
    //             print_string("Warning: Block appears empty", current_row, 0);
    //             current_row++;
    //             continue;
    //         }
    //     }

    //     uint32_t offset = 0;
    //     while (offset < LS_BUFFER_BLOCK_SIZE) {
    //         // Pointer ke awal entri direktori saat ini
    //         struct EXT2DirectoryEntry* entry = (struct EXT2DirectoryEntry*)(block_buffer + offset);

    //         // Periksa validitas rec_len
    //         // Ukuran minimum bagian tetap adalah 9 byte. rec_len harus >= itu dan kelipatan 4.
    //         // Smallest possible rec_len is for name_len=1: 9 bytes fixed + 1 byte name = 10. Rounded to 4 = 12.
    //         if (entry->rec_len == 0 || entry->rec_len < 12 ) { // Cek rec_len minimal
    //             break; // Akhir dari daftar entri di blok ini atau korup
    //         }
    //          // Pastikan rec_len tidak menyebabkan pembacaan melebihi buffer
    //         if (offset + entry->rec_len > LS_BUFFER_BLOCK_SIZE) {
    //             break;
    //         }

    //         // Periksa apakah entri valid (inode bukan 0 dan ada nama)
    //         if (entry->inode != 0 && entry->name_len > 0) {
    //             // Salin nama entri dengan hati-hati
    //             // name_len Anda uint16_t, bisa sangat besar jika ada kesalahan baca dari disk
    //             // atau jika format disk sebenarnya uint8_t. Batasi untuk keamanan buffer.
    //             uint16_t current_name_len = entry->name_len;
    //             if (current_name_len > 255) {
    //                 current_name_len = 255; // Batasi agar muat di entry_name_buffer
    //             }

    //             // **AKSES NAMA YANG BENAR**: Nama dimulai setelah 9 byte pertama dari struct Anda
    //             char* name_source = ((char*)entry) + 9;
                
    //             for (int k = 0; k < current_name_len; ++k) {
    //                 entry_name_buffer[k] = name_source[k];
    //             }
    //             entry_name_buffer[current_name_len] = '\0'; // **NULL TERMINATE PENTING!**

    //             print_string(entry_name_buffer, current_row, 2); // Cetak dengan sedikit indentasi
    //             current_row++;

    //             if (current_row >= 24) { // Hindari overflow layar (baris 0-24)
    //                 print_string("--More--", current_row, 0);
    //                 // Idealnya ada mekanisme paging di sini
    //                 return; // Keluar jika layar penuh untuk menghindari masalah
    //             }
    //         }
    //         offset += entry->rec_len; // Pindah ke entri berikutnya
    //     }
    // }
    (void) current_row;
    // Anda mungkin perlu menangani indirect blocks jika direktori sangat besar.
}

void handle_mkdir(const char* name, int current_row) {
    syscall(25, (uint32_t)current_working_directory, (uint32_t)name, (uint32_t)current_row); // Assuming syscall 25 for creating directory
}

void handle_cat(const char* filename, int current_row) {
    syscall(26, (uint32_t)current_working_directory, (uint32_t)filename, (uint32_t)current_row); // Assuming syscall 26 for reading file
}

void handle_cp(const char* source, const char* destination, int current_row) {
    print_string("cp: requires two arguments, rudimentary support only.", current_row + 1, 0);
    syscall(27, (uint32_t)current_working_directory, (uint32_t)source, (uint32_t)destination); // Assuming syscall 27 for copying file
}

void handle_rm(const char* path, int current_row) {
    syscall(28, (uint32_t)current_working_directory, (uint32_t)path, (uint32_t)current_row); // Assuming syscall 28 for removing file/directory
}

void handle_mv(const char* source, const char* destination, int current_row) {
    print_string("mv: requires two arguments, rudimentary support only.", current_row + 1, 0);
    syscall(29, (uint32_t)current_working_directory, (uint32_t)source, (uint32_t)destination); // Assuming syscall 29 for moving/renaming
}

void handle_find(const char* name, int current_row) {
    syscall(30, (uint32_t)current_working_directory, (uint32_t)name, (uint32_t)current_row); // Assuming syscall 30 for finding file
}

// Utility functions (from user-shell.c / shared)
void print_string(const char* str, int row, int col);
void print_char(char c, int row, int col);