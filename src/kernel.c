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
#include "header/shell/user-shell.h"

void kernel_setup(void)
{
  load_gdt(&_gdt_gdtr);
  pic_remap();
  initialize_idt();
  activate_keyboard_interrupt();
  framebuffer_clear();
  framebuffer_set_cursor(0, 0);
  initialize_filesystem_ext2();
  gdt_install_tss();
  set_tss_register();

  // Allocate first 4 MiB virtual memory
  paging_allocate_user_page_frame(&_paging_kernel_page_directory, (uint8_t *)0);

  // Debugging
  framebuffer_write(0, 0, 'R', 0x0F, 0x00);
  framebuffer_write(0, 1, 'E', 0x0F, 0x00);
  framebuffer_write(0, 2, 'A', 0x0F, 0x00);
  framebuffer_write(0, 3, 'D', 0x0F, 0x00);
  framebuffer_write(0, 4, 'Y', 0x0F, 0x00);
  framebuffer_write(0, 5, 'Y', 0x0F, 0x00);

  // Write shell into memory
  struct EXT2DriverRequest request = {
      .buf = (uint8_t *)0,
      .name = "shell",
      .parent_inode = 1,
      .buffer_size = 0x100000,
      .name_len = 5,
  };
  read(request);

  // Set TSS $esp pointer and jump into shell
  set_tss_kernel_current_stack();
  kernel_execute_user_program((uint8_t *)0);

  while (true)
    ;
}
