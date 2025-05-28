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

    // Initialize EXT2 filesystem
    initialize_filesystem_ext2();

    // Initialize TSS for multi-tasking
    gdt_install_tss();
    set_tss_register();

    // CRITICAL FIX: Proper shell loading address
    uint8_t *shell_load_addr = (uint8_t *)0x400000;  // 4MB mark

    // Pre-allocate memory for shell in kernel page directory
    for (uint32_t i = 0; i < 32; i++) {  // 32 pages = 128KB
        paging_allocate_user_page_frame(&_paging_kernel_page_directory, 
                                       shell_load_addr + i * 0x1000);
    }

    // CRITICAL: Load shell with correct address
    struct EXT2DriverRequest request = {
        .buf = shell_load_addr,        // ✅ 0x400000 not 0x0
        .name = "shell",
        .parent_inode = 2,
        .buffer_size = 0x100000,       // 1MB buffer
        .name_len = 5,
    };

    // Load shell from EXT2
    int8_t shell_status = read(request);
    
    if (shell_status == 0) {
        // Shell loaded successfully
        set_tss_kernel_current_stack();
        
        // Create user process with validated address
        int32_t process_result = process_create_user_process(request);
        
        if (process_result == PROCESS_CREATE_SUCCESS) {
            // Switch to user process
            paging_use_page_directory(_process_list[0].context.page_directory_virtual_addr);
            scheduler_switch_to_next_process();
        } else {
            // Process creation failed - halt
            while (1) __asm__("hlt");
        }
    } else {
        // Shell loading failed - halt
        while (1) __asm__("hlt");
    }
}
