// src/builtin_commands.c

#include "header/shell/builtin_commands.h"
#include "header/stdlib/string.h"
#include "header/shell/file_operations.h" // Perlu menyertakan ini untuk fungsi-fungsi dari file_operations.c
// #include "header/stdlib/stdio.h" // Ini mungkin tidak perlu jika print_string di extern

// Global current working directory (ini dari user-shell.c, asumsikan bisa diakses)
// extern char current_working_directory[256];

// Deklarasi syscall (sesuai dengan yang ada di user-shell.c)
void syscall(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx);

// Fungsi print_string dan print_char yang memanggil syscall
void print_string(const char* str, int row, int col) {
    syscall(6, (uint32_t)str, row, col);
}

void print_char(char c, int row, int col) {
    syscall(5, (uint32_t)&c, col, row);
}

// --- Helper for path concatenation (Purely for shell display, not actual FS changes) ---
// Bagian ini tidak berubah, Anda bisa membiarkannya seperti yang Anda berikan.
// ... (resolve_path_display function content) ...
void resolve_path_display(char* resolved_path, const char* path) {
    char temp_path[256];
    strcpy(temp_path, "");

    char *segment_start;
    char *ptr;

    if (path[0] == '/') {
        strcpy(temp_path, path);
        resolved_path[0] = '/';
        resolved_path[1] = '\0';
    } else {
        strcpy(temp_path, current_working_directory);
        if (strlen(temp_path) > 1 && temp_path[strlen(temp_path) - 1] != '/') {
            strcat(temp_path, "/");
        }
        strcat(temp_path, path);
    }

    char normalized_path[256];
    strcpy(normalized_path, "/");

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

        if (len == 2 && segment_start[0] == '.' && segment_start[1] == '.') {
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
        } else if (len == 1 && segment_start[0] == '.') {
            // Do nothing
        } else {
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
}


// --- Built-in Command Implementations ---

void handle_cd(const char* path, int current_row) {
    int status = change_directory(path); // Panggil fungsi wrapper dari file_operations
    if (status == 0) {
        // Jika kernel berhasil mengubah CWD, perbarui display CWD di shell
        // Anda perlu syscall `SYS_GET_CWD` jika ingin kernel yang mengelola CWD sepenuhnya.
        // Untuk saat ini, kita bisa menggunakan `resolve_path_display` hanya untuk tampilan.
        char new_display_path[256];
        resolve_path_display(new_display_path, path);
        strcpy(current_working_directory, new_display_path); // Update CWD for shell prompt
        print_string("Directory ", current_row + 1, 0);
        print_string(current_working_directory, current_row + 1, strlen("Directory "));
    } else {
        print_string("cd: Fail ", current_row + 1, 0);
        // Tambahkan pesan error yang lebih spesifik berdasarkan status
        if (status == -1) print_string("Path not found.", current_row + 2, 0);
    }
}

void handle_ls(int current_row) {
    print_string("Listing:", current_row + 1, 0);
    // `list_directory_contents` sudah mencetak outputnya sendiri.
    // Argument `NULL` berarti list CWD (atau root jika CWD di kernel belum diimplementasikan).
    list_directory_contents(NULL);
}

void handle_mkdir(const char* name, int current_row) {
    int status = make_directory(name); // Panggil fungsi wrapper dari file_operations
    if (status == 0) {
        print_string("mkdir: Success", current_row + 1, 0);
    } else {
        print_string("mkdir: Fail ", current_row + 1, 0);
        // Tambahkan pesan error spesifik jika status memberikan lebih banyak info
    }
}

void handle_cat(const char* filename, int current_row) {
    print_string("File content:", current_row + 1, 0);
    display_file_contents(filename); // Panggil fungsi wrapper dari file_operations
}

void handle_cp(const char* source, const char* destination, int current_row) {
    int status = copy_file(source, destination); // Panggil fungsi wrapper dari file_operations
    if (status == 0) {
        print_string("cp: Success", current_row + 1, 0);
    } else {
        print_string("cp: Fail", current_row + 1, 0);
    }
}

void handle_rm(const char* path, int current_row) {
    int status = remove_file(path); // Panggil fungsi wrapper dari file_operations
    if (status == 0) {
        print_string("rm: Success", current_row + 1, 0);
    } else {
        print_string("rm: Fail", current_row + 1, 0);
        // Tambahkan pesan error spesifik jika status memberikan lebih banyak info
    }
}

void handle_mv(const char* source, const char* destination, int current_row) {
    int status = move_file(source, destination); // Panggil fungsi wrapper dari file_operations
    if (status == 0) {
        print_string("mv: Success", current_row + 1, 0);
    } else {
        print_string("mv: Fail", current_row + 1, 0);
    }
}

void handle_find(const char* name, int current_row) {
    // Implementasi find akan sangat kompleks tanpa kemampuan rekursif
    // dan filesystem API yang lebih luas (seperti membaca semua direktori).
    // Untuk saat ini, ini akan tetap menjadi stub.
    print_string("find: ", current_row + 1, 0);
    (void)name; // Suppress unused parameter warning
}

// handle_exit sekarang di handle langsung di user-shell.c, tetapi
// jika Anda ingin syscall exit (misal, untuk mematikan QEMU), bisa tambahkan ini:
void handle_exit(int current_row) {
    print_string("Goodbye!", current_row, 0);
    // Ini akan memanggil syscall 11.
    // Jika Anda ingin mematikan QEMU, syscall 11 di `process.c` Anda melakukan `qemu_exit()`.
    syscall(11, 0, 0, 0); // Asumsi 0 adalah PID shell atau sinyal qemu_exit
}


// Fungsi `handle_builtin_command_execution` tidak diperlukan lagi
// karena `process_command` di `user-shell.c` sudah memanggil `handle_xxx` secara langsung.