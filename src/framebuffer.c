#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "header/text/framebuffer.h"
#include "header/stdlib/string.h"
#include "header/cpu/portio.h"

void framebuffer_set_cursor(uint8_t r, uint8_t c) {
    uint16_t pos = r * 80 + c;
    outb(0x3D4, 0x0E);
    outb(0x3D5, (pos >> 8) & 0xFF);
    outb(0x3D4, 0x0F);
    outb(0x3D5, pos & 0xFF);
}

void framebuffer_write(uint8_t row, uint8_t col, char c, uint8_t fg, uint8_t bg) {
    uint16_t *fb = (uint16_t *) FRAMEBUFFER_MEMORY_OFFSET;
    uint16_t pos = row * 80 + col;
    fb[pos] = (fg | (bg << 4)) << 8 | c;
}

void framebuffer_clear(void) {
    uint16_t *fb = (uint16_t *) FRAMEBUFFER_MEMORY_OFFSET;
    uint16_t blank = (0x07 << 8) | ' ';
    for (int i = 0; i < 80 * 25; i++) {
        fb[i] = blank;
    }
    framebuffer_set_cursor(0, 0);
}
