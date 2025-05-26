#include "header/driver/keyboard.h"
#include "header/cpu/portio.h"
#include "header/stdlib/string.h"
#include "header/text/framebuffer.h"

// Add these definitions after the includes
#define KEYBOARD_BUFFER_SIZE 256
#define FRAMEBUFFER_WIDTH 80
#define FRAMEBUFFER_HEIGHT 25
#define SHIFT_PRESSED 0x01
#define CAPS_LOCK_ACTIVE 0x02
#define LEFT_SHIFT_PRESSED 0x2A
#define RIGHT_SHIFT_PRESSED 0x36
#define LEFT_SHIFT_RELEASED 0xAA
#define RIGHT_SHIFT_RELEASED 0xB6
#define CAPS_LOCK_PRESSED 0x3A
// static char keyboard_buffer[KEYBOARD_BUFFER_SIZE];
// static uint8_t buffer_index = 0;
// static bool keyboard_input_on = false;

struct KeyboardDriverState keyboard_state = {
    .read_extended_mode = false,
    .keyboard_input_on = false,
    .buffer_index = 0,
    .keyboard_buffer = {0},
    .cursor_x = 0,
    .cursor_y = 0,
};

static uint8_t key_state = 0;
static uint8_t line_end_positions[FRAMEBUFFER_HEIGHT];

const char keyboard_scancode_1_to_ascii_map[256] = {
    0,
    0x1B,
    '1',
    '2',
    '3',
    '4',
    '5',
    '6',
    '7',
    '8',
    '9',
    '0',
    '-',
    '=',
    '\b',
    '\t',
    'q',
    'w',
    'e',
    'r',
    't',
    'y',
    'u',
    'i',
    'o',
    'p',
    '[',
    ']',
    '\n',
    0,
    'a',
    's',
    'd',
    'f',
    'g',
    'h',
    'j',
    'k',
    'l',
    ';',
    '\'',
    ' ',
    0,
    '\\',
    'z',
    'x',
    'c',
    'v',
    'b',
    'n',
    'm',
    ',',
    '.',
    '/',
    0,
    '*',
    0,
    ' ',
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    '-',
    0,
    0,
    0,
    '+',
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,

    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
};

const char keyboard_scancode_1_to_shifted_ascii_map[256] = {
    0,
    0x1B,
    '!',
    '@',
    '#',
    '$',
    '%',
    '^',
    '&',
    '*',
    '(',
    ')',
    '_',
    '+',
    '\b',
    '\t',
    'Q',
    'W',
    'E',
    'R',
    'T',
    'Y',
    'U',
    'I',
    'O',
    'P',
    '{',
    '}',
    '\n',
    0,
    'A',
    'S',
    'D',
    'F',
    'G',
    'H',
    'J',
    'K',
    'L',
    ':',
    '"',
    ' ',
    0,
    '|',
    'Z',
    'X',
    'C',
    'V',
    'B',
    'N',
    'M',
    '<',
    '>',
    '?',
    0,
    '*',
    0,
    ' ',
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    '-',
    0,
    0,
    0,
    '+',
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,

    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
};

char to_uppercase(char c) {
    if (c >= 'a' && c <= 'z') {
        return c - 32; // Shift from lowercase to uppercase in ASCII
    }
    return c;
}

void keyboard_state_activate(void)
{
  keyboard_state.keyboard_input_on = true;
  keyboard_state.buffer_index = 0;
  keyboard_state.cursor_x = 0; // Initialize cursor position
  keyboard_state.cursor_y = 0; // Initialize cursor position
  key_state = 0;
  for (int i = 0; i < FRAMEBUFFER_HEIGHT; i++) {
    line_end_positions[i] = 0;
  }
  memset(keyboard_state.keyboard_buffer, 0, KEYBOARD_BUFFER_SIZE);
}

// Deactivate keyboard ISR / stop listening keyboard interrupt
void keyboard_state_deactivate(void)
{
  keyboard_state.keyboard_input_on = false;
}

// Get keyboard buffer value and flush the buffer - @param buf Pointer to char buffer
void get_keyboard_buffer(char *buf) {
    if (keyboard_state.buffer_index > 0) {
        // Ambil karakter terakhir yang dimasukkan
        *buf = keyboard_state.keyboard_buffer[keyboard_state.buffer_index - 1];
        keyboard_state.buffer_index--;
        
        // Clear karakter yang sudah diambil
        keyboard_state.keyboard_buffer[keyboard_state.buffer_index] = '\0';
    } else {
        *buf = '\0';
    }
}

/* -- Keyboard Interrupt Service Routine -- */

/**
 * Handling keyboard interrupt & process scancodes into ASCII character.
 * Will start listen and process keyboard scancode if keyboard_input_on.
 */
void keyboard_isr(void) {
    uint8_t scancode = in(KEYBOARD_DATA_PORT);

    if (!keyboard_state.keyboard_input_on)
        return;
    
    // Handle shift keys
    if (scancode == LEFT_SHIFT_PRESSED || scancode == RIGHT_SHIFT_PRESSED) {
        key_state |= SHIFT_PRESSED;
        return;
    }
    else if (scancode == LEFT_SHIFT_RELEASED || scancode == RIGHT_SHIFT_RELEASED) {
        key_state &= ~SHIFT_PRESSED;
        return;
    }
    else if (scancode == CAPS_LOCK_PRESSED && !(scancode & 0x80)) {
        key_state ^= CAPS_LOCK_ACTIVE;
        return;
    }

    // Only process key press (not release)
    if (!(scancode & 0x80)) {
        char ascii;
        if (key_state & SHIFT_PRESSED) {
            ascii = keyboard_scancode_1_to_shifted_ascii_map[scancode];
        } else {
            ascii = keyboard_scancode_1_to_ascii_map[scancode];
            
            if ((key_state & CAPS_LOCK_ACTIVE) && ascii >= 'a' && ascii <= 'z') {
                ascii = to_uppercase(ascii);
            }
        }
        
        // HANYA simpan ke buffer, JANGAN tampilkan
        if (ascii && keyboard_state.buffer_index < KEYBOARD_BUFFER_SIZE - 1) {
            keyboard_state.keyboard_buffer[keyboard_state.buffer_index++] = ascii;
        }
    }
}

// void handle_backspace() {
//   if (keyboard_state.cursor_x == 0 && keyboard_state.cursor_y > 0) {
//     keyboard_state.cursor_y--; // Move to previous line
    
//     // Go to the last known character position on the previous line
//     keyboard_state.cursor_x = line_end_positions[keyboard_state.cursor_y];
    
//     // If the line was empty, position at the start
//     if (keyboard_state.cursor_x == 0) {
//         keyboard_state.cursor_x = 0;
//     }
    
//     // Clear the character at the cursor position
//     print_str(" ", keyboard_state.cursor_y, keyboard_state.cursor_x, 0x07);
//   } 
//   else if (keyboard_state.cursor_x > 0) {
//     // Normal backspace within a line
//     keyboard_state.cursor_x--;
//     print_str(" ", keyboard_state.cursor_y, keyboard_state.cursor_x, 0x07);
    
//     // Update the line end position if needed
//     if (keyboard_state.cursor_x < line_end_positions[keyboard_state.cursor_y]) {
//         line_end_positions[keyboard_state.cursor_y] = keyboard_state.cursor_x;
//     }
//   }
  
//   if (keyboard_state.buffer_index > 0) {
//     keyboard_state.buffer_index--;
//     keyboard_state.keyboard_buffer[keyboard_state.buffer_index] = '\0';
//   }
// }