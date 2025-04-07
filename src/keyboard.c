#include "header/driver/keyboard.h"
#include "header/text/framebuffer.h"
#include "header/cpu/portio.h"
#include "header/stdlib/string.h"
#include "header/cpu/interrupt.h"

const char keyboard_scancode_1_to_ascii_map[256] = {
      0, 0x1B, '1', '2', '3', '4', '5', '6',  '7', '8', '9',  '0',  '-', '=', '\b', '\t',
    'q',  'w', 'e', 'r', 't', 'y', 'u', 'i',  'o', 'p', '[',  ']', '\n',   0,  'a',  's',
    'd',  'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',   0, '\\',  'z', 'x',  'c',  'v',
    'b',  'n', 'm', ',', '.', '/',   0, '*',    0, ' ',   0,    0,    0,   0,    0,    0,
      0,    0,   0,   0,   0,   0,   0,   0,    0,   0, '-',    0,    0,   0,  '+',    0,
      0,    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,    0,   0,    0,    0,
      0,    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,    0,   0,    0,    0,
      0,    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,    0,   0,    0,    0,

      0,    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,    0,   0,    0,    0,
      0,    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,    0,   0,    0,    0,
      0,    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,    0,   0,    0,    0,
      0,    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,    0,   0,    0,    0,
      0,    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,    0,   0,    0,    0,
      0,    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,    0,   0,    0,    0,
      0,    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,    0,   0,    0,    0,
      0,    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,    0,   0,    0,    0,
};

// Inisialisasi keyboard_state
static struct KeyboardDriverState keyboard_state = {
    .read_extended_mode = false,
    .keyboard_input_on = false,
    .keyboard_buffer = '\0'
};

/* -- Driver Interfaces -- */

// Activate keyboard ISR / start listen keyboard & save to buffer
void keyboard_state_activate(void) {
    keyboard_state.keyboard_input_on = true;
    keyboard_state.keyboard_buffer = '\0';
    activate_keyboard_interrupt(); // Pastikan interrupt keyboard diaktifkan
}

// Deactivate keyboard ISR / stop listening keyboard interrupt
void keyboard_state_deactivate(void) {
    keyboard_state.keyboard_input_on = false;
}

// Get keyboard buffer value and flush the buffer - @param buf Pointer to char buffer
void get_keyboard_buffer(char *buf) {
    *buf = keyboard_state.keyboard_buffer;
    keyboard_state.keyboard_buffer = '\0';
}

/* -- Keyboard Interrupt Service Routine -- */

/**
 * Handling keyboard interrupt & process scancodes into ASCII character.
 * Will start listen and process keyboard scancode if keyboard_input_on.
 */
void keyboard_isr(void) {
    uint8_t scancode = in(KEYBOARD_DATA_PORT);
    
    if (keyboard_state.keyboard_input_on) {
        if (scancode == EXTENDED_SCANCODE_BYTE) {
            keyboard_state.read_extended_mode = true;
        } else if (keyboard_state.read_extended_mode) {
            keyboard_state.read_extended_mode = false;
            switch (scancode) {
                case EXT_SCANCODE_DOWN:
                    // Handle arrow down if needed
                    break;
                case EXT_SCANCODE_UP:
                    // Handle arrow up if needed
                    break;
                case EXT_SCANCODE_LEFT:
                    // Handle arrow left if needed
                    break;
                case EXT_SCANCODE_RIGHT:
                    // Handle arrow right if needed
                    break;
            }
        } else {
            // Cek apakah ini make code (key press)
            if ((scancode & 0x80) == 0) {
                char ascii = keyboard_scancode_1_to_ascii_map[scancode];
                if (ascii != 0) {  // Filter karakter yang tidak valid
                    keyboard_state.keyboard_buffer = ascii;
                }
            }
            // Jika ini break code, kita abaikan
        }
    }
    
    pic_ack(IRQ_KEYBOARD);
}