#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "header/filesystem/ext2.h"
#include "header/driver/disk.h"
#include "header/stdlib/string.h"

// Global variable
uint8_t *image_storage;
uint8_t *file_buffer;
uint8_t *read_buffer;

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

void read_blocks(void *ptr, uint32_t logical_block_address, uint8_t block_count) {
    for (int i = 0; i < block_count; i++) {
        memcpy(
            (uint8_t*) ptr + BLOCK_SIZE*i, 
            image_storage + BLOCK_SIZE*(logical_block_address+i), 
            BLOCK_SIZE
        );
    }
}

void write_blocks(const void *ptr, uint32_t logical_block_address, uint8_t block_count) {
    for (int i = 0; i < block_count; i++) {
        memcpy(
            image_storage + BLOCK_SIZE*(logical_block_address+i), 
            (uint8_t*) ptr + BLOCK_SIZE*i, 
            BLOCK_SIZE
        );
    }
}

// Tambahkan fungsi berikut setelah write_blocks
void ensure_filesystem_exists(void) {
    // Cek apakah filesystem sudah ada
    if (is_empty_storage()) {
        printf("Filesystem belum ada, melakukan format...\n");
        create_ext2();
        printf("Format selesai.\n");
    } else {
        printf("Filesystem EXT2 terdeteksi.\n");
    }
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "inserter: ./inserter <file to insert> <parent cluster index> <storage>\n");
        exit(1);
    }

    // Read storage into memory, requiring 4 MB memory
    image_storage = malloc(4*1024*1024);
    if (image_storage == NULL) {
        perror("Gagal mengalokasikan memori untuk image_storage");
        exit(1);
    }
    
    file_buffer = malloc(4*1024*1024);
    if (file_buffer == NULL) {
        perror("Gagal mengalokasikan memori untuk file_buffer");
        free(image_storage);
        exit(1);
    }
    
    read_buffer = malloc(4*1024*1024);
    if (read_buffer == NULL) {
        perror("Gagal mengalokasikan memori untuk read_buffer");
        free(image_storage);
        free(file_buffer);
        exit(1);
    }
    
    FILE *fptr = fopen(argv[3], "rb");
    if (fptr == NULL) {
        perror("Tidak dapat membuka file storage");
        free(image_storage);
        free(file_buffer);
        free(read_buffer);
        exit(1);
    }
    
    // Baca berdasarkan elemen 1 byte sebanyak 4MB
    size_t bytes_read = fread(image_storage, 1, 4*1024*1024, fptr);
    printf("Membaca %zu bytes dari storage\n", bytes_read);
    fclose(fptr);

    // Read target file, assuming file is less than 4 MiB
    FILE *fptr_target = fopen(argv[1], "rb");
    size_t filesize = 55;
    if (fptr_target == NULL) {
        printf("Peringatan: Tidak dapat membuka file %s - akan membuat file kosong\n", argv[1]);
        filesize = 0;
    } else {
        fseek(fptr_target, 0, SEEK_END);
        filesize = ftell(fptr_target);
        fseek(fptr_target, 0, SEEK_SET);
        
        if (filesize > 0) {
            size_t read_size = fread(file_buffer, 1, filesize, fptr_target);
            if (read_size != filesize) {
                printf("Peringatan: Hanya membaca %zu dari %zu bytes\n", read_size, filesize);
                filesize = read_size;
            }
        }
        fclose(fptr_target);
    }

    printf("Filename : %s\n", argv[1]);
    printf("Filesize : %ld bytes\n", filesize);

    // EXT2 operations
    printf("Menginisialisasi filesystem EXT2...\n");
    initialize_filesystem_ext2();
    
    // Pastikan filesystem sudah ada
    ensure_filesystem_exists();
    
    char *name = argv[1];
    struct EXT2DriverRequest request;
    memset(&request, 0, sizeof(request));  // Penting! Inisialisasi struktur
    
    request.buf = file_buffer;
    request.buffer_size = filesize;
    
    // FIX: Salin string ke field name alih-alih menetapkan pointer
    // Pastikan request.name adalah array karakter dalam struct EXT2DriverRequest, bukan pointer
    strncpy(request.name, name, sizeof(request.name) - 1);
    request.name[sizeof(request.name) - 1] = '\0';  // Pastikan null-terminated
    
    request.name_len = strlen(name);
    request.is_directory = FALSE;
    
    if (sscanf(argv[2], "%u", &request.parent_inode) != 1) {
        printf("Error: Parent inode harus berupa angka\n");
        free(image_storage);
        free(file_buffer);
        free(read_buffer);
        exit(1);
    }
    
    printf("Filename       : %s\n", request.name);
    printf("Filename length: %ld\n", (long)request.name_len);
    printf("Parent inode: %u\n", request.parent_inode);
    
    // Coba baca file yang sudah ada dengan hati-hati
    printf("Mencoba membaca file yang sudah ada...\n");
    struct EXT2DriverRequest reqread;
    memset(&reqread, 0, sizeof(reqread));  // Inisialisasi struktur baru

    // Buat salinan nama file untuk memastikan batas yang jelas
    strncpy(reqread.name, name, sizeof(reqread.name) - 1);
    reqread.name[sizeof(reqread.name) - 1] = '\0';  // Pastikan null-terminated
    
    reqread.buf = read_buffer;
    reqread.buffer_size = 4*1024*1024;  // Beri tahu fungsi read berapa banyak ruang yang tersedia
    reqread.name_len = strlen(reqread.name);
    reqread.parent_inode = request.parent_inode;
    reqread.is_directory = request.is_directory;

    printf("Debug - Membaca: parent=%u, name=%s, len=%lu\n", 
       reqread.parent_inode, reqread.name, (unsigned long)reqread.name_len);
    
    int retcode = -1;
    printf("[DINONAKTIFKAN] Melewatkan operasi read untuk debugging\n");
    
    printf("Menulis file ke filesystem...\n");
    printf("Debug - Menulis: parent=%u, name=%s, len=%lu, size=%lu\n", 
        request.parent_inode, request.name, 
        (unsigned long)request.name_len, (unsigned long)request.buffer_size);

    printf("DEBUG: request.name_len = %u\n", request.name_len);
    if (request.name_len >= 256) {
        printf("ERROR: Name length too long!\n");
        return 1;
    }

    // Salin nama file ke struct request
    // Ini sangat penting untuk mencegah stack smashing
    char name_copy[256] = {0};
    strncpy(name_copy, name, 255);
    memcpy(request.name, name_copy, strlen(name_copy) + 1);
    // Panggil write dengan struktur (bukan pointer)
    retcode = write(request);
    
    if (retcode == 0) {
    printf("File berhasil ditulis ke filesystem\n");
    } else {
        printf("Error saat menulis file: code %d\n", retcode);
    }
    
    if (retcode == 0)
        printf("Penulisan berhasil\n");
    else
        printf("Error: Code %d\n", retcode);

    // Write image in memory into original, overwrite them
    printf("Menyimpan perubahan ke disk...\n");
    fptr = fopen(argv[3], "wb");
    if (fptr == NULL) {
        perror("Tidak dapat membuka file storage untuk penulisan");
        free(image_storage);
        free(file_buffer);
        free(read_buffer);
        exit(1);
    }
    
    fwrite(image_storage, 1, 4*1024*1024, fptr);  // Pastikan menulis semua bytes
    fclose(fptr);
    
    // Pembersihan
    free(image_storage);
    free(file_buffer);
    free(read_buffer);
    printf("Selesai\n");

    return 0;
}