#include "header/driver/keyboard.h"
#include "header/cpu/portio.h"
#include "header/stdlib/string.h"
#include "header/text/framebuffer.h"

// Add these definitions after the includes
// #define KEYBOARD_BUFFER_SIZE 256
// static char keyboard_buffer[KEYBOARD_BUFFER_SIZE];
// static uint8_t buffer_index = 0;
// static bool keyboard_input_on = false;

struct KeyboardDriverState keyboard_state = {
    .read_extended_mode = false,
    .keyboard_input_on = false,
    .buffer_index = 0,
    .keyboard_buffer = {0},
};

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

void keyboard_state_activate(void)
{
  keyboard_state.keyboard_input_on = true;
  keyboard_state.buffer_index = 0;
  keyboard_state.cursor_x = 0; // Initialize cursor position
  keyboard_state.cursor_y = 0; // Initialize cursor position
  memset(keyboard_state.keyboard_buffer, 0, KEYBOARD_BUFFER_SIZE);
}

// Deactivate keyboard ISR / stop listening keyboard interrupt
void keyboard_state_deactivate(void)
{
  keyboard_state.keyboard_input_on = false;
}

// Get keyboard buffer value and flush the buffer - @param buf Pointer to char buffer
void get_keyboard_buffer(char *buf)
{
  memcpy(buf, keyboard_state.keyboard_buffer, keyboard_state.buffer_index);
  buf[keyboard_state.buffer_index] = '\0';
  keyboard_state.buffer_index = 0;
  memset(keyboard_state.keyboard_buffer, 0, KEYBOARD_BUFFER_SIZE);
}

/* -- Keyboard Interrupt Service Routine -- */

/**
 * Handling keyboard interrupt & process scancodes into ASCII character.
 * Will start listen and process keyboard scancode if keyboard_input_on.
 */
void keyboard_isr(void)
{
  uint8_t scancode = in(KEYBOARD_DATA_PORT);

  if (!keyboard_state.keyboard_input_on)
    return;

  if (!(scancode & 0x80))
  {
    char ascii = keyboard_scancode_1_to_ascii_map[scancode];
    if (ascii && keyboard_state.buffer_index < KEYBOARD_BUFFER_SIZE - 1)
    {
      // Store character in buffer
      keyboard_state.keyboard_buffer[keyboard_state.buffer_index++] = ascii;

      // Create a temporary single character string
      char temp[2] = {ascii, '\0'};

      // Handle special characters
      if (ascii == '\n')
      {
        keyboard_state.cursor_x = 0;
        keyboard_state.cursor_y++;
      }
      else if (ascii == '\b')
      {
        if (keyboard_state.cursor_x > 0)
        {
          keyboard_state.cursor_x--;
          print_str(" ", keyboard_state.cursor_y, keyboard_state.cursor_x, 0x07);
        }
      }
      else
      {
        // Display character and advance cursor
        print_str(temp, keyboard_state.cursor_y, keyboard_state.cursor_x, 0x07);
        keyboard_state.cursor_x++;

        // Handle line wrapping
        if (keyboard_state.cursor_x >= 80)
        {
          keyboard_state.cursor_x = 0;
          keyboard_state.cursor_y++;
        }
      }

      // Update hardware cursor position
      framebuffer_set_cursor(keyboard_state.cursor_y, keyboard_state.cursor_x);
    }
  }
}