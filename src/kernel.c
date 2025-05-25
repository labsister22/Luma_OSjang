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
  bool allocation_success = paging_allocate_user_page_frame(&_paging_kernel_page_directory, (uint8_t *)0x400000);

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