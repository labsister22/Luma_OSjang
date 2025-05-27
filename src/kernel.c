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
#include "header/process/process.h"
#include "header/scheduler/scheduler.h"

void kernel_setup(void)
{
    load_gdt(&_gdt_gdtr);
    pic_remap();
    initialize_idt();
    activate_keyboard_interrupt();

    framebuffer_clear();
    framebuffer_set_cursor(0, 0);

    // 2. Inisialisasi sistem file EXT2
    initialize_filesystem_ext2();

    // 3. Inisialisasi TSS untuk multi-tasking (user mode)
    gdt_install_tss();
    set_tss_register();
  
    // Allocate first 4 MiB virtual memory
    paging_allocate_user_page_frame(&_paging_kernel_page_directory, (uint8_t *)0);

    // 5. Muat program shell ke memori
    struct EXT2DriverRequest request = {
        .buf = (uint8_t *)0,
        .name = "shell",
        .parent_inode = 2,
        .buffer_size = 0x100000,
        .name_len = 5,
    };
    // Panggil syscall read (EAX=0) untuk memuat shell.
    // Catatan: Syscall read di interrupt.c Anda saat ini tidak mengembalikan status sukses/gagal.
    // Jika ada masalah pemuatan, Anda mungkin tidak melihatnya secara langsung di sini.
    read(request);

    // Set TSS $esp pointer and jump into shell
    set_tss_kernel_current_stack();

    process_create_user_process(request);
    paging_use_page_directory(_process_list[0].context.page_directory_virtual_addr);
    scheduler_switch_to_next_process();
}
