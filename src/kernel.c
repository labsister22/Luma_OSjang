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

// Helper function to print debug messages
void debug_print(int x, int y, char c, uint8_t fg, uint8_t bg)
{
    framebuffer_write(x, y, c, fg, bg);
}

void debug_print_string(int x, int y, const char *str, uint8_t fg, uint8_t bg)
{
    int i = 0;
    while (str[i] != '\0')
    {
        framebuffer_write(x + i, y, str[i], fg, bg);
        i++;
    }
}

void kernel_setup(void)
{
    load_gdt(&_gdt_gdtr);
    pic_remap();
    initialize_idt();
    activate_keyboard_interrupt();
    framebuffer_clear();
    framebuffer_set_cursor(0, 0);

    debug_print_string(0, 0, "Kernel Init...", 0xF, 0x0);

    initialize_filesystem_ext2();
    debug_print_string(0, 1, "FS Init OK", 0xF, 0x0);

    gdt_install_tss();
    set_tss_register();
    debug_print_string(0, 2, "TSS Init OK", 0xF, 0x0);

    // Use address that maps to page index 1 (not 0 which is blocked)
    uint8_t *shell_addr = (uint8_t *)0x400000; // 4MB mark (page index 1)

    // Allocate virtual memory for shell
    debug_print_string(0, 3, "Allocating memory...", 0xF, 0x0);
    bool alloc_success = paging_allocate_user_page_frame(&_paging_kernel_page_directory, shell_addr);
    if (!alloc_success)
    {
        debug_print_string(0, 4, "Memory alloc FAILED!", 0xC, 0x0);
        while (true)
            ;
    }
    debug_print_string(0, 4, "Memory alloc OK", 0xF, 0x0);

    // Also allocate stack memory (at 0x800000 - where assembly code puts stack)
    uint8_t *stack_addr = (uint8_t *)0x800000;
    bool stack_alloc = paging_allocate_user_page_frame(&_paging_kernel_page_directory, stack_addr);
    if (!stack_alloc)
    {
        debug_print_string(0, 8, "Stack alloc FAILED!", 0xC, 0x0);
        while (true)
            ;
    }

    // Write shell into memory
    struct EXT2DriverRequest request = {
        .buf = shell_addr,
        .name = "shell",
        .parent_inode = 2,
        .buffer_size = 0x100000, // 1MB buffer
        .name_len = 5,
    };

    debug_print_string(0, 5, "Loading shell...", 0xF, 0x0);
    int8_t status = read(&request);

    // Enhanced error reporting
    if (status != 0)
    {
        debug_print_string(0, 6, "Error: ", 0xC, 0x0); // Red text
        framebuffer_write(7, 6, '0' + (status < 0 ? -status : status), 0xC, 0x0);

        // Check if it's a specific error we can identify
        if (status == -1)
        {
            debug_print_string(0, 7, "File not found", 0xC, 0x0);
        }
        else if (status == -2)
        {
            debug_print_string(0, 7, "Read error", 0xC, 0x0);
        }
        else if (status == -3)
        {
            debug_print_string(0, 7, "Buffer too small", 0xC, 0x0);
        }

        while (true)
            ;
    }

    debug_print_string(0, 6, "Shell loaded OK", 0xA, 0x0); // Green text

    // Verify shell data
    if (shell_addr[0] == 0x7F && shell_addr[1] == 'E' &&
        shell_addr[2] == 'L' && shell_addr[3] == 'F')
    {
        debug_print_string(0, 7, "ELF detected", 0xA, 0x0);
        // TODO: Add ELF parsing here
    }
    else
    {
        debug_print_string(0, 7, "Raw binary", 0xE, 0x0); // Yellow text
        // Show first few bytes for debugging
        for (int i = 0; i < 8; i++)
        {
            char hex_chars[] = "0123456789ABCDEF";
            framebuffer_write(12 + i * 2, 7, hex_chars[(shell_addr[i] >> 4) & 0xF], 0xE, 0x0);
            framebuffer_write(13 + i * 2, 7, hex_chars[shell_addr[i] & 0xF], 0xE, 0x0);
        }
    }

    debug_print_string(0, 8, "Jumping to shell...", 0xF, 0x0);

    // Add debugging - this should appear if we reach here
    framebuffer_write(0, 10, 'J', 0xC, 0x0); // Red 'J' for Jump

    // Set TSS $esp pointer and jump into shell
    set_tss_kernel_current_stack();
    kernel_execute_user_program(shell_addr);

    // Add debugging - this should NEVER appear
    framebuffer_write(1, 10, 'R', 0xC, 0x0); // Red 'R' for Return (bad!)

    // This should never be reached
    debug_print_string(0, 9, "ERROR: Returned from shell!", 0xC, 0x0);

    while (true)
        ;
}