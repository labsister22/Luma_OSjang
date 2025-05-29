// filepath: /os-hardware-drivers/os-hardware-drivers/header/speaker_driver.h
#ifndef _SPEAKER_H
#define _SPEAKER_H

#include <stdint.h>

// Speaker control constants
#define SPEAKER_PORT 0x61
#define MAX_VOLUME 100
#define MIN_VOLUME 0
#define BEEP_FREQUENCY 400 // Frequency for beep sound

// Function prototypes
void speaker_init(void);
void speaker_play(uint32_t frequency);
void speaker_stop(void);

#endif