#ifndef VGA_GRAPHICS_H
#define VGA_GRAPHICS_H

#include <stdint.h>

// VGA Text Mode dimensions (80x25 standard)
#define VGA_WIDTH 80
#define VGA_HEIGHT 25

// For compatibility with existing code, define TEXT_ROWS and TEXT_COLS
#define TEXT_ROWS VGA_HEIGHT
#define TEXT_COLS VGA_WIDTH

// VGA Colors - TAMBAHKAN INI
#define VGA_COLOR_BLACK 0
#define VGA_COLOR_BLUE 1
#define VGA_COLOR_GREEN 2
#define VGA_COLOR_CYAN 3
#define VGA_COLOR_RED 4
#define VGA_COLOR_MAGENTA 5
#define VGA_COLOR_BROWN 6
#define VGA_COLOR_LIGHT_GREY 7
#define VGA_COLOR_DARK_GREY 8
#define VGA_COLOR_LIGHT_BLUE 9
#define VGA_COLOR_LIGHT_GREEN 10
#define VGA_COLOR_LIGHT_CYAN 11
#define VGA_COLOR_LIGHT_RED 12
#define VGA_COLOR_LIGHT_MAGENTA 13
#define VGA_COLOR_LIGHT_BROWN 14
#define VGA_COLOR_WHITE 15

// Function prototypes
void vga_graphics_init(void);
void vga_plot_pixel(int x, int y, uint8_t color_index);
void vga_clear_screen_kernel(void);
void vga_set_cursor_kernel(int col, int row);
void vga_draw_char_kernel(char c, int row, int col, uint8_t fg, uint8_t bg);
void vga_scroll_up_kernel(void);

#endif // VGA_GRAPHICS_H