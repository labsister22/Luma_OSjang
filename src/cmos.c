#include "header/driver/cmos.h"
#include "header/cpu/portio.h"


int clock_enabled = 0;
static uint8_t read_cmos_register(uint8_t reg) {
    out(0x70, reg);
    return in(0x71);
}

static uint8_t bcd_to_bin(uint8_t val) {
    return (val & 0x0F) + ((val >> 4) * 10);
}

struct Time get_cmos_time() {
    struct Time t;
    t.second = bcd_to_bin(read_cmos_register(0x00));
    t.minute = bcd_to_bin(read_cmos_register(0x02));
    t.hour   = bcd_to_bin(read_cmos_register(0x04));
    return t;
}
