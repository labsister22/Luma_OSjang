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
    while (1)
      ;
  }
  break;
  case 0x0D:                                // General Protection Fault
    framebuffer_write(6, 0, 'G', 0xF, 0x0); // G for GP Fault
    while (1)
      ;
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
    framebuffer_write(
        frame.cpu.general.edx,            // row
        frame.cpu.general.ecx,            // col
        *((char *)frame.cpu.general.ebx), // character
        0x0F,                             // color (white on black)
        0x00);                            // bg_color
    break;

  case 6: // SYS_PUTS - Print string
  {
    char *str = (char *)frame.cpu.general.ebx;
    uint8_t row = frame.cpu.general.ecx;
    uint8_t col = frame.cpu.general.edx;

    // Simple puts implementation
    uint32_t i = 0;
    uint8_t current_row = row;
    uint8_t current_col = col;
    while (str[i] != '\0' && i < 1000)
    { // Safety limit
      if (str[i] == '\n')
      {
        current_row++;
        current_col = 0;
      }
      else
      {
        framebuffer_write(current_row, current_col, str[i], 0x0F, 0x00);
        current_col++;
        if (current_col >= 80)
        {
          current_col = 0;
          current_row++;
        }
      }
      i++;
      if (current_row >= 25)
        break; // Screen height limit
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
        frame.cpu.general.ecx,  // row
        frame.cpu.general.ebx); // col
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
  case 11: // SYS_EXIT - Process termination
  {
    // Exit code is passed in ebx
    uint32_t exit_code = frame.cpu.general.ebx;
    
    // TODO: Implement proper process termination
    // For now, display exit message and halt
    framebuffer_write(0, 60, 'E', 0xF, 0x0); // E for Exit
    framebuffer_write(0, 61, 'X', 0xF, 0x0); // X
    framebuffer_write(0, 62, 'I', 0xF, 0x0); // I
    framebuffer_write(0, 63, 'T', 0xF, 0x0); // T
    framebuffer_write(0, 64, ':', 0xF, 0x0); // :
    
    // Display exit code (simple single digit for now)
    if (exit_code < 10) {
      framebuffer_write(0, 65, '0' + exit_code, 0xF, 0x0);
    } else {
      framebuffer_write(0, 65, 'X', 0xF, 0x0); // X for non-single digit
    }
    
    // In a real OS, this would:
    // 1. Clean up process resources
    // 2. Update process state
    // 3. Schedule next process
    // 4. Return to scheduler
    
    // For now, just halt the system
    __asm__ volatile("cli; hlt");
  }
  break;
  case 18: // SYS_CHANGE_DIR
{
    uint32_t target_inode = frame.cpu.general.ebx;
    // char* path = (char*)frame.cpu.general.ecx;
    // bool update_display = (bool)frame.cpu.general.edx;
    if (target_inode == 0 || target_inode > 1000) { // Simple bounds check
        frame.cpu.general.eax = 1; // Error - invalid inode
        break;
    }
    // Simple validation - check if inode exists
    struct EXT2Inode inode;
    read_inode(target_inode, &inode);
    
    // Check if it's a directory
    if (!(inode.i_mode & EXT2_S_IFDIR)) {
        frame.cpu.general.eax = 2; // Error - not a directory
        break;
    }
    
    // Success - it's a valid directory
    frame.cpu.general.eax = 0; // Success
}
  break;
  case 22: // SYS_LIST_DIR
{
    // Dapatkan ALAMAT pointer dari userspace yang dikirim melalui ebx
    uint32_t user_row_ptr_addr = frame.cpu.general.ebx; // Ini adalah ALAMAT VIRTUAL USERSAPCE

    // Validasi alamat userspace (PENTING UNTUK KEAMANAN DAN STABILITAS)
    // Jika user_row_ptr_addr tidak berada di segmen data userspace yang valid,
    // maka akses langsung akan menyebabkan fault.
    // Misal: if (user_row_ptr_addr < USER_CODE_START || user_row_ptr_addr + sizeof(uint32_t) > USER_DATA_END) { /* handle error */ }
    // Untuk saat ini, kita asumsikan image_storage adalah representasi flat memory.
    // Jika tidak, Anda perlu mekanisme MMU atau fungsi copy_from_user/copy_to_user.
    
    // Baca nilai awal baris dari userspace melalui pointer tersebut.
    // ASUMSI KRITIS: user_row_ptr_addr adalah offset yang valid dalam image_storage
    uint32_t initial_user_row_value = *(uint32_t*)(user_row_ptr_addr);
    uint8_t current_print_row = (uint8_t)initial_user_row_value; // Casting ke uint8_t jika baris tidak akan melebihi 255

    uint32_t dir_inode_idx = frame.cpu.general.ecx;

    // --- HAPUS PENCETAKAN HEADER DI SINI ---
    // Karena header sudah dicetak oleh handle_ls di userspace
    
    char header1[] = "name                type   size";
    for (int k = 0; header1[k] != '\0'; k++) {
        framebuffer_write(current_print_row, k, header1[k], 0x0F, 0x00);
    }
    current_print_row++;
    if ((current_print_row) >= 24) {
        frame.cpu.general.eax = current_print_row;
        goto end_list_dir_syscall_modified;
    }
    char header2[] = "================================";
    for (int k = 0; header2[k] != '\0' && k < 32; k++) {
        framebuffer_write(current_print_row, k, header2[k], 0x0F, 0x00);
    }
    current_print_row++;
    if (current_print_row >= 24) {
        frame.cpu.general.eax = current_print_row;
        goto end_list_dir_syscall_modified;
    }
    

    // Lanjutkan dengan membaca direktori dan mencetak entri
    struct EXT2Inode dir_inode;
    read_inode(dir_inode_idx, &dir_inode);

    if (!(dir_inode.i_mode & EXT2_S_IFDIR)) {
        frame.cpu.general.eax = current_print_row;
        break; // break, bukan goto, jika ini adalah error yang dihandle
    }

    uint8_t block_buffer[BLOCK_SIZE];
    for (uint32_t i = 0; i < dir_inode.i_blocks && i < 12; i++) {
        if (dir_inode.i_block[i] == 0) continue;

        read_blocks(block_buffer, dir_inode.i_block[i], 1);

        uint32_t offset = 0;
        while (offset < BLOCK_SIZE) {
            struct EXT2DirectoryEntry *entry = (struct EXT2DirectoryEntry *)(block_buffer + offset);

            if (entry->inode == 0 || entry->rec_len == 0) {
                break;
            }
            if (offset + entry->rec_len > BLOCK_SIZE) {
                 break;
            }

            // Print entry name, type, and size
            const uint8_t col_name = 0; // Tetapkan kembali jika perlu
            const uint8_t col_type = 20;
            const uint8_t col_size = 27;

            char name_char;
            const char* entry_name = (const char*)(entry + 1);
            for(uint8_t k=0; k < entry->name_len && k < (col_type - col_name -1) ; k++) {
                 name_char = entry_name[k];
                 if (name_char >= 32 && name_char <= 126) {
                    framebuffer_write(current_print_row, col_name + k, name_char, 0x0F, 0x00);
                 } else {
                    framebuffer_write(current_print_row, col_name + k, '?', 0x0F, 0x00);
                 }
            }

            const char *type_str = "unk";
            if (entry->file_type == 2) {
                type_str = "dir";
            } else if (entry->file_type == 1) {
                type_str = "file";
            }
            for(int k=0; type_str[k] != '\0' && k < 4; k++) {
                 framebuffer_write(current_print_row, col_type + k, type_str[k], 0x0F, 0x00);
            }

            char size_placeholder[] = "0";
            for(int k=0; size_placeholder[k] != '\0'; k++) {
                 framebuffer_write(current_print_row, col_size + k, size_placeholder[k], 0x0F, 0x00);
            }

            current_print_row++; // Kenaikan baris untuk setiap entri
            if (current_print_row >= 24) {
                goto end_list_dir_syscall_modified;
            }

            offset += entry->rec_len;
        }
    }

    // Tulis kembali nilai `current_print_row` yang sudah diupdate ke alamat userspace
    // PENTING: Gunakan user_row_ptr_addr sebagai offset dari image_storage.
    *(uint32_t*)(user_row_ptr_addr) = current_print_row;

    end_list_dir_syscall_modified:;
    framebuffer_set_cursor(current_print_row, 0);

    // CRITICAL FIX: Update the user space pointer AND return value consistently
    *(uint32_t*)(user_row_ptr_addr) = current_print_row;
    frame.cpu.general.eax = current_print_row;
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
