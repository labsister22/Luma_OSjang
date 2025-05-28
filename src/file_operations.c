// src/file_operations.c

#include "header/shell/file_operations.h"
#include "header/stdlib/string.h"    // Menggunakan string.h kustom Anda (sekarang termasuk strncpy)
#include "header/filesystem/ext2.h"  // Untuk struct EXT2DriverRequest
#include <stdio.h>    // Untuk print_string/print_char

// Definisi syscall (sesuai dengan yang ada di user-shell.c)
void syscall(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx);

// Syscall untuk mencetak string (diperlukan di sini karena file_operations mungkin perlu mencetak pesan error)
extern void print_string(const char* str, int row, int col);


// --- Implementasi Fungsi-fungsi File Operations ---

// Syscall 12: SYS_CD
int change_directory(const char* path) {
    int8_t status = -1; // Default to failure
    // Parameter `ecx` untuk `out_cwd_buffer` opsional, bisa NULL jika tidak ingin CWD baru dikembalikan
    syscall(12, (uint32_t)path, (uint32_t)NULL, (uint32_t)&status);
    return (int)status; // Konversi int8_t ke int
}

// Syscall 13: SYS_GET_CWD
int get_current_working_directory_kernel(char* buffer, uint32_t buffer_size) {
    int8_t status = -1;
    syscall(13, (uint32_t)buffer, buffer_size, (uint32_t)&status);
    return (int)status;
}


// Syscall 1: SYS_WRITE (untuk membuat direktori)
int make_directory(const char *path) {
    struct EXT2DriverRequest req;
    memset(&req, 0, sizeof(req));
    // Asumsi parent_inode adalah root (inode 2) untuk kesederhanaan.
    // Nanti, Anda harus mendapatkan parent_inode dari CWD aktual.
    req.parent_inode = 2; // Default ke root
    strncpy(req.name, path, sizeof(req.name) - 1);
    req.name[sizeof(req.name) - 1] = '\0'; // Pastikan null-terminated
    req.name_len = strlen(req.name);
    req.is_directory = true;
    req.buf = NULL; // Tidak ada data untuk direktori
    req.buffer_size = 0;

    int8_t status = 0;
    syscall(1, (uint32_t)&req, (uint32_t)&status, 0);
    return (int)status;
}

// Syscall 2: SYS_DELETE (untuk menghapus file)
int remove_file(const char *path) {
    struct EXT2DriverRequest req;
    memset(&req, 0, sizeof(req));
    req.parent_inode = 2; // Default ke root
    strncpy(req.name, path, sizeof(req.name) - 1);
    req.name[sizeof(req.name) - 1] = '\0';
    req.name_len = strlen(req.name);
    req.is_directory = false; // Asumsi file

    int8_t status = 0;
    syscall(2, (uint32_t)&req, (uint32_t)&status, 0);
    return (int)status;
}

// Syscall 2: SYS_DELETE (untuk menghapus direktori)
int remove_directory(const char *path) {
    struct EXT2DriverRequest req;
    memset(&req, 0, sizeof(req));
    req.parent_inode = 2; // Default ke root
    strncpy(req.name, path, sizeof(req.name) - 1);
    req.name[sizeof(req.name) - 1] = '\0';
    req.name_len = strlen(req.name);
    req.is_directory = true; // Asumsi direktori

    int8_t status = 0;
    syscall(2, (uint32_t)&req, (uint32_t)&status, 0);
    return (int)status;
}


// Implementasi `copy_file` (membutuhkan SYS_READ dan SYS_WRITE)
int copy_file(const char *source_path, const char *dest_path) {
    // PENTING: Buffer ini harus dialokasikan di userspace atau menjadi global/static
    // karena kita tidak punya malloc di userspace LumaOS.
    // Ukuran buffer harus cukup besar untuk file terbesar yang ingin Anda salin.
    // Jika file lebih besar dari buffer, ini akan gagal atau rusak.
    static uint8_t file_content_buffer[BLOCK_SIZE * 15]; // Misal 15 blok = 15KB

    struct EXT2DriverRequest read_req;
    memset(&read_req, 0, sizeof(read_req));
    read_req.parent_inode = 2; // Asumsi root. Ini perlu diubah ke CWD yang sesungguhnya.
    strncpy(read_req.name, source_path, sizeof(read_req.name) - 1);
    read_req.name[sizeof(read_req.name) - 1] = '\0';
    read_req.name_len = strlen(read_req.name);
    read_req.buf = file_content_buffer;
    read_req.buffer_size = sizeof(file_content_buffer);

    int8_t read_status = 0;
    syscall(0, (uint32_t)&read_req, (uint32_t)&read_status, 0); // SYS_READ

    if (read_status != 0) {
        print_string("cp: Failed to read source file.", 0, 0); // Koordinat dummy, sesuaikan
        return -1;
    }

    struct EXT2DriverRequest write_req;
    memset(&write_req, 0, sizeof(write_req));
    write_req.parent_inode = 2; // Asumsi root. Ini perlu diubah ke CWD yang sesungguhnya.
    strncpy(write_req.name, dest_path, sizeof(write_req.name) - 1);
    write_req.name[sizeof(write_req.name) - 1] = '\0';
    write_req.name_len = strlen(write_req.name);
    write_req.buf = file_content_buffer;
    write_req.buffer_size = read_req.buffer_size; // Gunakan ukuran yang benar-benar dibaca

    int8_t write_status = 0;
    syscall(1, (uint32_t)&write_req, (uint32_t)&write_status, 0); // SYS_WRITE

    if (write_status != 0) {
        print_string("cp: Failed to write destination file.", 0, 0); // Koordinat dummy, sesuaikan
        return -1;
    }
    return 0;
}

