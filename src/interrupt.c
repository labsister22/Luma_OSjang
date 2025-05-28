#include "header/cpu/interrupt.h"
#include "header/cpu/portio.h"
#include "header/driver/keyboard.h"
#include "header/cpu/gdt.h"
#include "header/filesystem/test_ext2.h"
#include "header/filesystem/ext2.h"
#include "header/text/framebuffer.h"
#include "header/driver/cmos.h"
#include "header/driver/vga_graphics.h"

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

// SAFE VGA write for error messages - NO FUNCTION CALLS
static void safe_error_write(char c, int pos) {
    volatile uint16_t* vga = (volatile uint16_t*)0xB8000;
    if (pos >= 0 && pos < 80 * 25) {
        vga[pos] = (uint16_t)c | (0x4F << 8); // White on red background
    }
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
    keyboard_isr();
    break;
  case 0x30:
    syscall(frame);
    break;
  case 0x0E: // Page Fault - PERBAIKAN DI SINI
  {
    uint32_t cr2;
    __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
    
    // SAFE error display - direct VGA write tanpa function calls
    safe_error_write('P', 0);
    safe_error_write('A', 1);
    safe_error_write('G', 2);
    safe_error_write('E', 3);
    safe_error_write(' ', 4);
    safe_error_write('F', 5);
    safe_error_write('A', 6);
    safe_error_write('U', 7);
    safe_error_write('L', 8);
    safe_error_write('T', 9);
    safe_error_write(':', 10);
    
    // Convert cr2 to hex and display
    char hex_chars[] = "0123456789ABCDEF";
    for (int i = 0; i < 8; i++) {
        int nibble = (cr2 >> (28 - i * 4)) & 0xF;
        safe_error_write(hex_chars[nibble], 12 + i);
    }
    
    while (1); // Halt
  }
  break;
  case 0x0D: // General Protection Fault - PERBAIKAN DI SINI
    safe_error_write('G', 0);
    safe_error_write('P', 1);
    safe_error_write(' ', 2);
    safe_error_write('F', 3);
    safe_error_write('A', 4);
    safe_error_write('U', 5);
    safe_error_write('L', 6);
    safe_error_write('T', 7);
    while (1);
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

struct rtc_time
{
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
    {
        char char_to_print = *((char *)frame.cpu.general.ebx);
        int col = (int)frame.cpu.general.ecx;
        int row = (int)frame.cpu.general.edx;

        // Bounds check BEFORE calling VGA functions
        if (row >= 0 && row < TEXT_ROWS && col >= 0 && col < TEXT_COLS) {
            vga_draw_char_kernel(char_to_print, row, col, 15, 0);
        }
    }
    break;
    
  case 6: // SYS_PUTS - Print string
  {
    char *str = (char *)frame.cpu.general.ebx;
    int current_col = (int)frame.cpu.general.edx; // col
    int current_row = (int)frame.cpu.general.ecx; // row

    // Bounds check
    if (current_row < 0 || current_row >= TEXT_ROWS) current_row = 0;
    if (current_col < 0 || current_col >= TEXT_COLS) current_col = 0;

    uint32_t i = 0;
    while (str && str[i] != '\0' && i < 1024 && current_row < TEXT_ROWS)
    {
      if (str[i] == '\n')
      {
        current_row++;
        current_col = 0;
      }
      else
      {
        if (current_col >= TEXT_COLS)
        {
          current_col = 0;
          current_row++;
        }
        
        if (current_row < TEXT_ROWS && current_col < TEXT_COLS) {
            vga_draw_char_kernel(str[i], current_row, current_col, 15, 0);
            current_col++;
        }
      }
      i++;
    }
  }
  break;
  
  case 7: // SYS_KEYBOARD_ACTIVATE - Activate keyboard
    keyboard_state_activate();
    break;

  case 8: // SYS_CLEAR_SCREEN - Clear screen
    vga_clear_screen_kernel();
    break;

  case 9: // SYS_SET_CURSOR - Set cursor position
    {
        int col = (int)frame.cpu.general.ebx;
        int row = (int)frame.cpu.general.ecx;
        
        // Bounds check
        if (row >= 0 && row < TEXT_ROWS && col >= 0 && col < TEXT_COLS) {
            vga_set_cursor_kernel(col, row);
        }
    }
    break;
    
  case 10: // SYS_GET_TIME
      {
          struct Time t_cmos = get_cmos_time();
          struct rtc_time* out_time_ptr = (struct rtc_time*) frame.cpu.general.ebx;
          if (out_time_ptr) {
              out_time_ptr->hour = t_cmos.hour;
              out_time_ptr->minute = t_cmos.minute;
              out_time_ptr->second = t_cmos.second;
          }
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

void activate_timer_interrupt(void)
{
  __asm__ volatile("cli");
  // Setup how often PIT fire
  uint32_t pit_timer_counter_to_fire = PIT_TIMER_COUNTER;
  out(PIT_COMMAND_REGISTER_PIO, PIT_COMMAND_VALUE);
  out(PIT_CHANNEL_0_DATA_PIO, (uint8_t)(pit_timer_counter_to_fire & 0xFF));
  out(PIT_CHANNEL_0_DATA_PIO, (uint8_t)((pit_timer_counter_to_fire >> 8) & 0xFF));

  // Activate the interrupt
  out(PIC1_DATA, in(PIC1_DATA) & ~(1 << IRQ_TIMER));
}