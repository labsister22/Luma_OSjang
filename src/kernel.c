
#include <stdbool.h>
#include <stdint.h>
#include "header/cpu/gdt.h"
#include "header/kernel-entrypoint.h"
#include "header/text/framebuffer.h"
#include "header/cpu/interrupt.h"
#include "header/cpu/idt.h"

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
  framebuffer_clear();
  framebuffer_set_cursor(0, 0);
  __asm__("int $0x4");
  while (true)
    ;
}
