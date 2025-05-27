#include <stdbool.h>
#include <stdint.h>
#include "header/cpu/gdt.h"
#include "header/kernel-entrypoint.h"
#include "header/text/framebuffer.h"
#include "header/cpu/portio.h"
#include "header/cpu/idt.h"
#include "header/cpu/interrupt.h"
#include "header/driver/keyboard.h"
#include "header/driver/disk.h"
#include "header/filesystem/ext2.h"
#include "header/filesystem/test_ext2.h" // Assuming this has the 'read' function definition for syscall 0
#include "header/memory/paging.h"
#include "header/stdlib/string.h" // Diperlukan untuk strlen dalam kernel_print_string


// --- Helper untuk Debugging Output ---
// Fungsi ini hanya digunakan di dalam kernel.c untuk mencetak pesan debug.
// Pastikan framebuffer_write berfungsi dengan benar.
void kernel_print_string(const char* str, int row, int col) {
    uint32_t i = 0;
    uint8_t current_col = col;
    uint8_t current_row = row;
    while (str[i] != '\0') {
        if (str[i] == '\n') {
            current_row++;
            current_col = 0;
        } else {
            // Asumsi framebuffer_write() bekerja dengan parameter (row, col, char, fg_color, bg_color)
            framebuffer_write(current_row, current_col, str[i], 0x0F, 0x00);
            current_col++;
            if (current_col >= 80) {
                current_col = 0;
                current_row++;
            }
        }
        i++;
        if (current_row >= 25) break; // Batasi tinggi layar
    }
}


void kernel_setup(void) {
    // 1. Inisialisasi dasar CPU dan display
    load_gdt(&_gdt_gdtr);
    pic_remap();
    initialize_idt();
    activate_keyboard_interrupt();

    framebuffer_clear();
    framebuffer_set_cursor(0, 0);
    kernel_print_string("LumaOS Kernel Starting...\n", 0, 0); // Debug 1

    // 2. Inisialisasi sistem file EXT2
    initialize_filesystem_ext2();
    kernel_print_string("EXT2 Filesystem Initialized.\n", 2, 0); // Debug 2

    // 3. Inisialisasi TSS untuk multi-tasking (user mode)
    gdt_install_tss();
    set_tss_register();
    kernel_print_string("TSS Initialized.\n", 3, 0); // Debug 3

    // 4. Alokasi memori virtual untuk program user (shell)
    // Shell akan dimuat ke alamat virtual 0x0 di user space
    paging_allocate_user_page_frame(&_paging_kernel_page_directory, (uint8_t*) 0);
    kernel_print_string("User Page Frame Allocated for shell at 0x0.\n", 4, 0); // Debug 4

    // 5. Muat program shell ke memori
    struct EXT2DriverRequest request = {
        .buf             = (uint8_t*) 0, // Target buffer untuk shell
        .name            = "shell",      // Nama file shell di disk
        .parent_inode    = 2,            // Asumsi root directory inode
        .buffer_size     = 0x100000,     // Ukuran buffer (1MB)
        .name_len        = 5,
    };
    // Panggil syscall read (EAX=0) untuk memuat shell.
    // Catatan: Syscall read di interrupt.c Anda saat ini tidak mengembalikan status sukses/gagal.
    // Jika ada masalah pemuatan, Anda mungkin tidak melihatnya secara langsung di sini.
    read(request);
    kernel_print_string("Attempting to load 'shell' from disk to 0x0... (Check disk image for 'shell' file!)\n", 5, 0); // Debug 5

    // 6. Set pointer stack TSS dan lompat ke program user (shell)
    set_tss_kernel_current_stack();
    kernel_print_string("Switching to user mode and executing shell at 0x0...\n", 6, 0); // Debug 6 (Pesan ini mungkin tidak terlihat jika shell langsung crash)
    kernel_execute_user_program((uint8_t*) 0);

    // 7. Loop kernel (jika program user mengembalikan kontrol ke kernel, yang seharusnya tidak terjadi)
    kernel_print_string("ERROR: Shell execution returned to kernel! This should not happen if shell runs indefinitely.\n", 7, 0); // Debug 7
    while (true); // Kernel akan berhenti di sini
}