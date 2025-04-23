#include <stdbool.h>
#include <stdint.h>
#include "header/cpu/gdt.h"
#include "header/kernel-entrypoint.h"
#include "header/text/framebuffer.h"

// void kernel_setup(void) {
//     load_gdt(&_gdt_gdtr);
//     while (true);

// }

#include "header/text/framebuffer.h"
#include "header/cpu/interrupt.h"
#include "header/cpu/idt.h"
#include "header/driver/keyboard.h"
#include "header/driver/disk.h"

// void kernel_setup(void)
// {
//   load_gdt(&_gdt_gdtr);
//   pic_remap();
//   initialize_idt();
//   activate_keyboard_interrupt();
//   framebuffer_clear();
//   framebuffer_set_cursor(0, 0);

//   int row = 0, col = 0;
//   keyboard_state_activate();
//   while (true)
//   {
//     char c;
//     get_keyboard_buffer(&c);
//     if (c)
//     {
//       framebuffer_write(row, col, c, 0xF, 0);
//       if (col >= FRAMEBUFFER_WIDTH)
//       {
//         ++row;
//         col = 0;
//       }
//       else
//       {
//         ++col;
//       }
//       framebuffer_set_cursor(row, col);
//     }
//   }
// }

void kernel_setup(void) {
  load_gdt(&_gdt_gdtr);
  pic_remap();
  activate_keyboard_interrupt();
  initialize_idt();
  framebuffer_clear();
  framebuffer_set_cursor(0, 0);

  struct BlockBuffer b;
  for (int i = 0; i < 512; i++) b.buf[i] = i % 16;
  write_blocks(&b, 17, 1);
  while (true);
}
