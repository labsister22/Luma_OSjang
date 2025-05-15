#include "header/cpu/gdt.h"
#include "header/cpu/interrupt.h"

/**
 * global_descriptor_table, predefined GDT.
 * Initial SegmentDescriptor already set properly according to Intel Manual & OSDev.
 * Table entry : [{Null Descriptor}, {Kernel Code}, {Kernel Data (variable, etc)}, ...].
 */
struct GlobalDescriptorTable global_descriptor_table = {
    .table = {

        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // Null descriptor
        {
            .segment_low = 0xFFFF,     // Limit 0-15
            .base_low = 0x0000,        // Base 0-15
            .base_mid = 0x00,          // Base 16-23
            .type_bit = 0xA,           // Code segment, Executable, Readable
            .non_system = 1,           // 1 = Code/Data segment
            .dpl = 0,                  // Ring 0 (kernel mode)
            .present = 1,              // Segment aktif
            .segment_limit_high = 0xF, // Limit 16-19 (0xFFFFF total)
            .available = 0,            // Unused
            .long_mode = 0,            // 32-bit protected mode
            .default_big = 1,          // 1 = 32-bit segment
            .granularity = 1,          // 1 = Limit dalam 4 KiB
            .base_high = 0x00          // Base 24-31
        },                             // Kernel Code Segment
        {
            .segment_low = 0xFFFF,     // Limit 0-15
            .base_low = 0x0000,        // Base 0-15
            .base_mid = 0x00,          // Base 16-23
            .type_bit = 0x2,           // Data segment, Writable
            .non_system = 1,           // 1 = Code/Data segment
            .dpl = 0,                  // Ring 0 (kernel mode)
            .present = 1,              // Segment aktif
            .segment_limit_high = 0xF, // Limit 16-19 (0xFFFFF total)
            .available = 0,            // Unused
            .long_mode = 0,            // 32-bit protected mode
            .default_big = 1,          // 1 = 32-bit segment
            .granularity = 1,          // 1 = Limit dalam 4 KiB
            .base_high = 0x00          // Base 24-31
        }, // Kernel Data Segment
        // User Code Segment - Same as kernel but with DPL=3
        {
            .segment_low = 0xFFFF,     // Limit 0-15
            .base_low = 0x0000,        // Base 0-15
            .base_mid = 0x00,          // Base 16-23
            .type_bit = 0xA,           // Code segment, Executable, Readable
            .non_system = 1,           // 1 = Code/Data segment
            .dpl = 3,                  // Ring 3 (user mode)
            .present = 1,              // Segment aktif
            .segment_limit_high = 0xF, // Limit 16-19 (0xFFFFF total)
            .available = 0,            // Unused
            .long_mode = 0,            // 32-bit protected mode
            .default_big = 1,          // 1 = 32-bit segment
            .granularity = 1,          // 1 = Limit dalam 4 KiB
            .base_high = 0x00          // Base 24-31
        },
        // User Data Segment - Same as kernel but with DPL=3
        {
            .segment_low = 0xFFFF,     // Limit 0-15
            .base_low = 0x0000,        // Base 0-15
            .base_mid = 0x00,          // Base 16-23
            .type_bit = 0x2,           // Data segment, Writable
            .non_system = 1,           // 1 = Code/Data segment
            .dpl = 3,                  // Ring 3 (user mode) 
            .present = 1,              // Segment aktif
            .segment_limit_high = 0xF, // Limit 16-19 (0xFFFFF total)
            .available = 0,            // Unused
            .long_mode = 0,            // 32-bit protected mode
            .default_big = 1,          // 1 = 32-bit segment
            .granularity = 1,          // 1 = Limit dalam 4 KiB
            .base_high = 0x00          // Base 24-31
        },
        // TSS Entry
        {
            .segment_low = sizeof(struct TSSEntry),
            .base_low = 0,
            .base_mid = 0,
            .type_bit = 0x9,           // 32-bit TSS (Available)
            .non_system = 0,           // 0 = System segment
            .dpl = 0,                  // Ring 0
            .present = 1,              // Present
            .segment_limit_high = (sizeof(struct TSSEntry) & (0xF << 16)) >> 16,
            .available = 0,
            .long_mode = 0,
            .default_big = 1,          // 32-bit
            .granularity = 0,          // Byte granularity
            .base_high = 0
        },
        
        // Additional empty entry
        {0}
    }};

/**
 * _gdt_gdtr, predefined system GDTR.
 * GDT pointed by this variable is already set to point global_descriptor_table above.
 * From: https://wiki.osdev.org/Global_Descriptor_Table, GDTR.size is GDT size minus 1.
 */
struct GDTR _gdt_gdtr = {

    .size = sizeof(global_descriptor_table) - 1,
    .address = &global_descriptor_table};

void gdt_install_tss(void) {
    uint32_t base = (uint32_t) &_interrupt_tss_entry;
    global_descriptor_table.table[5].base_high = (base & (0xFF << 24)) >> 24;
    global_descriptor_table.table[5].base_mid  = (base & (0xFF << 16)) >> 16;
    global_descriptor_table.table[5].base_low  = base & 0xFFFF;
}

