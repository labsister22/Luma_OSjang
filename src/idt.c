#include "header/cpu/idt.h"
#include "header/cpu/interrupt.h"
#include "header/cpu/gdt.h"
#include <stdint.h>

struct IDTGate interrupt_descriptor_table[IDT_MAX_ENTRY_COUNT] = {0};

struct IDTR _idt_idtr = {
    .limit = sizeof(struct IDTGate) * IDT_MAX_ENTRY_COUNT - 1,
    .base = (uintptr_t)&interrupt_descriptor_table
};

void set_interrupt_gate(uint8_t int_vector, void *handler_address, uint16_t gdt_seg_selector, uint8_t dpl) {
    struct IDTGate *idt_int_gate = &interrupt_descriptor_table[int_vector];
    uint32_t handler = (uint32_t)handler_address;

    idt_int_gate->offset_low = handler & 0xFFFF;
    idt_int_gate->segment = gdt_seg_selector;
    idt_int_gate->reserved = 0;
    idt_int_gate->type_attr = 0x80 | (dpl << 5) | 0x0E;  // P=1, DPL, Type=0xE (32-bit interrupt gate)
    idt_int_gate->offset_high = (handler >> 16) & 0xFFFF;
}

void initialize_idt(void) {
    for (int i = 0; i < ISR_STUB_TABLE_LIMIT; i++) {
        set_interrupt_gate(i, isr_stub_table[i], GDT_KERNEL_CODE_SEGMENT_SELECTOR, 0);
    }

    __asm__ volatile("lidt %0" : : "m"(_idt_idtr));
    __asm__ volatile("sti");
}
