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
#include "header/filesystem/test_ext2.h"
#include "header/memory/paging.h"
#include "header/stdlib/string.h"
#include "header/process/process.h"
#include "header/scheduler/scheduler.h"
#include "header/driver/vga_graphics.h"

void kernel_setup(void)
{
    // 1. Initialize basic systems
    load_gdt(&_gdt_gdtr);
    pic_remap();
    initialize_idt();
    activate_keyboard_interrupt();

    // 2. Use framebuffer for initial display (more stable)
    framebuffer_clear();
    framebuffer_set_cursor(0, 0);
    framebuffer_write(0, 0, 'L', 0x0F, 0);
    framebuffer_write(0, 1, 'u', 0x0F, 0);
    framebuffer_write(0, 2, 'm', 0x0F, 0);
    framebuffer_write(0, 3, 'a', 0x0F, 0);
    framebuffer_write(0, 4, 'O', 0x0F, 0);
    framebuffer_write(0, 5, 'S', 0x0F, 0);

    // 3. Initialize filesystem
    initialize_filesystem_ext2();

    // 4. Initialize TSS
    gdt_install_tss();
    set_tss_register();

    // 5. Initialize VGA AFTER everything else is stable
    vga_graphics_init();
  
    // 6. Allocate first 4 MiB virtual memory
    paging_allocate_user_page_frame(&_paging_kernel_page_directory, (uint8_t *)0);

    // 7. Load shell program
    struct EXT2DriverRequest request = {
        .buf = (uint8_t *)0,
        .name = "shell",
        .parent_inode = 2,
        .buffer_size = 0x100000,
        .name_len = 5,
    };
    
    read(request);

    // 8. Set TSS and start user process
    set_tss_kernel_current_stack();
    process_create_user_process(request);
    paging_use_page_directory(_process_list[0].context.page_directory_virtual_addr);
    scheduler_switch_to_next_process();
}