#include "header/cpu/gdt.h"

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
        } // Kernel Data Segment
    }};

/**
 * _gdt_gdtr, predefined system GDTR.
 * GDT pointed by this variable is already set to point global_descriptor_table above.
 * From: https://wiki.osdev.org/Global_Descriptor_Table, GDTR.size is GDT size minus 1.
 */
struct GDTR _gdt_gdtr = {
    .size = sizeof(global_descriptor_table) - 1,
    .address = &global_descriptor_table};