#include "header/driver/vga_graphics.h"
#include <stdint.h>
#include <stddef.h>

// VGA Colors
#define VGA_COLOR_BLACK 0
#define VGA_COLOR_WHITE 15
#define DEFAULT_FG_COLOR VGA_COLOR_WHITE
#define DEFAULT_BG_COLOR VGA_COLOR_BLACK

// VGA text mode memory
static volatile uint16_t* const VGA_MEMORY = (volatile uint16_t*)0xB8000;
static int vga_initialized = 0;

static uint8_t vga_entry_color(uint8_t fg, uint8_t bg) {
    return fg | (bg << 4);
}

static uint16_t vga_entry(unsigned char uc, uint8_t color) {
    return (uint16_t) uc | ((uint16_t) color << 8);
}

void vga_graphics_init(void) {
    // MINIMAL INIT - just set flag
    vga_initialized = 1;
    // Don't touch VGA memory here to avoid page fault during boot
}

void vga_plot_pixel(int x, int y, uint8_t color_index) {
    (void)x; (void)y; (void)color_index;
}

void vga_clear_screen_kernel(void) {
    if (!vga_initialized) return;
    
    uint8_t color = vga_entry_color(DEFAULT_FG_COLOR, DEFAULT_BG_COLOR);
    uint16_t blank = vga_entry(' ', color);
    
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        VGA_MEMORY[i] = blank;
    }
}

void vga_set_cursor_kernel(int col, int row) {
    if (!vga_initialized) return;
    
    if (row < 0) row = 0;
    if (row >= VGA_HEIGHT) row = VGA_HEIGHT - 1;
    if (col < 0) col = 0;
    if (col >= VGA_WIDTH) col = VGA_WIDTH - 1;

    uint16_t pos = row * VGA_WIDTH + col;
    
    __asm__ volatile("outb %1, %0" : : "dN"(0x3D4), "a"((uint8_t)0x0F));
    __asm__ volatile("outb %1, %0" : : "dN"(0x3D5), "a"((uint8_t)(pos & 0xFF)));
    __asm__ volatile("outb %1, %0" : : "dN"(0x3D4), "a"((uint8_t)0x0E));
    __asm__ volatile("outb %1, %0" : : "dN"(0x3D5), "a"((uint8_t)((pos >> 8) & 0xFF)));
}

void vga_draw_char_kernel(char c, int row, int col, uint8_t fg, uint8_t bg) {
    if (!vga_initialized) return;
    if (row < 0 || row >= VGA_HEIGHT || col < 0 || col >= VGA_WIDTH) return;
    
    uint8_t color = vga_entry_color(fg, bg);
    size_t index = row * VGA_WIDTH + col;
    VGA_MEMORY[index] = vga_entry(c, color);
}

void vga_scroll_up_kernel(void) {
    if (!vga_initialized) return;
    
    for (int i = 0; i < (VGA_HEIGHT - 1) * VGA_WIDTH; i++) {
        VGA_MEMORY[i] = VGA_MEMORY[i + VGA_WIDTH];
    }
    
    uint8_t color = vga_entry_color(DEFAULT_FG_COLOR, DEFAULT_BG_COLOR);
    uint16_t blank = vga_entry(' ', color);
    
    for (int i = (VGA_HEIGHT - 1) * VGA_WIDTH; i < VGA_HEIGHT * VGA_WIDTH; i++) {
        VGA_MEMORY[i] = blank;
    }
}