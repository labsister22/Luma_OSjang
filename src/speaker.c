#include "header/driver/speaker.h"
#include "header/cpu/portio.h" // Include for out and in
#include <stdint.h>

#define SPEAKER_PORT_CTRL 0x61 
#define PIT_CHANNEL2_DATA 0x42
#define PIT_COMMAND_MODE  0x43
#define SOUND_FREQUENCY_BASE 1193180 // Base frequency for PIT

void speaker_init() {
    // Ensure speaker is off by clearing bits 0 and 1 of port 0x61
    uint8_t current_state = in(SPEAKER_PORT_CTRL); // Changed inb to in
    out(SPEAKER_PORT_CTRL, current_state & 0xFC); // Changed outb to out
}

void speaker_play(uint32_t frequency) {
    if (frequency == 0) {
        speaker_stop();
        return;
    }
    uint32_t divisor = SOUND_FREQUENCY_BASE / frequency;

    // Set PIT channel 2 mode: square wave generator
    out(PIT_COMMAND_MODE, 0xB6); // Changed outb to out

    // Send divisor low byte then high byte
    out(PIT_CHANNEL2_DATA, (uint8_t)(divisor & 0xFF)); // Changed outb to out
    out(PIT_CHANNEL2_DATA, (uint8_t)((divisor >> 8) & 0xFF)); // Changed outb to out

    // Enable speaker by setting bits 0 and 1 of port 0x61
    uint8_t current_state = in(SPEAKER_PORT_CTRL); // Changed inb to in
    out(SPEAKER_PORT_CTRL, current_state | 0x03); // Changed outb to out
}

void speaker_stop() {
    // Disable speaker by clearing bits 0 and 1 of port 0x61
    uint8_t current_state = in(SPEAKER_PORT_CTRL); // Changed inb to in
    out(SPEAKER_PORT_CTRL, current_state & 0xFC); // Changed outb to out
}

void set_volume(uint8_t volume) {
    // Volume control implementation (if supported by hardware)
    // This is a placeholder as actual volume control may depend on the hardware.
    (void)volume; // Mark 'volume' as intentionally unused to suppress the warning
}