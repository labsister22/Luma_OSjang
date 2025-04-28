#ifndef _IDT_H
#define _IDT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// IDT hard limit, see Intel x86 manual 3a - 6.10 Interrupt Descriptor Table
#define IDT_MAX_ENTRY_COUNT 256
#define ISR_STUB_TABLE_LIMIT 64

// Struct defined exactly in Intel x86 Vol 3a - Figure 6-2. IDT Gate Descriptors
struct __attribute__((packed)) IDTGate {
    uint16_t offset_low;      // Lower 16 bits of handler function address
    uint16_t segment;         // GDT segment selector
    uint8_t reserved;         // Reserved, always 0
    uint8_t type_attr;        // Type and attributes
    uint16_t offset_high;     // Higher 16 bits of handler function address
};

struct __attribute__((packed)) IDTR {
    uint16_t limit; // Size of IDT in bytes - 1
    uintptr_t base; // Base address of IDT
};

extern void *isr_stub_table[ISR_STUB_TABLE_LIMIT];
extern struct IDTGate interrupt_descriptor_table[IDT_MAX_ENTRY_COUNT];
extern struct IDTR _idt_idtr;

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