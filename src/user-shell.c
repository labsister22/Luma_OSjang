// src/user-shell.c

#include <stdint.h>
#include <stdbool.h>
#include "header/stdlib/string.h"
#include "header/shell/builtin_commands.h"
#include "header/driver/speaker.h"

#define COMMAND_BUFFER_SIZE 128
#define MAX_PATH_LENGTH 256

#define BEEP_FREQUENCY 440
// #define BEEP_DURATION_LOOPS 500000 // Jika ingin beep berhenti otomatis

#define SHELL_TEXT_ROWS 25
#define SHELL_TEXT_COLS 80 // Pastikan ini konsisten dengan penggunaan

// Global current working directory
char current_working_directory[MAX_PATH_LENGTH] = "/";

struct Time {
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
};

int simple_atoi(const char* str) {
    int res = 0;
    int i = 0;
    while (str[i] == ' ') {
        i++;
    }
    while (str[i] >= '0' && str[i] <= '9') {
        res = res * 10 + (str[i] - '0');
        i++;
    }
    return res;
}

void syscall(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx) {
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
    // Batasi kursor agar tidak keluar dari layar teks
    if (row >= SHELL_TEXT_ROWS) row = SHELL_TEXT_ROWS - 1;
    if (col >= SHELL_TEXT_COLS) col = SHELL_TEXT_COLS - 1;
    if (row < 0) row = 0;
    if (col < 0) col = 0;
    syscall(9, (uint32_t)col, (uint32_t)row, 0);
}

