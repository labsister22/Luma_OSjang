// header/file_operations.h

#ifndef FILE_OPERATIONS_H
#define FILE_OPERATIONS_H

#include <stdint.h> // For uint32_t etc.
#include <stdbool.h> // For bool

// Fungsi-fungsi yang akan memanggil syscall EXT2
int change_directory(const char* path); // Mengubah CWD di kernel
int get_current_working_directory_kernel(char* buffer, uint32_t buffer_size); // Mendapatkan CWD dari kernel

int make_directory(const char *path); // Memanggil SYS_WRITE
int remove_directory(const char *path); // Memanggil SYS_DELETE

int copy_file(const char *source_path, const char *dest_path); // Memanggil SYS_READ & SYS_WRITE
int move_file(const char *source_path, const char *dest_path); // Memanggil copy_file & remove_file
int remove_file(const char *path); // Memanggil SYS_DELETE

void list_directory_contents(const char *path); // Memanggil SYS_LS_DIR
void display_file_contents(const char *path); // Memanggil SYS_READ

#endif // FILE_OPERATIONS_H