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
    size_t filesize = 0;
    if (fptr_target == NULL) {
        printf("Peringatan: Tidak dapat membuka file %s - akan membuat file kosong\n", argv[1]);
        filesize = 0;
    } else {
        fseek(fptr_target, 0, SEEK_END);
        filesize = ftell(fptr_target);
        fseek(fptr_target, 0, SEEK_SET);
        
        if (filesize > 0 && filesize <= 4*1024*1024) {
            size_t read_size = fread(file_buffer, 1, filesize, fptr_target);
            if (read_size != filesize) {
                printf("Peringatan: Hanya membaca %zu dari %zu bytes\n", read_size, filesize);
                filesize = read_size;
            }
        } else if (filesize > 4*1024*1024) {
            printf("Error: File terlalu besar! Maksimum 4MB\n");
            fclose(fptr_target);
            free(image_storage);
            free(file_buffer);
            free(read_buffer);
            exit(1);
        }
        fclose(fptr_target);
    }

    // Extract just the filename from the path
    char *name = strrchr(argv[1], '/');
    if (name) {
        name++; // Skip the '/'
    } else {
        name = argv[1]; // No path, use as-is
    }

    printf("Filename : %s\n", name);
    printf("Filesize : %zu bytes\n", filesize);

    // EXT2 operations
    printf("Menginisialisasi filesystem EXT2...\n");
    initialize_filesystem_ext2();
    
    // Initialize request structure properly with zero
    struct EXT2DriverRequest request;
    memset(&request, 0, sizeof(request));
    
    // Validate filename length first
    size_t name_len = strlen(name);
    if (name_len >= 256) {
        printf("ERROR: Filename terlalu panjang! Maksimum 255 karakter, diberikan %zu\n", name_len);
        free(image_storage);
        free(file_buffer);
        free(read_buffer);
        exit(1);
    }
    
    // Set up request structure
    request.buf = file_buffer;
    request.buffer_size = (uint32_t)filesize;
    
    // Copy filename safely
    strncpy(request.name, name, 255);
    request.name[255] = '\0';  // Ensure null termination
    request.name_len = (uint8_t)name_len;  // Cast to uint8_t since it's validated to be < 256
    request.is_directory = 0;  // FALSE
    
    // Parse parent inode
    if (sscanf(argv[2], "%u", &request.parent_inode) != 1) {
        printf("Error: Parent inode harus berupa angka\n");
        free(image_storage);
        free(file_buffer);
        free(read_buffer);
        exit(1);
    }
    
    printf("Filename       : %s\n", request.name);
    printf("Filename length: %u\n", request.name_len);
    printf("Parent inode: %u\n", request.parent_inode);
    printf("Buffer size: %u\n", request.buffer_size);
    
    printf("Menulis file ke filesystem...\n");
    printf("Debug - Menulis: parent=%u, name=%s, len=%u, size=%u\n", 
        request.parent_inode, request.name, 
        request.name_len, request.buffer_size);

    // Call write function
    int8_t retcode = write(request);
    
    if (retcode == 0) {
        printf("File berhasil ditulis ke filesystem\n");
    } else {
        printf("Error saat menulis file: code %d\n", retcode);
    }

    // Write image back to disk
    printf("Menyimpan perubahan ke disk...\n");
    fptr = fopen(argv[3], "wb");
    if (fptr == NULL) {
        perror("Tidak dapat membuka file storage untuk penulisan");
        free(image_storage);
        free(file_buffer);
        free(read_buffer);
        exit(1);
    }
    
    size_t written = fwrite(image_storage, 1, 4*1024*1024, fptr);
    fclose(fptr);
    
    if (written != 4*1024*1024) {
        printf("Peringatan: Hanya menulis %zu dari %d bytes\n", written, 4*1024*1024);
    } else {
        printf("Berhasil menyimpan 4MB ke disk\n");
    }
    
    // Cleanup
    free(image_storage);
    free(file_buffer);
    free(read_buffer);
    printf("Selesai\n");

    return 0;
}