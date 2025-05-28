// src/user-shell.c

#include <stdint.h>
#include <stdbool.h>
#include "header/stdlib/string.h" // Now strictly adhering to provided functions only
#include "header/shell/builtin_commands.h"

#define COMMAND_BUFFER_SIZE 64
#define MAX_PATH_LENGTH 32

// Global current working directory
char current_working_directory[MAX_PATH_LENGTH] = "/"; // Deklarasikan sebagai extern karena didefinisikan di builtin_commands.c

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
void clear_screen() {
    syscall(8, 0, 0, 0);
}

void set_cursor(int col, int row) {
    syscall(9, col, row, 0);
}

char get_char() {
    char c = 0;
    do {
        syscall(4, (uint32_t)&c, 0, 0);
        for (volatile int i = 0; i < 100; i++);
    } while (c == 0);
    return c;
}

void get_time(struct Time* t) {
    syscall(10, (uint32_t)t, 0, 0);
}

void get_time_string(char* buffer) {
    struct Time t;
    get_time(&t);
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

// Deklarasikan print_string dan print_char yang ada di builtin_commands.c
void print_string(const char* str, int row, int col) {
    syscall(6, (uint32_t)str, row, col); // SYS_PUTS
}

void print_char(char c, int row, int col) {
    syscall(5, (uint32_t)&c, col, row); // SYS_PUTCHAR
}

// Function to process commands
void process_command_to_kernel(char* command_buffer, int* current_row_ptr) {
    if (strlen(command_buffer) == 0) {
        return; // Perintah kosong
    }

    // Buffer untuk output dari kernel
    // Ukurannya harus sangat minimal, kernel harus membatasi outputnya
    static char kernel_output_buffer[COMMAND_BUFFER_SIZE * 2]; // Misal 2 kali ukuran command
    memset(kernel_output_buffer, 0, sizeof(kernel_output_buffer));

    // Syscall baru: SYS_EXEC_CMD (Misal EAX = 14)
    // EBX: pointer ke command_buffer
    // ECX: pointer ke kernel_output_buffer
    // EDX: ukuran kernel_output_buffer
    int8_t status = 0; // Status dari kernel
    syscall(14, (uint32_t)command_buffer, (uint32_t)kernel_output_buffer, (uint32_t)sizeof(kernel_output_buffer));
    status = (int8_t)kernel_output_buffer[0]; // Asumsi byte pertama output adalah status (opsional)

    // Cetak output dari kernel
    if (strlen(kernel_output_buffer + 1) > 0) { // +1 karena byte pertama adalah status
        print_string(kernel_output_buffer + 1, *current_row_ptr, 0);
    } else {
        // Jika tidak ada output spesifik, bisa cetak OK atau Error
        if (status == 0) {
            print_string("OK", *current_row_ptr, 0);
        } else {
            print_string("ERR", *current_row_ptr, 0);
        }
    }
    *current_row_ptr += (strlen(kernel_output_buffer+1) / 80) + 1; // Pindah baris sesuai output
    if (*current_row_ptr >= 24) { // Pengecekan overflow
        clear_screen();
        *current_row_ptr = 2;
        print_string("LumaOS vM", 0, 0); // Versi mini
        print_string("---CLR---", 1, 0);
    }

    // Untuk exit dan clock, masih bisa ditangani di userspace untuk respons cepat
    if (strcmp(command_buffer, "exit") == 0) {
        // Kernel akan mematikan proses saat SYS_EXEC_CMD mengembalikan perintah "exit"
        // Atau panggil syscall 11 langsung dari sini
        syscall(11, 0, 0, 0); // SYS_EXIT (matikan QEMU jika PID 0)
    }
    if (strcmp(command_buffer, "clock") == 0) {
        // Toggle clock display di userspace
        // Ini tidak akan masuk ke SYS_EXEC_CMD
        // (Anda harus memindahkan logika ini ke main, atau handle di sini secara khusus)
    }
}



int main(void)
{
    clear_screen(); // Harus membersihkan layar
    char buffer[COMMAND_BUFFER_SIZE];
    int current_row = 0;
    int buffer_pos = 0;
    int cursor_col = 0;
    bool exit_shell = false;
    bool clock_enabled = false;
    syscall(7, 0, 0, 0); // Activate keyboard [cite: 5]
    clear_screen(); // [cite: 5]
    
    current_row = 2; // Biarkan 2 baris teratas untuk pesan sistem atau header

    char last_time[9] = "";
    while (!exit_shell) {
        char time_str[9];
        get_time_string(time_str); // [cite: 1]
        if (clock_enabled) {
            if (strcmp(time_str, last_time) != 0) {
                print_string(time_str, 24, 70); //
                strcpy(last_time, time_str); // [cite: 2]
            }
        }

        // Tampilkan prompt
        char prompt_text[MAX_PATH_LENGTH + 15];
        strcpy(prompt_text, "luma@os:"); // [cite: 2]
        strcat(prompt_text, current_working_directory); // [cite: 2]
        strcat(prompt_text, "~$ "); // [cite: 2]
        print_string(prompt_text, current_row, 0); //

        cursor_col = strlen(prompt_text); // [cite: 2]
        set_cursor(cursor_col, current_row); // [cite: 1]
        
        buffer_pos = 0;
        memset(buffer, 0, COMMAND_BUFFER_SIZE); // [cite: 2]

        while (1) {
            if (clock_enabled) {
                get_time_string(time_str); // [cite: 1]
                if (strcmp(time_str, last_time) != 0) {
                    print_string(time_str, 24, 70); //
                    strcpy(last_time, time_str); // [cite: 2]
                }
            }
            // Cek input keyboard (non-blocking polling)
            char c = 0; // Inisialisasi setiap iterasi
            syscall(4, (uint32_t)&c, 0, 0); // (SYS_KEYBOARD_READ) [cite: 5]
            
            if (c != 0) { // Jika ada karakter yang diterima
                if (c == '\n' || c == '\r') {
                    buffer[buffer_pos] = '\0';
                    current_row++; // Pindah ke baris baru setelah input

                    if (strcmp(buffer, "exit") == 0) { // [cite: 2]
                        print_string("Goodbye!", current_row, 0); //
                        exit_shell = true;
                        // Opsi: Panggil syscall SYS_EXIT jika Anda ingin mematikan QEMU dari sini
                        // syscall(11, 0, 0, 0); // Asumsi 0 adalah PID shell atau sinyal qemu_exit [cite: 5]
                        break;
                    }
                    if (strcmp(buffer, "clock") == 0) {
                        clock_enabled = !clock_enabled;
                        print_string(clock_enabled ? "CLK ON" : "CLK OFF", current_row, 0);
                        current_row++;
                        break;
                    }                    
                    process_command_to_kernel(buffer, &current_row);
                    current_row++;
                    break; // Keluar dari loop input, siap untuk prompt berikutnya
                } else if (c == '\b' || c == 127) { // Backspace
                    if (buffer_pos > 0) {
                        buffer_pos--;
                        cursor_col--;
                        buffer[buffer_pos] = '\0';
                        print_char(' ', current_row, cursor_col); //
                        set_cursor(cursor_col, current_row); // [cite: 1]
                    }
                } else if (c >= 32 && c <= 126 && buffer_pos < COMMAND_BUFFER_SIZE - 1) {
                    buffer[buffer_pos] = c;
                    buffer_pos++;
                    print_char(c, current_row, cursor_col); //
                    cursor_col++;
                    set_cursor(cursor_col, current_row); // [cite: 1]
                    if (cursor_col >= 80) { // Auto-wrap to next line if full
                        current_row++;
                        cursor_col = 0;
                        set_cursor(cursor_col, current_row); // [cite: 1]
                    }
                }
            }
            // Delay polling (mengurangi penggunaan CPU, penting untuk OS Anda)
            for (volatile int d = 0; d < 10000; d++);
        }
        
        // Pengecekan overflow layar
        if (current_row >= 24) {
            clear_screen(); // [cite: 1]
            current_row = 2;
            print_string("LumaOS Shell v1.0", 0, 0); //
            print_string("--- Screen cleared due to overflow ---", 1, 0); //
        }
    }
    return 0;
}