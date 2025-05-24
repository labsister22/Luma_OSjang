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

// void kernel_setup(void)
// {
//   load_gdt(&_gdt_gdtr);
//   pic_remap();
//   initialize_idt();
//   activate_keyboard_interrupt();
//   framebuffer_clear();
//   framebuffer_set_cursor(0, 0);

//   // Debug marker A - Basic initialization done
//   framebuffer_write(0, 0, 'A', 0x0F, 0x00);

//   initialize_filesystem_ext2();

//   // Debug marker B - Filesystem initialized
//   framebuffer_write(0, 1, 'B', 0x0F, 0x00);

//   gdt_install_tss();
//   set_tss_register();

//   // Debug marker C - TSS setup
//   framebuffer_write(0, 2, 'C', 0x0F, 0x00);

//   // Allocate first 4 MiB virtual memory for user program
//   bool alloc_success = paging_allocate_user_page_frame(&_paging_kernel_page_directory, (uint8_t *)0);

//   if (!alloc_success) {
//       framebuffer_write(0, 3, 'E', 0x4F, 0x00); // Red E for allocation error
//       while(1);
//   }

//   // Debug marker D - Memory allocated
//   framebuffer_write(0, 3, 'D', 0x0F, 0x00);

//   // Try to read shell from filesystem
//   struct EXT2DriverRequest request = {
//       .buf = (uint8_t *)0,
//       .name = "shell",
//       .parent_inode = 2,  // Root directory
//       .buffer_size = 0x100000,
//       .name_len = 5,
//   };

//   // Debug marker before read
//   framebuffer_write(0, 4, 'R', 0x0F, 0x00);

//   int8_t read_result = read(request);

//   if (read_result != 0) {
//       // Shell not found or read error
//       framebuffer_write(0, 5, 'F', 0x4F, 0x00);

//       // Show error code
//       char error_digit = '0' + (read_result < 0 ? -read_result : read_result);
//       if (error_digit > '9') error_digit = '9';
//       framebuffer_write(1, 5, error_digit, 0x4F, 0x00);

//       // For now, create a simple inline shell instead of loading from disk
//       framebuffer_write(0, 6, 'I', 0x0E, 0x00); // Yellow I for "Inline shell"

//       // Create a minimal shell program in memory
//       // This is a simple infinite loop that just exists
//       uint8_t *shell_mem = (uint8_t *)0;

//       // x86 assembly for: jmp $ (infinite loop)
//       shell_mem[0] = 0xEB;  // JMP short
//       shell_mem[1] = 0xFE;  // -2 (jump to itself)

//       // Continue with user program execution
//   } else {
//       // Shell loaded successfully
//       framebuffer_write(0, 5, 'S', 0x0F, 0x00);
//   }

//   // Set TSS for kernel stack and jump into user program
//   set_tss_kernel_current_stack();

//   // Debug marker before user program execution
//   framebuffer_write(0, 6, 'U', 0x0F, 0x00);

//   kernel_execute_user_program((uint8_t *)0);

//   // Should never reach here
//   framebuffer_write(0, 7, 'X', 0x4F, 0x00);
//   while (true);
// }

void kernel_setup(void)
{
  framebuffer_write(0, 0, '1', 0x0F, 0x00);
  load_gdt(&_gdt_gdtr);

  framebuffer_write(0, 1, '2', 0x0F, 0x00);
  pic_remap();

  framebuffer_write(0, 2, '3', 0x0F, 0x00);
  activate_keyboard_interrupt();

  framebuffer_write(0, 3, '4', 0x0F, 0x00);
  paging_activate(&_paging_kernel_page_directory);

  load_gdt(&_gdt_gdtr);
  pic_remap();
  activate_keyboard_interrupt();
  paging_activate(&_paging_kernel_page_directory);

  framebuffer_clear();
  framebuffer_set_cursor(0, 0);

  initialize_filesystem_ext2();

  // Step 5: Add TSS and memory allocation
  gdt_install_tss();
  set_tss_register();

  // Allocate memory for shell
  bool allocation_success = paging_allocate_user_page_frame(&_paging_kernel_page_directory, (uint8_t *)0);

  if (!allocation_success)
  {
    framebuffer_write(0, 0, 'E', 0x4F, 0x00);
    while (1)
      ;
  }

  // Show TSS and memory allocation working
  framebuffer_write(0, 0, 'M', 0x0F, 0x00);
  framebuffer_write(1, 0, 'E', 0x0F, 0x00);
  framebuffer_write(2, 0, 'M', 0x0F, 0x00);
  framebuffer_write(3, 0, 'O', 0x0F, 0x00);
  framebuffer_write(4, 0, 'K', 0x0F, 0x00);

  while (true)
    ;
}