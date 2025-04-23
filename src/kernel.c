#include <stdbool.h>
#include <stdint.h>
#include "header/cpu/gdt.h"
#include "header/kernel-entrypoint.h"
#include "header/text/framebuffer.h"
#include "header/cpu/portio.h"
#include "header/cpu/idt.h"
#include "header/cpu/interrupt.h"
#include "header/driver/keyboard.h"

// void kernel_setup(void) {
//     framebuffer_write(0, 0, 'L', 0xF, 0x0);
//     framebuffer_write(0, 1, 'u', 0xF, 0x0);
//     framebuffer_write(0, 2, 'm', 0xF, 0x0);
//     framebuffer_write(0, 3, 'a', 0xF, 0x0);
//     framebuffer_write(0, 4, ' ', 0xF, 0x0);
//     framebuffer_write(0, 5, 'O', 0xF, 0x0);
//     framebuffer_write(0, 6, 'S', 0xF, 0x0);
// }

#include "header/text/framebuffer.h"
#include "header/cpu/interrupt.h"
#include "header/cpu/idt.h"
#include "header/driver/keyboard.h"

void kernel_setup(void)
{
  load_gdt(&_gdt_gdtr);
  pic_remap();
  initialize_idt();
  activate_keyboard_interrupt();
  framebuffer_clear();
  framebuffer_set_cursor(0, 0);

  int row = 0, col = 0;
  keyboard_state_activate();
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

