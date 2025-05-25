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

// Minimal snprintf replacement for: "Found '%s' at inode %u" and "'%s' not found"
void print_find_result(const char* name, uint32_t inode, int found, int row) {
    char msg[64];
    int i = 0;
    if (found) {
        const char* prefix = "Found '";
        for (int j = 0; prefix[j]; j++) msg[i++] = prefix[j];
        for (int j = 0; name[j] && i < 40; j++) msg[i++] = name[j];
        msg[i++] = '\'';
        const char* at = " at inode ";
        for (int j = 0; at[j]; j++) msg[i++] = at[j];
        char num[16];
        itoa(inode, num);
        for (int j = 0; num[j] && i < 60; j++) msg[i++] = num[j];
        msg[i] = '\0';
    } else {
        msg[i++] = '\'';
        for (int j = 0; name[j] && i < 40; j++) msg[i++] = name[j];
        msg[i++] = '\'';
        const char* notf = " not found";
        for (int j = 0; notf[j]; j++) msg[i++] = notf[j];
        msg[i] = '\0';
    }
    print_string(msg, row, 0);
}

// Find file or folder by name (exact match) in the entire EXT2 filesystem (recursive search)
// Returns inode number if found, 0 if not found
// name: null-terminated string (max 255 chars)
uint32_t find_file_folder_recursive(uint32_t dir_inode_idx, const char *name) {
    struct EXT2Inode dir_inode;
    syscall(21, (uint32_t)&dir_inode, dir_inode_idx, 0); // 21: get inode by index
    if ((dir_inode.i_mode & 0x4000) != 0x4000) return 0; // Not a directory (EXT2_S_IFDIR)

    uint8_t buf[512];
    syscall(23, (uint32_t)buf, dir_inode.i_block[0], 1); // 23: read_blocks
    uint32_t offset = 0;
    // Cari di seluruh direktori, termasuk subdirektori
    while (offset < 512) {
        struct EXT2DirectoryEntry *entry = (struct EXT2DirectoryEntry *)(buf + offset);
        if (entry->inode == 0 || entry->rec_len == 0) break;
        char *entry_name = (char *)(entry + 1);
        int entry_len = entry->name_len;
        if (entry_len > 255) entry_len = 255;
        for (int k = 0; k < entry_len; k++) ;
        // Skip "." and ".."
        if (!((entry->name_len == 1 && entry_name[0] == '.') || (entry->name_len == 2 && entry_name[0] == '.' && entry_name[1] == '.'))) {
            // Perbaikan: bandingkan persis sepanjang input, abaikan trailing null/space di EXT2
            int input_len = strlen(name);
            int match = 1;
            if (entry_len == input_len) {
                for (int m = 0; m < input_len; m++) {
                    if (entry_name[m] != name[m]) {
                        match = 0;
                        break;
                    }
                }
                // Pastikan tidak ada karakter non-null setelah input_len di entry_name
                if (match) {
                    for (int m = input_len; m < entry_len; m++) {
                        if (entry_name[m] != '\0' && entry_name[m] != ' ') {
                            match = 0;
                            break;
                        }
                    }
                }
                if (match) return entry->inode;
            }
            // Jika entry adalah direktori, rekursif
            struct EXT2Inode child_inode;
            syscall(21, (uint32_t)&child_inode, entry->inode, 0);
            if ((child_inode.i_mode & 0x4000) == 0x4000) {
                uint32_t found = find_file_folder_recursive(entry->inode, name);
                if (found != 0) return found;
            }
        }
        offset += entry->rec_len;
    }
    return 0;
}

// Wrapper: search from root (inode 2)
uint32_t find_file_folder(const char *name) {
    return find_file_folder_recursive(2, name);
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
                // find command: find <name>
                if (buffer[0] == 'f' && buffer[1] == 'i' && buffer[2] == 'n' && buffer[3] == 'd' && buffer[4] == ' ') {
                    char *name = buffer + 5;
                    if (name[0] != '\0') {
                        uint32_t inode = find_file_folder(name);
                        if (inode != 0) {
                            print_find_result(name, inode, 1, ++current_row);
                        } else {
                            print_find_result(name, 0, 0, ++current_row);
                        }
                        current_row++;
                        break;
                    }
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