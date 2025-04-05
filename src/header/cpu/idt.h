#ifndef _IDT_H
#define _IDT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// IDT hard limit, see Intel x86 manual 3a - 6.10 Interrupt Descriptor Table
#define IDT_MAX_ENTRY_COUNT    256
#define ISR_STUB_TABLE_LIMIT   64
#define INTERRUPT_GATE_R_BIT_1 0b000
#define INTERRUPT_GATE_R_BIT_2 0b110
#define INTERRUPT_GATE_R_BIT_3 0b0
struct IDTGate {
    // First 32-bit (Bit 0 to 31)
    uint16_t offset_low;
    uint16_t segment;        // Selector segment di GDT
    uint8_t  _reserved : 5;  // Harus 0
    uint8_t  _r_bit_1  : 3;  // Harus 0b000
    uint8_t  _r_bit_2  : 3;  // Harus 0b110 (Interrupt Gate)
    uint8_t  gate_32   : 1;  // 1 = 32-bit gate, 0 = 16-bit
    uint8_t  _r_bit_3  : 1;  // Harus 0
    uint8_t  privilege : 2;  // Privilege level (0 = kernel, 3 = user)
    uint8_t  valid_bit   : 1;  // 1 jika entry aktif
    uint16_t offset_high;    // Offset handler (bits 16-31)
} __attribute__((packed));
struct IDTR {
    uint16_t limit;  // Panjang IDT dalam byte - 1
    uintptr_t  base;   // Alamat IDT di memori
} __attribute__((packed));
// Interrupt Handler / ISR stub for reducing code duplication, this array can be iterated in initialize_idt()
extern void *isr_stub_table[ISR_STUB_TABLE_LIMIT];
extern struct IDTGate interrupt_descriptor_table[IDT_MAX_ENTRY_COUNT];
extern struct IDTR _idt_idtr;

/**
 * IDTGate, IDT entry that point into interrupt handler
 * Struct defined exactly in Intel x86 Vol 3a - Figure 6-2. IDT Gate Descriptors
 *
 * @param offset_low  Lower 16-bit offset
 * @param segment     Memory segment
 * @param _reserved   Reserved bit, bit length: 5
 * @param _r_bit_1    Reserved for idtgate type, bit length: 3
 * @param _r_bit_2    Reserved for idtgate type, bit length: 3
 * @param gate_32     Is this gate size 32-bit? If not then its 16-bit gate
 * @param _r_bit_3    Reserved for idtgate type, bit length: 1
 * ...
 */




/**
 * Set IDTGate with proper interrupt handler values.
 * Will directly edit global IDT variable and set values properly
 * 
 * @param int_vector       Interrupt vector to handle
 * @param handler_address  Interrupt handler address
 * @param gdt_seg_selector GDT segment selector, for kernel use GDT_KERNEL_CODE_SEGMENT_SELECTOR
 * @param privilege        Descriptor privilege level
 */
void set_interrupt_gate(uint8_t int_vector, void *handler_address, uint16_t gdt_seg_selector, uint8_t privilege);


/**
 * Set IDT with proper values and load with lidt
 */
void initialize_idt(void);

#endif