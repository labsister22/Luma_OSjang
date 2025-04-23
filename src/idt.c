
#include "header/cpu/idt.h"
#include "header/cpu/interrupt.h"
#include "header/cpu/gdt.h"
#include <stdint.h>

struct IDTGate interrupt_descriptor_table[IDT_MAX_ENTRY_COUNT] = {0};
struct IDTR _idt_idtr = {
    .limit = sizeof(struct IDTGate) * IDT_MAX_ENTRY_COUNT - 1,
    .base = (uintptr_t)&interrupt_descriptor_table};

void set_interrupt_gate(
    uint8_t int_vector,
    void *handler_address,
    uint16_t gdt_seg_selector,
    uint8_t privilege)
{
  struct IDTGate *idt_int_gate = &interrupt_descriptor_table[int_vector];
  uint32_t handler = (uint32_t)handler_address;

  idt_int_gate->offset_low = handler & 0xFFFF;
  idt_int_gate->offset_high = (handler >> 16) & 0xFFFF;
  idt_int_gate->segment = gdt_seg_selector;
  idt_int_gate->_r_bit_1 = INTERRUPT_GATE_R_BIT_1;
  idt_int_gate->_r_bit_2 = INTERRUPT_GATE_R_BIT_2;
  idt_int_gate->gate_32 = 1;
  idt_int_gate->_r_bit_3 = INTERRUPT_GATE_R_BIT_3;
  idt_int_gate->privilege = privilege & 0x3;
  idt_int_gate->valid_bit = 1; // Ensure no duplicate or conflicting assignments
}

void initialize_idt(void)
{
  for (int i = 0; i < ISR_STUB_TABLE_LIMIT; i++)
  {
    set_interrupt_gate(i, isr_stub_table[i], GDT_KERNEL_CODE_SEGMENT_SELECTOR, 0);
  }

  // Load IDT and enable interrupts
  __asm__ volatile("lidt %0" : : "m"(_idt_idtr));
  __asm__ volatile("sti");
}

#include "header/cpu/idt.h"
#include "header/cpu/interrupt.h"
#include "header/cpu/gdt.h"
#include "header/driver/keyboard.h"
#include <stdint.h>


struct IDTGate interrupt_descriptor_table[IDT_MAX_ENTRY_COUNT] = {0};
struct IDTR _idt_idtr = {
    .limit = sizeof(struct IDTGate) * IDT_MAX_ENTRY_COUNT - 1,
    .base = (uintptr_t)&interrupt_descriptor_table
};

extern void keyboard_isr_stub();

void set_interrupt_gate(
    uint8_t  int_vector, 
    void     *handler_address, 
    uint16_t gdt_seg_selector, 
    uint8_t  privilege
) {
    struct IDTGate *idt_int_gate = &interrupt_descriptor_table[int_vector];
    uint32_t handler = (uint32_t) handler_address;

    idt_int_gate->offset_low  = handler & 0xFFFF;
    idt_int_gate->offset_high = (handler >> 16) & 0xFFFF;
    idt_int_gate->segment     = gdt_seg_selector;
    idt_int_gate->_r_bit_1    = INTERRUPT_GATE_R_BIT_1;
    idt_int_gate->_r_bit_2    = INTERRUPT_GATE_R_BIT_2;
    idt_int_gate->gate_32     = 1;
    idt_int_gate->_r_bit_3    = INTERRUPT_GATE_R_BIT_3;
    idt_int_gate->privilege   = privilege & 0x3;
    idt_int_gate->valid_bit   = 1; // Ensure no duplicate or conflicting assignments
}

void initialize_idt(void) {
    for (int i = 0; i < ISR_STUB_TABLE_LIMIT; i++) {
        set_interrupt_gate(i, isr_stub_table[i], GDT_KERNEL_CODE_SEGMENT_SELECTOR, 0);
    }
    set_interrupt_gate(0x21, keyboard_isr_stub, GDT_KERNEL_CODE_SEGMENT_SELECTOR, 0);
    // Load IDT and enable interrupts
    __asm__ volatile("lidt %0" : : "m"(_idt_idtr));
    __asm__ volatile("sti");
}