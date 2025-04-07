#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "header/text/framebuffer.h"
#include "header/stdlib/string.h"
#include "header/cpu/portio.h"

void framebuffer_set_cursor(uint8_t r, uint8_t c)
{
  uint16_t pos = r * 80 + c;
  out(0x3D4, 0x0E);
  out(0x3D5, (pos >> 8) & 0xFF);
  out(0x3D4, 0x0F);
  out(0x3D5, pos & 0xFF);
}

void framebuffer_write(uint8_t row, uint8_t col, char c, uint8_t fg, uint8_t bg)
{
  uint16_t *fb = (uint16_t *)FRAMEBUFFER_MEMORY_OFFSET;
  uint16_t pos = row * 80 + col;
  fb[pos] = (fg | (bg << 4)) << 8 | c;
}

void framebuffer_clear(void)
{
  uint16_t *fb = (uint16_t *)FRAMEBUFFER_MEMORY_OFFSET;
  uint16_t blank = (0x07 << 8) | ' ';
  for (int i = 0; i < 80 * 25; i++)
  {
    fb[i] = blank;
  }
  framebuffer_set_cursor(0, 0);
}

void print_str(const char *str, uint8_t row, uint8_t col, uint8_t color)
{
  uint8_t fg = color & 0x0F;
  uint8_t bg = (color >> 4) & 0x0F;
  uint8_t current_row = row;
  uint8_t current_col = col;

  while (*str)
  {
    framebuffer_write(current_row, current_col, *str, fg, bg);
    current_col++;
    str++;

    if (current_col >= 80)
    {
      current_col = 0;
      current_row++;
      if (current_row >= 25)
        current_row = 24; // Limit rows to prevent overflow
    }
    // Update cursor position after each character
    framebuffer_set_cursor(current_row, current_col);
  }
}