// src/user-shell.c

#include <stdint.h>
#include <stdbool.h>
#include "header/stdlib/string.h" // Now strictly adhering to provided functions only
#include "header/shell/builtin_commands.h"

#define COMMAND_BUFFER_SIZE 128
#define MAX_PATH_LENGTH 256
// Frekuensi untuk nada A4
// #define BEEP_DURATION_LOOPS 500000 

// Global current working directory
char current_working_directory[MAX_PATH_LENGTH] = "/";
extern int current_output_row;
struct Time {
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
};

int simple_atoi(const char* str) {
    int res = 0;
    int i = 0;
    // Lewati spasi di awal jika ada
    while (str[i] == ' ') {
        i++;
    }
    // Proses digit
    while (str[i] >= '0' && str[i] <= '9') {
        res = res * 10 + (str[i] - '0');
        i++;
    }
    return res;
}

void user_syscall(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx)
{
    __asm__ volatile("mov %0, %%ebx" : : "r"(ebx));
    __asm__ volatile("mov %0, %%ecx" : : "r"(ecx));
    __asm__ volatile("mov %0, %%edx" : : "r"(edx));
    __asm__ volatile("mov %0, %%eax" : : "r"(eax));
    __asm__ volatile("int $0x30");
}
void clear_screen() {
    user_syscall(8, 0, 0, 0);
}

void set_cursor(int col, int row) {
    user_syscall(9, col, row, 0);
}

char get_char() {
    char c = 0;
    do {
        user_syscall(4, (uint32_t)&c, 0, 0);
        for (volatile int i = 0; i < 100; i++);
    } while (c == 0);
    return c;
}

