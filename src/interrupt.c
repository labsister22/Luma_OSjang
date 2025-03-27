#include "header/cpu/interrupt.h"
#include "header/text/framebuffer.h"
#include "header/cpu/portio.h"

void io_wait(void)
{
  out(0x80, 0);
}

void pic_ack(uint8_t irq)
{
  if (irq >= 8)
    out(PIC2_COMMAND, PIC_ACK);
  out(PIC1_COMMAND, PIC_ACK);
}

void pic_remap(void)
{
  // Starts the initialization sequence in cascade mode
  out(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
  io_wait();
  out(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
  io_wait();
  out(PIC1_DATA, PIC1_OFFSET); // ICW2: Master PIC vector offset
  io_wait();
  out(PIC2_DATA, PIC2_OFFSET); // ICW2: Slave PIC vector offset
  io_wait();
  out(PIC1_DATA, 0b0100); // ICW3: tell Master PIC, slave PIC at IRQ2 (0000 0100)
  io_wait();
  out(PIC2_DATA, 0b0010); // ICW3: tell Slave PIC its cascade identity (0000 0010)
  io_wait();

  out(PIC1_DATA, ICW4_8086);
  io_wait();
  out(PIC2_DATA, ICW4_8086);
  io_wait();

  // Disable all interrupts
  out(PIC1_DATA, PIC_DISABLE_ALL_MASK);
  out(PIC2_DATA, PIC_DISABLE_ALL_MASK);
}

void main_interrupt_handler(struct InterruptFrame frame)
{
  // First acknowledge IRQs immediately if applicable
  if (frame.int_number >= 0x20 && frame.int_number <= 0x2F)
  {
    pic_ack(frame.int_number - 0x20);
  }

  switch (frame.int_number)
  {
  case 0x00: // Divide by Zero
    framebuffer_write(0, 0, 'D', 4, 0);
    framebuffer_write(0, 1, 'Z', 4, 0);
    framebuffer_write(0, 2, 'E', 4, 0);
    framebuffer_write(0, 3, 'R', 4, 0);
    framebuffer_write(0, 4, 'O', 4, 0);
    while (1)
    {
      asm volatile("cli; hlt");
    }
    break;

  case 0x0D: // General Protection Fault
    framebuffer_write(0, 0, 'G', 4, 0);
    framebuffer_write(0, 1, 'P', 4, 0);
    framebuffer_write(0, 2, 'F', 4, 0);
    while (1)
    {
      asm volatile("cli; hlt");
    }
    break;

  case 0x0E: // Page Fault
    framebuffer_write(0, 0, 'P', 4, 0);
    framebuffer_write(0, 1, 'F', 4, 0);
    while (1)
    {
      asm volatile("cli; hlt");
    }
    break;

  case 0x20: // Timer interrupt (IRQ0)
    framebuffer_write(1, 0, '.', 2, 0);
    break;

  case 0x21: // Keyboard interrupt (IRQ1)
    uint8_t scancode = in(0x60);
    if (scancode != 0)
    {
      framebuffer_write(2, 0, 'K', 3, 0);
      framebuffer_write(2, 1, 'B', 3, 0);
    }
    break;

  default:
    // Print debug info for unknown interrupts
    framebuffer_write(3, 0, 'I', 1, 0);
    framebuffer_write(3, 1, 'N', 1, 0);
    framebuffer_write(3, 2, 'T', 1, 0);
    framebuffer_write(3, 3, ':', 1, 0);

    char hex[] = "0123456789ABCDEF";
    framebuffer_write(3, 4, hex[(frame.int_number >> 4) & 0xF], 1, 0);
    framebuffer_write(3, 5, hex[frame.int_number & 0xF], 1, 0);

    // For fatal exceptions, halt
    if (frame.int_number < 0x20)
    {
      while (1)
      {
        asm volatile("cli; hlt");
      }
    }
    break;
  }
}