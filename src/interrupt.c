#include "header/cpu/interrupt.h"
#include "header/cpu/portio.h"
#include "header/driver/keyboard.h"
#include "header/cpu/gdt.h"
#include "header/filesystem/test_ext2.h"
#include "header/filesystem/ext2.h"
#include "header/text/framebuffer.h"
#include "header/driver/cmos.h"

struct TSSEntry _interrupt_tss_entry = {
    .ss0 = GDT_KERNEL_DATA_SEGMENT_SELECTOR,
};

void set_tss_kernel_current_stack(void)
{
  uint32_t stack_ptr;
  // Reading base stack frame instead esp
  __asm__ volatile("mov %%ebp, %0" : "=r"(stack_ptr) : /* <Empty> */);
  // Add 8 because 4 for ret address and other 4 is for stack_ptr variable
  _interrupt_tss_entry.esp0 = stack_ptr + 8;
}

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
  
  uint32_t int_num = frame.int_number;

  switch (int_num)
  {
  case 0x00:
    break;
  case 0x21:
    // Keyboard interrupt (IRQ1)
    // ACK keyboard interrupt (IRQ1)
    keyboard_isr();
    // pic_ack(IRQ_KEYBOARD);
    break;
  case 0x30:
    syscall(frame);
    break;
  case 0x0E: // Page Fault
    {
        uint32_t cr2;
        __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
        framebuffer_write(5, 0, 'F', 0xF, 0x0); // F for Fault
        framebuffer_write(6, 0, ((cr2 >> 24) & 0xF) + '0', 0xF, 0x0);
        framebuffer_write(7, 0, ((cr2 >> 20) & 0xF) + '0', 0xF, 0x0);
        framebuffer_write(8, 0, ((cr2 >> 16) & 0xF) + '0', 0xF, 0x0);
        framebuffer_write(9, 0, ((cr2 >> 12) & 0xF) + '0', 0xF, 0x0);
        framebuffer_write(10, 0, ((cr2 >> 8) & 0xF) + '0', 0xF, 0x0);
        framebuffer_write(11, 0, ((cr2 >> 4) & 0xF) + '0', 0xF, 0x0);
        framebuffer_write(12, 0, (cr2 & 0xF) + '0', 0xF, 0x0);
        // Print cr2, eip, error code, dsb
        while(1);
    }
    break;
  case 0x0D: // General Protection Fault
    framebuffer_write(6, 0, 'G', 0xF, 0x0); // G for GP Fault
    while(1);
    break;

  default:
    break;
  }

  // Send End of Interrupt (EOI) untuk IRQ >= 0x20
  if (int_num >= PIC1_OFFSET && int_num <= PIC2_OFFSET + 7)
  {
    pic_ack(int_num - PIC1_OFFSET);
  }
}

void activate_keyboard_interrupt(void)
{
  out(PIC1_DATA, in(PIC1_DATA) & ~(1 << IRQ_KEYBOARD));
}

struct rtc_time {
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
};

struct Time get_cmos_time();

void syscall(struct InterruptFrame frame)
{
  switch (frame.cpu.general.eax)
  {
  case 0: // SYS_READ - File system read
    *((int8_t *)frame.cpu.general.ecx) = read(
        *(struct EXT2DriverRequest *)frame.cpu.general.ebx);
    break;

  case 4: // SYS_KEYBOARD_READ - Read keyboard input
    get_keyboard_buffer((char *)frame.cpu.general.ebx);
    break;

  case 5: // SYS_PUTCHAR - Print single character
    framebuffer_write(
        frame.cpu.general.edx,     // row
        frame.cpu.general.ecx,     // col
        *((char *)frame.cpu.general.ebx), // character
        0x0F,                      // color (white on black)
        0x00);                     // bg_color
    break;

  case 6: // SYS_PUTS - Print string
    {
      char *str = (char *)frame.cpu.general.ebx;
      uint8_t col = frame.cpu.general.edx;
      uint8_t row = frame.cpu.general.ecx;
      
      // Simple puts implementation
      uint32_t i = 0;
      uint8_t current_col = col;
      uint8_t current_row = row;
      while (str[i] != '\0' && i < 1000) { // Safety limit
        if (str[i] == '\n') {
          current_row++;
          current_col = 0;
        } else {
          framebuffer_write(current_row, current_col, str[i], 0x0F, 0x00);
          current_col++;
          if (current_col >= 80) {
            current_col = 0;
            current_row++;
          }
        }
        i++;
        if (current_row >= 25) break; // Screen height limit
      }
      framebuffer_set_cursor(current_row, current_col); // Update cursor position
    }
    break;

  case 7: // SYS_KEYBOARD_ACTIVATE - Activate keyboard
    keyboard_state_activate();
    break;

  case 8: // SYS_CLEAR_SCREEN - Clear screen
    framebuffer_clear();
    framebuffer_set_cursor(0, 0);
    break;

  case 9: // SYS_SET_CURSOR - Set cursor position (new)
    framebuffer_set_cursor(
        frame.cpu.general.ecx,     // row
        frame.cpu.general.ebx);    // col
    break;
  case 10:
    {
      struct Time t = get_cmos_time();
      struct Time* out = (struct Time*) frame.cpu.general.ebx;
      out->hour = t.hour;
      out->minute = t.minute;
      out->second = t.second;
    }
    break;
  case 11: // SYS_COPY_FILE - Copy file
    {
      char *source = (char *)frame.cpu.general.ebx;
      char *dest = (char *)frame.cpu.general.ecx;
      uint32_t parent_inode = frame.cpu.general.edx;
      
      // Return result via eax register
      int8_t result = copy_file(source, dest, parent_inode);
      frame.cpu.general.eax = (uint32_t)result;
    }
    break;

  case 12: // SYS_COPY_DIR - Copy directory
    {
      char *source = (char *)frame.cpu.general.ebx;
      char *dest = (char *)frame.cpu.general.ecx;
      uint32_t parent_inode = frame.cpu.general.edx;
      
      // Return result via eax register
      int8_t result = copy_directory(source, dest, parent_inode);
      frame.cpu.general.eax = (uint32_t)result;
    }
    break;

  default:
    // Unknown system call
    break;
  }
}

void isr_handler(struct InterruptFrame frame)
{
  // Simple handler - just return
  (void)frame; // Suppress unused parameter warning
}