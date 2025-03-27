#include "header/cpu/idt.h"
#include "header/cpu/gdt.h"
#include <stdint.h>
#include <stddef.h>

struct IDT interrupt_descriptor_table = {0};
struct IDTR _idt_idtr;

void initialize_idt(void)
{
  /*
   * TODO:
   * Iterate all isr_stub_table,
   * Set all IDT entry with set_interrupt_gate()
   * with following values:
   * Vector: i
   * Handler Address: isr_stub_table[i]
   * Segment: GDT_KERNEL_CODE_SEGMENT_SELECTOR
   * Privilege: 0
   */
  extern void *isr_stub_table[];

  for (int i = 0; i < 256; i++)
  {
    if (isr_stub_table[i] != NULL)
    {
      set_interrupt_gate(i, isr_stub_table[i], GDT_KERNEL_CODE_SEGMENT_SELECTOR, 0);
    }
    set_interrupt_gate(i, isr_stub_table[i], GDT_KERNEL_CODE_SEGMENT_SELECTOR, 0);
  }

  _idt_idtr.base = (uint32_t)&interrupt_descriptor_table.table;
  _idt_idtr.limit = sizeof(interrupt_descriptor_table.table) - 1;

  __asm__ volatile("lidt %0" : : "m"(_idt_idtr)); // Load IDT
  __asm__ volatile("sti");                        // Enable interrupts
}

void set_interrupt_gate(
    uint8_t int_vector,
    void *handler_address,
    uint16_t gdt_seg_selector,
    uint8_t privilege)
{
  struct IDTGate *idt_int_gate = &interrupt_descriptor_table.table[int_vector];

  uint32_t handler_addr = (uint32_t)handler_address;

  idt_int_gate->offset_low = handler_addr & 0xFFFF;          // Low 16-bit dari alamat handler
  idt_int_gate->offset_high = (handler_addr >> 16) & 0xFFFF; // High 16-bit dari alamat handler

  idt_int_gate->segment = gdt_seg_selector; // Segment selector dari GDT

  // Set flag dengan format yang benar
  idt_int_gate->gate_32 = 1;                  // 32-bit interrupt gate
  idt_int_gate->present = 1;                  // Harus di-set agar handler aktif
  idt_int_gate->privilege = privilege & 0b11; // Privilege level (0 = kernel, 3 = user)

  // Reserved bits, sesuaikan dengan spesifikasi IDT
  idt_int_gate->_r_bit_1 = INTERRUPT_GATE_R_BIT_1;
  idt_int_gate->_r_bit_2 = INTERRUPT_GATE_R_BIT_2;
  idt_int_gate->_r_bit_3 = INTERRUPT_GATE_R_BIT_3;
}