// get_char() yang sudah ada, ini blocking
char get_char() {
    char c = 0;
    do {
        syscall(4, (uint32_t)&c, 0, 0); // SYSCALL_KEYBOARD_READ
        // Hapus loop delay jika syscall(4,...) sudah blocking
        // for (volatile int i = 0; i < 100; i++); 
    } while (c == 0); // Jika syscall get_char Anda bisa return 0 jika tidak ada input
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

void process_command(char* command_buffer, int* current_row_ptr) {
    char* command_name = command_buffer;
    char* arg1 = NULL;

    size_t cmd_len = strlen(command_buffer);
    size_t i;
    for (i = 0; i < cmd_len; ++i) {
        if (command_buffer[i] == ' ') {
            command_buffer[i] = '\0'; 
            arg1 = &command_buffer[i+1];
            while (*arg1 == ' ' && *arg1 != '\0') {
                arg1++;
            }
            if (*arg1 == '\0') { 
                arg1 = NULL;
            }
            break;
        }
    }

    if (strlen(command_name) == 0) {
        return; 
    }

    // Pesan akan dicetak di baris berikutnya dari prompt
    int msg_row = *current_row_ptr + 1; 
    if (msg_row >= SHELL_TEXT_ROWS) {
        msg_row = SHELL_TEXT_ROWS - 1; 
    }


    if (strcmp(command_name, "cd") == 0) {
        if (arg1) {
            handle_cd(arg1, *current_row_ptr); // handle_cd tidak mencetak, jadi current_row_ptr
        } else {
            print_string_shell("cd: missing argument", msg_row, 0);
        }
    } else if (strcmp(command_name, "ls") == 0) {
        handle_ls(msg_row); // handle_ls akan mencetak di msg_row
    } else if (strcmp(command_name, "mkdir") == 0) {
        if (arg1) {
            handle_mkdir(arg1, msg_row);
        } else {
            print_string_shell("mkdir: missing argument", msg_row, 0);
        }
    } else if (strcmp(command_name, "cat") == 0) {
        if (arg1) {
            handle_cat(arg1, msg_row);
        } else {
            print_string_shell("cat: missing argument", msg_row, 0);
        }
    } else if (strcmp(command_name, "cp") == 0) {
        print_string_shell("cp: requires two arguments", msg_row, 0);
    } else if (strcmp(command_name, "rm") == 0) {
        if (arg1) {
            handle_rm(arg1, msg_row);
        } else {
            print_string_shell("rm: missing argument", msg_row, 0);
        }
    } else if (strcmp(command_name, "mv") == 0) {
        print_string_shell("mv: requires two arguments", msg_row, 0);
    } else if (strcmp(command_name, "find") == 0) {
        if (arg1) {
            handle_find(arg1, msg_row);
        } else {
            print_string_shell("find: missing argument", msg_row, 0);
        }
    } else if (strcmp(command_name, "beep") == 0) { 
        print_string_shell("Playing beep...", msg_row, 0);
        speaker_play(BEEP_FREQUENCY);
        // Jika ingin beep berhenti otomatis, uncomment BEEP_DURATION_LOOPS dan loop delay
        // for (volatile unsigned int d = 0; d < BEEP_DURATION_LOOPS; d++);
        // speaker_stop(); // Panggil stop jika ingin durasi tertentu
    } else if (strcmp(command_name, "stop_sound") == 0) {
        speaker_stop();
        print_string_shell("Sound stopped.", msg_row, 0);
    } 
    // Perintah 'exit' dan 'clock' ditangani di loop utama
    else {
        char unknown_msg[COMMAND_BUFFER_SIZE + 20]; 
        strcpy(unknown_msg, "Unknown command: ");
        strcat(unknown_msg, command_name); 
        print_string_shell(unknown_msg, msg_row, 0);
    }

    *current_row_ptr += 1; 
}


int main(void)
{
    char buffer[COMMAND_BUFFER_SIZE];
    int current_row = 0; 
    int buffer_pos = 0;
    int cursor_col = 0; 
    bool exit_shell = false;
    bool clock_enabled = false;

    syscall(7, 0, 0, 0); // Activate keyboard
    speaker_init();      
    clear_screen(); 
    // print_string_shell("Welcome to LumaOS CLI", 0, 0); // Pesan selamat datang
    // current_row = 1; // Pindahkan ke baris berikutnya untuk prompt pertama

    char last_time_str[9] = ""; // Untuk membandingkan string waktu
    char time_str[9];

    while (!exit_shell) {
        if (current_row >= SHELL_TEXT_ROWS) { 
             // Implementasi scroll sederhana: clear dan reset jika baris meluap
             clear_screen();
             current_row = 0;
             // print_string_shell("--- Screen Cleared ---", current_row, 0);
             // current_row++;
        }

        // Bangun prompt dengan CWD
        char prompt_display_buffer[MAX_PATH_LENGTH + 30]; // Buffer untuk string prompt
        prompt_display_buffer[0] = '\0'; // Inisialisasi untuk strcat
        strcpy(prompt_display_buffer, "luma@os:");
        strcat(prompt_display_buffer, current_working_directory);
        // Tambahkan '/' jika CWD bukan root dan tidak diakhiri '/'
        if (strcmp(current_working_directory, "/") != 0 && 
            current_working_directory[strlen(current_working_directory) - 1] != '/') {
            strcat(prompt_display_buffer, "/");
        }
        strcat(prompt_display_buffer, "$ ");

        print_string_shell(prompt_display_buffer, current_row, 0);
        cursor_col = strlen(prompt_display_buffer); 
        set_cursor(cursor_col, current_row);

        // Bersihkan buffer perintah
        buffer_pos = 0;
        buffer[0] = '\0'; 

        char c_input;
        while (1) { // Loop untuk membaca satu baris perintah
            // Update jam jika aktif (lakukan sebelum get_char agar tidak mengganggu input)
            if (clock_enabled) {
                get_time_string(time_str);
                if (strcmp(time_str, last_time_str) != 0) {
                    int temp_input_cursor_col = cursor_col; 
                    int temp_input_current_row = current_row;
                    // Cetak jam di pojok kanan atas (misalnya baris 0, kolom disesuaikan)
                    print_string_shell(time_str, 0, SHELL_TEXT_COLS - 8 -1); // 8 char untuk waktu, 1 untuk null
                    strcpy(last_time_str, time_str);
                    set_cursor(temp_input_cursor_col, temp_input_current_row); // Kembalikan kursor input
                }
            }

            c_input = get_char(); // Menggunakan get_char() yang blocking

            if (c_input == '\n' || c_input == '\r') {
                buffer[buffer_pos] = '\0'; // Akhiri string perintah
                current_row++; // Baris untuk output perintah atau prompt berikutnya

                if (strcmp(buffer, "exit") == 0) {
                    if (current_row >= SHELL_TEXT_ROWS) { current_row = SHELL_TEXT_ROWS -1; }
                    print_string_shell("Goodbye!", current_row, 0);
                    exit_shell = true;
                    break; // Keluar dari loop input karakter, lalu keluar dari loop shell utama
                }
                if (strcmp(buffer, "clock") == 0) {
                    clock_enabled = !clock_enabled; // Toggle status jam
                    if (current_row >= SHELL_TEXT_ROWS) { current_row = SHELL_TEXT_ROWS -1; }
                    if (clock_enabled) {
                        print_string_shell("Clock enabled.", current_row, 0);
                        get_time_string(time_str); // Tampilkan jam sekali saat diaktifkan
                        print_string_shell(time_str, 0, SHELL_TEXT_COLS - 8 -1);
                        strcpy(last_time_str, time_str);
                    } else {
                        print_string_shell("Clock disabled.", current_row, 0);
                        // Hapus jam dari layar (cetak spasi)
                        print_string_shell("        ", 0, SHELL_TEXT_COLS - 8 -1);
                    }
                    current_row++; 
                    break; // Keluar dari loop input karakter, kembali ke prompt baru
                }

                if (!exit_shell && strlen(buffer) > 0) { 
                    process_command(buffer, &current_row); 
                } else if (strlen(buffer) == 0) { 
                    // Jika hanya enter ditekan, current_row sudah dinaikkan, prompt baru akan muncul
                }
                break; // Keluar dari loop input karakter, kembali ke prompt baru
            } else if (c_input == '\b' || c_input == 127) { // Backspace
                // Hanya backspace jika ada karakter di buffer dan kursor tidak di awal prompt
                if (buffer_pos > 0 && cursor_col > (int)strlen(prompt_display_buffer)) {
                    buffer_pos--;
                    cursor_col--;
                    buffer[buffer_pos] = '\0'; // Hapus karakter dari buffer
                    print_char_shell(' ', current_row, cursor_col); // Hapus karakter di layar
                    set_cursor(cursor_col, current_row);
                } else if (buffer_pos > 0 && cursor_col > 0) { // Jika prompt sangat pendek
                     buffer_pos--;
                     cursor_col--;
                     buffer[buffer_pos] = '\0';
                     print_char_shell(' ', current_row, cursor_col);
                     set_cursor(cursor_col, current_row);
                }
            } else if (c_input >= 32 && c_input <= 126 && buffer_pos < COMMAND_BUFFER_SIZE - 1) { // Karakter dapat dicetak
                buffer[buffer_pos] = c_input;
                buffer_pos++;
                print_char_shell(c_input, current_row, cursor_col);
                cursor_col++;
                if (cursor_col >= SHELL_TEXT_COLS) { // Auto wrap input sederhana
                    current_row++;
                    cursor_col = 0;
                    if (current_row >= SHELL_TEXT_ROWS) {
                        // Jika wrap menyebabkan overflow, biarkan kernel yang scroll atau clear screen
                        current_row = SHELL_TEXT_ROWS - 1; // Cegah kursor keluar batas bawah
                        // Mungkin perlu scroll manual di sini jika kernel tidak melakukannya
                    }
                }
                set_cursor(cursor_col, current_row);
            }
        } // End while(1) for char input (satu baris perintah)
    } // End while(!exit_shell)

    clear_screen();
    print_string_shell("LumaOS Shell Terminated.", 0,0);

    return 0;
}