// Implementasi `move_file` (menggunakan copy dan remove)
int move_file(const char *source_path, const char *dest_path) {
    if (copy_file(source_path, dest_path) != 0) {
        print_string("mv: Failed to copy source to destination.", 0, 0);
        return -1;
    }
    if (remove_file(source_path) != 0) {
        print_string("mv: Failed to remove original source file.", 0, 0);
        return -1;
    }
    return 0;
}

// Implementasi `list_directory_contents` (membutuhkan SYS_LS_DIR)
void list_directory_contents(const char *path) {
    (void)path; // Menandai parameter 'path' sebagai tidak digunakan

    // PENTING: Buffer ini juga statis dan harus cukup besar.
    // Asumsi: Setiap nama file maks 256 byte, dan ada maks 100 file.
    static char file_list_buffer[256 * 10];
    uint32_t file_count = 0;

    struct EXT2DriverRequest req;
    memset(&req, 0, sizeof(req));
    // Untuk `ls .` atau `ls`, gunakan CWD aktual dari kernel jika ada syscall GET_CWD,
    // atau untuk kesederhanaan saat ini, asumsikan parent_inode 2 (root).
    req.parent_inode = 2; // Default ke root untuk listing


    int8_t status = 0;
    // SYS_LS_DIR (syscall 3)
    // EAX=3, EBX=&req, ECX=output_buffer, EDX=&file_count
    syscall(3, (uint32_t)&req, (uint32_t)file_list_buffer, (uint32_t)&file_count);

    if (status == 0) {
        char *current_name_ptr = file_list_buffer;
        int current_display_row = 0; // Mulai dari baris tertentu
        for (uint32_t i = 0; i < file_count; i++) {
            print_string(current_name_ptr, current_display_row + 1, 0); // Cetak nama file
            current_display_row++;
            // Pindahkan pointer ke nama file berikutnya (tambah 1 untuk null terminator)
            current_name_ptr += (strlen(current_name_ptr) + 1);
            if (current_display_row >= 24) { // Hindari overflow layar
                // Anda mungkin ingin menambahkan logic untuk mempause atau menggulir
                break;
            }
        }
    } else {
        print_string("ls: Error listing directory.", 0, 0); // Koordinat dummy, sesuaikan
        // Tambahkan pesan error spesifik berdasarkan `status`
        if (status == -1) print_string("ls: Invalid arguments.", 1, 0);
        else if (status == -2) print_string("ls: Directory not found.", 1, 0);
    }
}

// Implementasi `display_file_contents` (membutuhkan SYS_READ)
void display_file_contents(const char *path) {
    // PENTING: Buffer ini juga statis dan harus cukup besar.
    static uint8_t file_content_buffer[BLOCK_SIZE * 2]; // Misal 15KB

    struct EXT2DriverRequest req;
    memset(&req, 0, sizeof(req));
    req.parent_inode = 2; // Asumsi root. Ini perlu diubah ke CWD yang sesungguhnya.
    strncpy(req.name, path, sizeof(req.name) - 1);
    req.name[sizeof(req.name) - 1] = '\0';
    req.name_len = strlen(req.name);
    req.buf = file_content_buffer;
    req.buffer_size = sizeof(file_content_buffer);

    int8_t status = 0;
    syscall(0, (uint32_t)&req, (uint32_t)&status, 0); // SYS_READ (syscall 0)

    if (status == 0) {
        // Tampilkan konten file
        // Asumsikan file_content_buffer berisi teks ASCII yang dapat dicetak
        print_string((const char*)file_content_buffer, 0, 0); // Koordinat dummy, sesuaikan
    } else {
        print_string("cat: Error reading file.", 0, 0); // Koordinat dummy, sesuaikan
        // Tambahkan pesan error spesifik
        if (status == 1) print_string("cat: Is a directory.", 1, 0);
        else if (status == 2) print_string("cat: Buffer too small.", 1, 0);
        else if (status == 3) print_string("cat: File not found.", 1, 0);
    }
}