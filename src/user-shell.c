#include <stdint.h>
#include <stdbool.h>
#include "header/stdlib/string.h"
#include "header/filesystem/ext2.h"
#include <stdio.h>

#define COMMAND_BUFFER_SIZE 128

struct Time {
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
};

void syscall(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx)
{
    __asm__ volatile("mov %0, %%ebx" : : "r"(ebx));
    __asm__ volatile("mov %0, %%ecx" : : "r"(ecx));
    __asm__ volatile("mov %0, %%edx" : : "r"(edx));
    __asm__ volatile("mov %0, %%eax" : : "r"(eax));
    __asm__ volatile("int $0x30");
}

void print_string(const char* str, int row, int col) {
    syscall(6, (uint32_t)str, row, col);
}
void print_char(char c, int row, int col) {
    syscall(5, (uint32_t)&c, col, row);  // Note: col, row order
}
// void print_prompt(int row) {
//     print_string("$ ", row, 0);
// }

void clear_screen() {
    syscall(8, 0, 0, 0);
}
void set_cursor(int col, int row) {
    syscall(9, col, row, 0);  // col, row order
}

char get_char() {
    char c = 0;
    do {
        syscall(4, (uint32_t)&c, 0, 0);
        // Add small delay to prevent busy waiting
        for (volatile int i = 0; i < 100; i++);
    } while (c == 0);
    return c;
}
void get_time(struct Time* t) {
    syscall(10, (uint32_t)t, 0, 0);  // Ganti 10 dengan syscall ID yang benar untuk waktu
}

void get_time_string(char* buffer) {
    struct Time t;
    get_time(&t);
    // Format: HH:MM:SS (e.g., 09:25:01)
    buffer[0] = '0' + (t.hour / 10);
    buffer[1] = '0' + (t.hour % 10);
    buffer[2] = ':';
    buffer[3] = '0' + (t.minute / 10);
    buffer[4] = '0' + (t.minute % 10);
    buffer[5] = ':';
    buffer[6] = '0' + (t.second / 10);
    buffer[7] = '0' + (t.second % 10);
    buffer[8] = '\0';
}

// Simple integer to string conversion (base 10)
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
    if (*found) return;

    struct EXT2Inode dir_inode;
    syscall(21, (uint32_t)&dir_inode, curr_inode, 0);

    if (dir_inode.i_size == 0) return;

    for (int blk = 0; blk < 12 && dir_inode.i_block[blk] != 0; blk++) {
        uint8_t buf[512];
        syscall(23, (uint32_t)buf, dir_inode.i_block[blk], 1);

        uint32_t offset = 0;
        while (offset < 512 && !(*found)) {
            struct EXT2DirectoryEntry* entry = (struct EXT2DirectoryEntry*)(buf + offset);

            // Validasi entry
            if (entry->rec_len == 0 || entry->rec_len < 8) break;
            if (offset + entry->rec_len > 512) break;
            if (entry->inode == 0 || entry->name_len == 0 || entry->name_len > 255) {
                offset += entry->rec_len;
                continue;
            }

            // Ambil nama entry
            char entry_name[256];
            char* name_ptr = (char*)(entry + 1);
            int name_len = entry->name_len;
            if (name_len > 255) name_len = 255;
            
            for (int i = 0; i < name_len; i++) {
                entry_name[i] = name_ptr[i];
            }
            entry_name[name_len] = '\0';

            // Skip . dan ..
            if ((name_len == 1 && entry_name[0] == '.') ||
                (name_len == 2 && entry_name[0] == '.' && entry_name[1] == '.')) {
                offset += entry->rec_len;
                continue;
            }

            // Bangun path lengkap
            char new_path[512];
            int path_len = 0;
            
            while (path[path_len] != '\0' && path_len < 500) {
                new_path[path_len] = path[path_len];
                path_len++;
            }
            
            if (!(path_len == 1 && path[0] == '/')) {
                new_path[path_len++] = '/';
            }
            
            for (int i = 0; i < name_len && path_len < 500; i++) {
                new_path[path_len++] = entry_name[i];
            }
            new_path[path_len] = '\0';

            // Bandingkan nama
            int search_len = 0;
            while (search_name[search_len] != '\0') search_len++;
            
            int match = (search_len == name_len);
            for (int i = 0; match && i < search_len; i++) {
                if (search_name[i] != entry_name[i]) match = 0;
            }

            if (match) {
                (*current_row)++;
                print_string("FOUND: ", *current_row, 0);
                print_string(new_path, *current_row, 7);
                *found = 1;
                return;
            }

            // Rekursi jika direktori
            struct EXT2Inode child_inode;
            syscall(21, (uint32_t)&child_inode, entry->inode, 0);
            if ((child_inode.i_mode & 0x4000) == 0x4000) {
                find_recursive(entry->inode, search_name, new_path, found, current_row);
            }

            offset += entry->rec_len;
        }
    }
}


