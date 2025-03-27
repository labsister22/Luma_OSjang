#include <stdbool.h>
#include <stdint.h>
#include "header/cpu/gdt.h"
#include "header/kernel-entrypoint.h"
#include "header/text/framebuffer.h"

// void kernel_setup(void) {
//     load_gdt(&_gdt_gdtr);
//     while (true);

// }

void kernel_setup(void) {
    framebuffer_clear();
    framebuffer_write(3, 8,  'b', 0, 0xF);
    framebuffer_write(3, 9,  'a', 0, 0xF);
    framebuffer_write(3, 10, 'b', 0, 0xF);
    framebuffer_write(3, 11, 'i', 0, 0xF);
    framebuffer_set_cursor(3, 10);
    while (true);
}
