#ifndef CMOS_H
#define CMOS_H

#include <stdint.h>
#include "header/cpu/portio.h"

struct Time {
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
};

struct Time get_cmos_time();

#endif
