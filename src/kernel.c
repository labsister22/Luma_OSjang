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

// void kernel_setup(void) {
//     framebuffer_write(0, 0, 'L', 0xF, 0x0);
//     framebuffer_write(0, 1, 'u', 0xF, 0x0);
//     framebuffer_write(0, 2, 'm', 0xF, 0x0);
//     framebuffer_write(0, 3, 'a', 0xF, 0x0);
//     framebuffer_write(0, 4, ' ', 0xF, 0x0);
//     framebuffer_write(0, 5, 'O', 0xF, 0x0);
//     framebuffer_write(0, 6, 'S', 0xF, 0x0);
// }

void kernel_setup(void)
{
  load_gdt(&_gdt_gdtr);
  pic_remap();
  initialize_idt();
  activate_keyboard_interrupt();
  asm volatile("sti");
  framebuffer_clear();
  framebuffer_set_cursor(0, 0);

  int row = 0, col = 0;
  keyboard_state_activate();
  struct BlockBuffer b;
  framebuffer_write(0, 0, 'L', 0xF, 0x0);
  for (int i = 0; i < 512; i++) {b.buf[i] = i % 16;}
  write_blocks(&b, 17, 1);
  framebuffer_write(0, 0, 'L', 0xF, 0x0);
  while (true)
  {
    char c;
    get_keyboard_buffer(&c);
    if (c)
    {
      framebuffer_write(row, col, c, 0xF, 0);
      if (col >= FRAMEBUFFER_WIDTH)
      {
        ++row;
        col = 0;
      }
      else
      {
        ++col;
      }
      framebuffer_set_cursor(row, col);
    }
  }
}
// void kernel_setup(void) {
//   load_gdt(&_gdt_gdtr);
//   pic_remap();
//   activate_keyboard_interrupt();
//   initialize_idt();
//   framebuffer_clear();
//   framebuffer_set_cursor(0, 0);

//   struct BlockBuffer b;
//   for (int i = 0; i < 512; i++) {b.buf[i] = i % 16;}
//   write_blocks(&b, 17, 1);
//   while (true);
// }