void find_command(const char* filename, int* current_row) {
    if (filename == NULL || filename[0] == '\0') {
        (*current_row)++;
        print_string("find: usage: find <filename>", *current_row, 0);
        return;
    }
    (*current_row)++;
    print_string("Searching for:", *current_row, 0);
    (*current_row)++;
    print_string(filename, *current_row, 0);

    char root_path[2] = {'/', '\0'};
    int found = 0;
    find_recursive(2, filename, root_path, &found, current_row);

    if (!found) {
        (*current_row)++;
        print_string("Not found", *current_row, 0);

    }
    (*current_row)++;
    // print_string("Search complete", *current_row, 0);
}

void ls_command(int* current_row) {
    (*current_row)++;
    print_string("Files in root:", *current_row, 0);
    
    struct EXT2Inode root_inode;
    syscall(21, (uint32_t)&root_inode, 2, 0);
    
    if (root_inode.i_size == 0) {
        (*current_row)++;
        print_string("Root directory is empty", *current_row, 0);
        return;
    }
    
    for (int blk = 0; blk < 12 && root_inode.i_block[blk] != 0; blk++) {
        uint8_t buf[512];
        syscall(23, (uint32_t)buf, root_inode.i_block[blk], 1);
        
        (*current_row)++;
        print_string("Block ", *current_row, 0);
        char blk_str[10];
        itoa(blk, blk_str);
        print_string(blk_str, *current_row, 6);
        print_string(":", *current_row, 7);
        
        uint32_t offset = 0;
        while (offset < 512) {
            struct EXT2DirectoryEntry* entry = (struct EXT2DirectoryEntry*)(buf + offset);
            
            if (entry->rec_len == 0 || entry->rec_len < 8) break;
            if (offset + entry->rec_len > 512) break;
            if (entry->inode == 0 || entry->name_len == 0) {
                offset += entry->rec_len;
                continue;
            }
            
            char entry_name[256];
            char* name_ptr = (char*)(entry + 1);
            for (int i = 0; i < entry->name_len && i < 255; i++) {
                entry_name[i] = name_ptr[i];
            }
            entry_name[entry->name_len] = '\0';
            
            (*current_row)++;
            print_string("  ", *current_row, 0);
            print_string(entry_name, *current_row, 2);
            
            offset += entry->rec_len;
        }
    }
}