void get_time(struct Time* t) {
    user_syscall(10, (uint32_t)t, 0, 0);
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
void parse_two_arguments(const char* input, char* arg1, char* arg2) {
    int i = 0, j = 0;
    
    // Clear buffers
    arg1[0] = '\0';
    arg2[0] = '\0';
    
    // Skip leading spaces
    while (input[i] == ' ' && input[i] != '\0') i++;
    
    // Parse first argument
    while (input[i] != ' ' && input[i] != '\0' && j < 63) {
        arg1[j++] = input[i++];
    }
    arg1[j] = '\0';
    
    // Skip spaces between arguments
    while (input[i] == ' ' && input[i] != '\0') i++;
    
    // Parse second argument
    j = 0;
    while (input[i] != '\0' && j < 63) {
        arg2[j++] = input[i++];
    }
    arg2[j] = '\0';
}
// Function to process commands
void process_command(char* command_buffer) {
    char* command_name = command_buffer;
    char* arg1 = NULL;
    char* arg2 = NULL;

    // Manual attempt to find spaces to separate command and arguments
    size_t cmd_len = strlen(command_buffer);
    size_t i;
    
    // Find first space (separates command from arg1)
    for (i = 0; i < cmd_len; ++i) {
        if (command_buffer[i] == ' ') {
            command_buffer[i] = '\0'; // Null-terminate command name
            arg1 = &command_buffer[i+1];
            // Skip leading spaces for the argument
            while (*arg1 == ' ' && *arg1 != '\0') {
                arg1++;
            }
            if (*arg1 == '\0') { // If argument is just spaces or empty
                arg1 = NULL;
                break;
            }
            
            // Find second space (separates arg1 from arg2)
            char* temp_ptr = arg1;
            while (*temp_ptr != '\0' && *temp_ptr != ' ') {
                temp_ptr++;
            }
            if (*temp_ptr == ' ') {
                *temp_ptr = '\0'; // Null-terminate arg1
                arg2 = temp_ptr + 1;
                // Skip leading spaces for arg2
                while (*arg2 == ' ' && *arg2 != '\0') {
                    arg2++;
                }
                if (*arg2 == '\0') { // If arg2 is just spaces or empty
                    arg2 = NULL;
                }
            }
            break;
        }
    }


    if (strlen(command_name) == 0) {
        return; // Empty command
    }

    if (strcmp(command_name, "cd") == 0) {
        if (arg1) {
            handle_cd(arg1);
        } else {
            print_line("cd: missing argument");
        }
    } else if (strcmp(command_name, "ls") == 0) {
        handle_ls();
    } else if (strcmp(command_name, "clear") == 0) {
        handle_clear();
    } else if (strcmp(command_name, "help") == 0) {
        handle_help();
    }else if (strcmp(command_name, "mkdir") == 0) {
        if (arg1) {
            handle_mkdir(arg1);
        } else {
            print_line("mkdir: missing argument");
        }
    } else if (strcmp(command_name, "cat") == 0) {
        if (arg1) {
            handle_cat(arg1);
        } else {
            print_line("cat: missing argument");
        }
    } else if (strcmp(command_name, "cp") == 0) {
        if (arg1 && arg2) {
            handle_cp(arg1, arg2);
        } else {
            print_line("cp: missing arguments (usage: cp source destination)");
        }
    } else if (strcmp(command_name, "rm") == 0) {
        if (arg1) {
            handle_rm(arg1);
        } else {
            print_line("rm: missing argument");
        }
    } else if (strcmp(command_name, "mv") == 0) {
       if (arg1 && arg2) {
            handle_mv(arg1, arg2);
        } else {
            print_line("mv: missing arguments (usage: mv source destination)");
        }
    } else if (strcmp(command_name, "find") == 0) {
        if (arg1) {
            handle_find(arg1);
        } else {
            print_line("find: missing argument");
        }
    } else if (strcmp(command_name, "beep") == 0) { // Tambahkan perintah beep
        int b = 0;
        print_string("Playing beep...", &current_output_row, &b);
        speaker_play(BEEP_FREQUENCY);
        // Tambahkan delay sederhana
        // for (volatile int d = 0; d < BEEP_DURATION_LOOPS; d++);
        // speaker_stop();
    } else if (strcmp(command_name, "stop") == 0) {
        speaker_stop();
        int b = 0;
        print_string("Sound stopped.", &current_output_row, &b);
    } else if (strcmp(command_name, "exit") == 0) { // Exit needs to be handled here directly now
        int b = 0;
        print_string("Goodbye!", &current_output_row, &b);
        // This will only be executed if 'exit' is the only thing typed.
        // It's technically unreachable now due to the main loop's check.
    } else if (strcmp(command_name, "clock") == 0) { // Clock also handled directly
        int b = 0;
        print_string("Clock running...", &current_output_row, &b);
        // It's technically unreachable now due to the main loop's check.
    } else if (strcmp(command_name, "exec") == 0) {
        // TAMBAHAN: Handle exec command
        if (arg1) {
            handle_exec(arg1);
        } else {
            print_line("exec: missing argument");
        }
    } else if (strcmp(command_name, "ps") == 0) {
        // TAMBAHAN: Handle ps command (no arguments needed)
        handle_ps();
    } else if (strcmp(command_name, "kill") == 0) {
        // TAMBAHAN: Handle kill command
        if (arg1) {
            handle_kill(arg1);
        } else {
            print_line("kill: missing argument (usage: kill <pid>)");
        }
    } else {
        int b = 0;
        print_string("Unknown command: ", &current_output_row, &b);
        int a = (int)strlen("Unknown command: ");
        print_string(command_name, &current_output_row, &a);
    }

    current_output_row += 1;
}


int main(void)
{
    // syscall(6, (uint32_t)"LumaOS CLI started\n", 0, 0); // Print initial message
    // return 0;
    char buffer[COMMAND_BUFFER_SIZE];
    // HAPUS `int current_row = 0;` (sekarang menggunakan global current_output_row)
    int buffer_pos = 0;
    int cursor_col_for_input = 0; // Variabel untuk melacak kolom kursor input
    bool exit_shell = false;
    bool clock_enabled = false;
    user_syscall(7, 0, 0, 0); // Activate keyboard
    speaker_init(); // Initialize speaker
    clear_screen();
    // print_string("Welcome-to-LumaOS-CLI\n", 0, 0);

    char last_time[9] = "";
    while (!exit_shell) {
        // Polling jam dan input secara multitasking
        int input_ready = 0;
        char c = 0;
        char time_str[9];
        get_time_string(time_str);
        // if (clock_enabled) {
        //     if (strcmp(time_str, last_time) != 0) {
        //         int a = 24;
        //         int b = 70;
        //         print_string(time_str, &a, &b);
        //         for (int i = 0; i < 9; i++) last_time[i] = time_str[i];
        //     }
        // }

        int temp_col = 0; // Reset kolom sementara untuk prompt
        // Cetak "luma@os:"
        print_string("luma@os:", &current_output_row, &temp_col);
        // Cetak current_working_directory
        print_string(current_working_directory, &current_output_row, &temp_col);
        // Cetak "$ "
        print_string("$ ", &current_output_row, &temp_col);

        // Update cursor_col_for_input berdasarkan panjang prompt yang sudah dicetak
        cursor_col_for_input = temp_col; // temp_col sudah berisi posisi kolom akhir setelah mencetak prompt

        set_cursor(cursor_col_for_input, current_output_row); // Set kursor untuk input

        buffer_pos = 0;
        for (int i = 0; i < COMMAND_BUFFER_SIZE; i++) buffer[i] = '\0';
        while (!input_ready) {
            // Update jam setiap polling
            if (clock_enabled) {
                get_time_string(time_str);
                if (strcmp(time_str, last_time) != 0) {
                    int a = 24;
                    int b = 70;
                    print_string(time_str, &a, &b);
                    for (int i = 0; i < 9; i++) last_time[i] = time_str[i];
                }
            }
            // Cek input keyboard (non-blocking polling)
            user_syscall(4, (uint32_t)&c, 0, 0);
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
                    int a = 24;
                    int b = 70;
                    print_string(time_str, &a, &b);
                    for (int i = 0; i < 9; i++) last_time[i] = time_str[i];
                }
            }
            if (c == '\n' || c == '\r') {
                buffer[buffer_pos] = '\0';
                    current_output_row++; // Majukan baris untuk output perintah

                    if (strcmp(buffer, "exit") == 0) {
                        print_line("Goodbye!");
                        exit_shell = true;
                    } else if (strcmp(buffer, "clock") == 0) {
                        clock_enabled = true;
                        print_line("Clock running...");
                    } else {
                        process_command(buffer); // Panggil tanpa argumen row
                    }
                    // Setelah perintah selesai, baris output terakhir sudah di current_output_row
                    cursor_col_for_input = 0; // Reset kolom kursor untuk prompt baru
                    // current_output_row sudah diupdate oleh process_command/handle_ls.
                    // Tidak perlu increment lagi di sini.
                    break;
            } else if (c == '\b' || c == 127) {
                int prompt_full_len = strlen("luma@os:") + strlen(current_working_directory) + strlen("$ ");
                
                if (buffer_pos > 0 && cursor_col_for_input > prompt_full_len) {
                    buffer_pos--;
                    cursor_col_for_input--;
                    buffer[buffer_pos] = '\0';
                    set_cursor(cursor_col_for_input, current_output_row);
                    user_syscall(5, (uint32_t)' ', cursor_col_for_input, current_output_row);
                    set_cursor(cursor_col_for_input, current_output_row);
                }
            } else if (c >= 32 && c <= 126 && buffer_pos < COMMAND_BUFFER_SIZE - 1) { // Karakter biasa
                    buffer[buffer_pos] = c;
                    buffer_pos++;
                    print_char(c, &current_output_row, &cursor_col_for_input); // Cetak karakter dan update kolom
                    set_cursor(cursor_col_for_input, current_output_row);
                }
            // Ambil input berikutnya (polling)
            c = 0;
            user_syscall(4, (uint32_t)&c, 0, 0);
            for (volatile int d = 0; d < 10000; d++);
        }
        if (current_output_row >= 24) {
            clear_screen(); // Syscall 8
            current_output_row = 0; // Mulai prompt setelah 2 baris header clear screen
            // print_string("LumaOS Shell v1.0", &current_output_row, &cursor_col_for_input); // Cetak dan update baris/kolom
            // print_string("--- Screen cleared due to overflow ---", &current_output_row, &cursor_col_for_input); // Cetak dan update baris/kolom
            // current_output_row sudah di-increment oleh print_string
        }
    }
    return 0;
}