int main(void)
{
    // syscall(6, (uint32_t)"LumaOS CLI started\n", 0, 0); // Print initial message
    // return 0;
    char buffer[COMMAND_BUFFER_SIZE];
    int current_row = 0;
    int buffer_pos = 0;
    int cursor_col = 0;
    bool exit_shell = false;
    bool clock_enabled = false;
    syscall(7, 0, 0, 0); // Activate keyboard
    clear_screen();
    // print_string("Welcome-to-LumaOS-CLI\n", 0, 0);

    char last_time[9] = "";
    while (!exit_shell) {
        // Polling jam dan input secara multitasking
        int input_ready = 0;
        char c = 0;
        char time_str[9];
        get_time_string(time_str);
        if (clock_enabled) {
            if (strcmp(time_str, last_time) != 0) {
                print_string(time_str, 24, 70);
                for (int i = 0; i < 9; i++) last_time[i] = time_str[i];
            }
        }
        print_string("luma@os:~$ ", current_row, 0);
        cursor_col = 11;
        set_cursor(cursor_col, current_row);
        buffer_pos = 0;
        for (int i = 0; i < COMMAND_BUFFER_SIZE; i++) buffer[i] = '\0';
        while (!input_ready) {
            // Update jam setiap polling
            if (clock_enabled) {
                get_time_string(time_str);
                if (strcmp(time_str, last_time) != 0) {
                    print_string(time_str, 24, 70);
                    for (int i = 0; i < 9; i++) last_time[i] = time_str[i];
                }
            }
            // Cek input keyboard (non-blocking polling)
            syscall(4, (uint32_t)&c, 0, 0);
            if (c != 0) {
                input_ready = 1;
                break;
            }
            // Delay polling
            for (volatile int d = 0; d < 100000; d++);
        }
        // Proses input seperti biasa
        while (1) {
            if (clock_enabled) {
                get_time_string(time_str);
                if (strcmp(time_str, last_time) != 0) {
                    print_string(time_str, 24, 70);
                    for (int i = 0; i < 9; i++) last_time[i] = time_str[i];
                }
            }
            if (c == '\n' || c == '\r') {
                buffer[buffer_pos] = '\0';
                if (buffer[0] == 'e' && buffer[1] == 'x' && buffer[2] == 'i' && buffer[3] == 't' && buffer[4] == '\0') {
                    print_string("Goodbye!", current_row, 0);
                    exit_shell = true;
                }
                if (buffer[0] == 'c' && buffer[1] == 'l' && buffer[2] == 'o' && buffer[3] == 'c' && buffer[4] == 'k' && buffer[5] == '\0') {
                    clock_enabled = true;
                    current_row++;
                    print_string("Clock running...", current_row, 0);
                    current_row++;
                    break;
                }

                // Di dalam main loop, setelah find command:
                if (buffer[0] == 'l' && buffer[1] == 's' && buffer[2] == '\0') {
                    ls_command(&current_row);
                    break;
                }
                // find command: find <name>
                if (buffer[0] == 'f' && buffer[1] == 'i' && buffer[2] == 'n' && buffer[3] == 'd' && buffer[4] == ' ') {
                    char *name = buffer + 5;
                    find_command(name, &current_row);
                    break;
                }
                if (!exit_shell) {
                    // process_command(buffer, &current_row);
                }
                current_row++;
                break;
            } else if (c == '\b' || c == 127) {
                if (buffer_pos > 0 && cursor_col > 11) {
                    buffer_pos--;
                    cursor_col--;
                    buffer[buffer_pos] = '\0';
                    print_char(' ', current_row, cursor_col);
                    set_cursor(cursor_col, current_row);
                }
            } else if (c >= 32 && c <= 126 && buffer_pos < COMMAND_BUFFER_SIZE - 1) {
                buffer[buffer_pos] = c;
                buffer_pos++;
                print_char(c, current_row, cursor_col);
                cursor_col++;
                set_cursor(cursor_col, current_row);
                if (cursor_col >= 80) {
                    current_row++;
                    cursor_col = 0;
                    set_cursor(cursor_col, current_row);
                }
            }
            // Ambil input berikutnya (polling)
            c = 0;
            syscall(4, (uint32_t)&c, 0, 0);
            for (volatile int d = 0; d < 10000; d++);
        }
        if (current_row >= 24) {
            clear_screen();
            current_row = 2;
            print_string("LumaOS Shell v1.0", 0, 0);
            print_string("--- Screen cleared due to overflow ---", 1, 0);
        }
    }
    return 0;
}