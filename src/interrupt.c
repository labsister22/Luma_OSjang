// src/interrupt.c

#include "header/cpu/interrupt.h"
#include "header/cpu/portio.h"
#include "header/driver/keyboard.h"
#include "header/cpu/gdt.h"
#include "header/filesystem/test_ext2.h"
#include "header/filesystem/ext2.h" // Perlu ini untuk EXT2DriverRequest dan fungsi EXT2
#include "header/text/framebuffer.h"
#include "header/driver/cmos.h"
#include "header/stdlib/string.h" // Perlu untuk memcpy, strlen, dll.
#include "header/process/process.h" // Untuk process_destroy

char kernel_current_working_directory_str[32] = "/"; 

void kernel_handle_cd(const char* path, char* output_buf, uint32_t buf_size) {
    if (!path || strlen(path) == 0) {
        strncpy(output_buf, "cd:no arg", buf_size - 1);
        output_buf[buf_size - 1] = '\0';
        return;
    }

    // Implementasi dummy untuk cd:
    // Kernel harus benar-benar mengubah CWD-nya dan memvalidasi path EXT2.
    // Untuk demo, kita asumsikan jika path adalah "/", berhasil.
    if (strcmp(path, "/") == 0) {
        strcpy(kernel_current_working_directory_str, "/");
        strncpy(output_buf, "OK:CD /", buf_size - 1);
    } else {
        // Implementasi sebenarnya akan mencari inode path dan memvalidasi itu direktori
        // Misalnya, cari di root_inode
        struct EXT2Inode root_inode;
        read_inode(2, &root_inode);
        uint32_t target_inode_idx;
        if (find_inode_in_dir(&root_inode, path, &target_inode_idx)) {
            struct EXT2Inode target_inode;
            read_inode(target_inode_idx, &target_inode);
            if (is_directory(&target_inode)) {
                // Kernel Anda harus melacak CWD sebagai inode ID
                // Ini hanya untuk tampilan, kernel tidak akan memvalidasi string path
                strcpy(kernel_current_working_directory_str, path);
                strncpy(output_buf, "OK:CD ", buf_size - 1);
                strncat(output_buf, path, buf_size - strlen(output_buf) -1);
            } else {
                strncpy(output_buf, "ERR:Not dir", buf_size - 1);
            }
        } else {
            strncpy(output_buf, "ERR:Path not found", buf_size - 1);
        }
    }
    output_buf[buf_size - 1] = '\0'; // Pastikan null-terminated
}

void kernel_handle_ls(const char* path, char* output_buf, uint32_t buf_size) {
    // path argumen opsional, jika kosong atau ".", list CWD kernel
    uint32_t target_inode = 2; // Default ke root untuk listing
    if (path && strlen(path) > 0 && strcmp(path, ".") != 0) {
        // Kernel harus resolve path ini ke inode ID
        // Untuk demo, kita abaikan path dan selalu list root
        strncpy(output_buf, "LS:Only root for now", buf_size - 1);
        output_buf[buf_size - 1] = '\0';
        return;
    }
    
    struct EXT2Inode parent_inode;
    read_inode(target_inode, &parent_inode);

    uint8_t dir_buf[BLOCK_SIZE];
    read_blocks(dir_buf, parent_inode.i_block[0], 1);

    uint32_t offset = 0;
    char *current_output_pos = output_buf;
    uint32_t bytes_written = 0;

    strncpy(current_output_pos, "LS:", buf_size -1);
    bytes_written += strlen("LS:");
    current_output_pos += strlen("LS:");


    while (offset < BLOCK_SIZE && bytes_written < buf_size - 1) {
        struct EXT2DirectoryEntry *entry = get_directory_entry(dir_buf, offset);
        if (entry->inode == 0) {
            break;
        }

        if (strcmp(get_entry_name(entry), ".") != 0 && strcmp(get_entry_name(entry), "..") != 0) {
            if (bytes_written + entry->name_len + 2 < buf_size) { // +2 for space and null
                strncat(current_output_pos, " ", 1); // Tambahkan spasi
                current_output_pos++;
                bytes_written++;
                strncpy(current_output_pos, get_entry_name(entry), entry->name_len);
                current_output_pos += entry->name_len;
                bytes_written += entry->name_len;
            } else {
                break; // Buffer penuh
            }
        }
        offset += entry->rec_len;
    }
    output_buf[buf_size - 1] = '\0'; // Pastikan null-terminated
}

void kernel_handle_mkdir(const char* name, char* output_buf, uint32_t buf_size) {
    if (!name || strlen(name) == 0) {
        strncpy(output_buf, "mkd:no arg", buf_size - 1);
        output_buf[buf_size - 1] = '\0';
        return;
    }
    struct EXT2DriverRequest req;
    memset(&req, 0, sizeof(req));
    req.parent_inode = 2; // Default root
    strncpy(req.name, name, sizeof(req.name) - 1);
    req.name[sizeof(req.name) - 1] = '\0';
    req.name_len = strlen(req.name);
    req.is_directory = true;

    int8_t status = write(req); // Fungsi write dari ext2.c
    if (status == 0) {
        strncpy(output_buf, "OK:MKD", buf_size - 1);
    } else {
        strncpy(output_buf, "ERR:MKD", buf_size - 1);
    }
    output_buf[buf_size - 1] = '\0';
}

void kernel_handle_cat(const char* filename, char* output_buf, uint32_t buf_size) {
    if (!filename || strlen(filename) == 0) {
        strncpy(output_buf, "cat:no arg", buf_size - 1);
        output_buf[buf_size - 1] = '\0';
        return;
    }
    struct EXT2DriverRequest req;
    memset(&req, 0, sizeof(req));
    req.parent_inode = 2; // Default root
    strncpy(req.name, filename, sizeof(req.name) - 1);
    req.name[sizeof(req.name) - 1] = '\0';
    req.name_len = strlen(req.name);
    req.buf = (uint8_t*)output_buf + 1; // Ruang output untuk file content, +1 untuk status byte
    req.buffer_size = buf_size - 1; // Ukuran buffer tersedia untuk konten file

    int8_t status = read(req); // Fungsi read dari ext2.c
    output_buf[0] = (char)status; // Simpan status di byte pertama buffer output

    if (status != 0) {
        strncpy(output_buf + 1, "ERR:CAT", buf_size - 2); // Pesan error jika gagal
    } else {
        // Output_buf+1 sudah berisi konten file jika berhasil
    }
    output_buf[buf_size - 1] = '\0';
}

void kernel_handle_cp(const char* source, const char* destination, char* output_buf, uint32_t buf_size) {
    if (!source || !destination || strlen(source) == 0 || strlen(destination) == 0) {
        strncpy(output_buf, "cp:args", buf_size - 1);
        output_buf[buf_size - 1] = '\0';
        return;
    }

    // Baca file sumber ke buffer sementara di kernel
    static uint8_t temp_file_buffer[BLOCK_SIZE * 2]; // Kernel-side buffer, bisa lebih besar dari userspace
    struct EXT2DriverRequest read_req;
    memset(&read_req, 0, sizeof(read_req));
    read_req.parent_inode = 2; // Default root
    strncpy(read_req.name, source, sizeof(read_req.name) - 1);
    read_req.name[sizeof(read_req.name) - 1] = '\0';
    read_req.name_len = strlen(read_req.name);
    read_req.buf = temp_file_buffer;
    read_req.buffer_size = sizeof(temp_file_buffer);

    int8_t read_status = read(read_req);
    if (read_status != 0) {
        strncpy(output_buf, "ERR:CP Rd", buf_size - 1);
        output_buf[buf_size - 1] = '\0';
        return;
    }

    // Tulis ke file tujuan
    struct EXT2DriverRequest write_req;
    memset(&write_req, 0, sizeof(write_req));
    write_req.parent_inode = 2; // Default root
    strncpy(write_req.name, destination, sizeof(write_req.name) - 1);
    write_req.name[sizeof(write_req.name) - 1] = '\0';
    write_req.name_len = strlen(write_req.name);
    write_req.buf = temp_file_buffer;
    write_req.buffer_size = read_req.buffer_size; // Ukuran aktual yang dibaca

    int8_t write_status = write(write_req);
    if (write_status != 0) {
        strncpy(output_buf, "ERR:CP Wr", buf_size - 1);
        output_buf[buf_size - 1] = '\0';
        return;
    }
    strncpy(output_buf, "OK:CP", buf_size - 1);
    output_buf[buf_size - 1] = '\0';
}

void kernel_handle_rm(const char* path, char* output_buf, uint32_t buf_size) {
    if (!path || strlen(path) == 0) {
        strncpy(output_buf, "rm:no arg", buf_size - 1);
        output_buf[buf_size - 1] = '\0';
        return;
    }
    struct EXT2DriverRequest req;
    memset(&req, 0, sizeof(req));
    req.parent_inode = 2; // Default root
    strncpy(req.name, path, sizeof(req.name) - 1);
    req.name[sizeof(req.name) - 1] = '\0';
    req.name_len = strlen(req.name);
    req.is_directory = false; // Asumsi file, akan di-check di delete()

    int8_t status = delete(req); // Fungsi delete dari ext2.c
    if (status == 0) {
        strncpy(output_buf, "OK:RM", buf_size - 1);
    } else {
        strncpy(output_buf, "ERR:RM", buf_size - 1);
    }
    output_buf[buf_size - 1] = '\0';
}

void kernel_handle_mv(const char* source, const char* destination, char* output_buf, uint32_t buf_size) {
    // MV = CP + RM (di kernel)
    char temp_output[64]; // Buffer sementara untuk output CP/RM
    kernel_handle_cp(source, destination, temp_output, sizeof(temp_output));
    if (temp_output[0] != 'O' || temp_output[1] != 'K') { // Jika CP gagal
        strncpy(output_buf, "ERR:MV CP", buf_size - 1);
        output_buf[buf_size - 1] = '\0';
        return;
    }
    kernel_handle_rm(source, temp_output, sizeof(temp_output));
    if (temp_output[0] != 'O' || temp_output[1] != 'K') { // Jika RM gagal
        strncpy(output_buf, "ERR:MV RM", buf_size - 1);
        output_buf[buf_size - 1] = '\0';
        return;
    }
    strncpy(output_buf, "OK:MV", buf_size - 1);
    output_buf[buf_size - 1] = '\0';
}

void kernel_handle_find(const char* name, char* output_buf, uint32_t buf_size) {
    (void) name;
    strncpy(output_buf, "FIND:stub", buf_size - 1);
    output_buf[buf_size - 1] = '\0';
}

// --- Parser Perintah Kernel ---
// Fungsi ini akan melakukan parsing string perintah lengkap
// dan memanggil handler kernel yang sesuai.
void kernel_parse_and_execute_command(const char* command_str, char* output_buf, uint32_t buf_size) {
  #define KERNEL_CMD_BUF_SIZE 64  
  char cmd_copy[KERNEL_CMD_BUF_SIZE]; // Pastikan KERNEL_CMD_BUF_SIZE cukup besar
    strncpy(cmd_copy, command_str, KERNEL_CMD_BUF_SIZE - 1);
    cmd_copy[KERNEL_CMD_BUF_SIZE - 1] = '\0';

    char *command_name = cmd_copy;
    char *arg1 = NULL;
    char *arg2 = NULL;

    char* first_space = strchr(cmd_copy, ' ');
    if (first_space != NULL) {
        *first_space = '\0';
        arg1 = first_space + 1;
        while (*arg1 == ' ' && *arg1 != '\0') { arg1++; } // Skip leading spaces

        if (*arg1 == '\0') { arg1 = NULL; }
        else {
            char* second_space = strchr(arg1, ' ');
            if (second_space != NULL) {
                *second_space = '\0';
                arg2 = second_space + 1;
                while (*arg2 == ' ' && *arg2 != '\0') { arg2++; }
                if (*arg2 == '\0') { arg2 = NULL; }
            }
        }
    }

    // Perintah internal kernel (misalnya, exit)
    if (strcmp(command_name, "exit") == 0) {
        process_destroy(process_get_current_running_pcb_pointer()->metadata.pid); // Asumsi PID 0 atau yang benar
        strncpy(output_buf, "OK:Bye", buf_size - 1);
    }
    // Perintah shell yang diimplementasikan di kernel
    else if (strcmp(command_name, "cd") == 0) {
        kernel_handle_cd(arg1, output_buf, buf_size);
    } else if (strcmp(command_name, "ls") == 0) {
        kernel_handle_ls(arg1, output_buf, buf_size);
    } else if (strcmp(command_name, "mkdir") == 0) {
        kernel_handle_mkdir(arg1, output_buf, buf_size);
    } else if (strcmp(command_name, "cat") == 0) {
        kernel_handle_cat(arg1, output_buf, buf_size);
    } else if (strcmp(command_name, "cp") == 0) {
        kernel_handle_cp(arg1, arg2, output_buf, buf_size);
    } else if (strcmp(command_name, "rm") == 0) {
        kernel_handle_rm(arg1, output_buf, buf_size);
    } else if (strcmp(command_name, "mv") == 0) {
        kernel_handle_mv(arg1, arg2, output_buf, buf_size);
    } else if (strcmp(command_name, "find") == 0) {
        kernel_handle_find(arg1, output_buf, buf_size);
    } else {
        strncpy(output_buf, "ERR:Bad cmd", buf_size - 1);
    }
    output_buf[buf_size - 1] = '\0';
}

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
  case 0x30: // Syscall interrupt
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

extern struct Time get_cmos_time(); // Deklarasi dari cmos.c

void syscall(struct InterruptFrame frame)
{
  switch (frame.cpu.general.eax)
  {
  case 0: // SYS_READ - File system read
    *((int8_t *)frame.cpu.general.ecx) = read(
        *(struct EXT2DriverRequest *)frame.cpu.general.ebx);
    break;

  case 1: // SYS_WRITE - File system write
    *((int8_t *)frame.cpu.general.ecx) = write(
        *(struct EXT2DriverRequest *)frame.cpu.general.ebx);
    break;

  case 2: // SYS_DELETE - File system delete
    *((int8_t *)frame.cpu.general.ecx) = delete(
        *(struct EXT2DriverRequest *)frame.cpu.general.ebx);
    break;

  case 3: // SYS_LS_DIR - List directory
          // ebx: pointer ke EXT2DriverRequest (hanya parent_inode yang digunakan)
          // ecx: pointer ke buffer untuk menyimpan nama file
          // edx: pointer ke uint32_t untuk menyimpan jumlah file
  {
    struct EXT2DriverRequest *req = (struct EXT2DriverRequest *)frame.cpu.general.ebx;
    char *output_buffer = (char *)frame.cpu.general.ecx; // Asumsi buffer cukup besar
    uint32_t *file_count = (uint32_t *)frame.cpu.general.edx;

    *file_count = 0; // Inisialisasi jumlah file
    if (!req || !output_buffer || !file_count) {
        // Handle error: Invalid arguments
        *((int8_t *)frame.cpu.general.ecx) = -1; // Return error code
        return;
    }

    struct EXT2Inode parent_inode;
    uint32_t parent_inode_idx;
    // Menggunakan find_dir untuk mencari inode direktori
    // `find_dir` mengembalikan bool, jadi kita perlu ubah return statusnya
    bool dir_found = find_dir(req->parent_inode, &parent_inode_idx);
    if (dir_found) {
        read_inode(parent_inode_idx, &parent_inode); // Baca parent inode

        // Iterasi melalui entri direktori
        uint8_t dir_buf[BLOCK_SIZE];
        read_blocks(dir_buf, parent_inode.i_block[0], 1); // Asumsi hanya 1 blok untuk direktori

        uint32_t offset = 0;
        char *current_output_pos = output_buffer;

        while (offset < BLOCK_SIZE) {
            struct EXT2DirectoryEntry *entry = get_directory_entry(dir_buf, offset);
            if (entry->inode == 0) { // End of valid entries
                break;
            }

            // Abaikan . dan ..
            if (strcmp(get_entry_name(entry), ".") != 0 && strcmp(get_entry_name(entry), "..") != 0) {
                // Salin nama file ke buffer output
                // Pastikan output_buffer cukup besar
                memcpy(current_output_pos, get_entry_name(entry), entry->name_len);
                current_output_pos[entry->name_len] = '\0'; // Null-terminate
                current_output_pos += (entry->name_len + 1); // Pindah ke posisi berikutnya (tambah 1 untuk null terminator)
                (*file_count)++;
            }
            offset += entry->rec_len;
        }
        *((int8_t *)frame.cpu.general.ecx) = 0; // Return success
    } else {
        *((int8_t *)frame.cpu.general.ecx) = -2; // Return error code (e.g., parent not found)
    }
  }
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
    uint8_t col = frame.cpu.general.edx;
    uint8_t row = frame.cpu.general.ecx;

    // Simple puts implementation
    uint32_t i = 0;
    uint8_t current_col = col;
    uint8_t current_row = row;
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

  case 9: // SYS_SET_CURSOR - Set cursor position
    framebuffer_set_cursor(
        frame.cpu.general.ecx,  // row
        frame.cpu.general.ebx); // col
    break;
  case 10: // SYS_GET_TIME
      {
          struct Time t = get_cmos_time();
          struct Time* out = (struct Time*) frame.cpu.general.ebx;
          out->hour = t.hour;
          out->minute = t.minute;
          out->second = t.second;
      }
      break;

  case 11: // SYS_EXIT - Exit process
    process_destroy(frame.cpu.general.ebx); // ebx = PID to destroy
    break;

  case 12: // SYS_CD - Change Directory
            // ebx: pointer ke path baru
            // ecx: pointer ke buffer untuk menyimpan CWD baru dari kernel (optional)
            // edx: pointer ke int8_t untuk status return
  {
    char *path_to_change = (char *)frame.cpu.general.ebx;
    char *out_cwd_buffer = (char *)frame.cpu.general.ecx; // Optional output buffer for new CWD
    int8_t *status_ptr = (int8_t *)frame.cpu.general.edx;

    if (!path_to_change || !status_ptr) {
        *status_ptr = -1; // Invalid arguments
        return;
    }

    // TODO: Implement actual kernel-level change directory logic here.
    // This involves searching the EXT2 file system for the path,
    // validating it's a directory, and updating the current process's CWD
    // (if your kernel supports per-process CWD).
    // For now, let's use a very simplified placeholder based on your EXT2 functions.

    uint32_t target_inode_idx;
    // Assume current CWD for the process is tracked by an inode number.
    // For simplicity, let's assume `cd` always operates relative to root (inode 2)
    // or provides an absolute path.
    // A more robust solution needs kernel to track process's current_inode.

    // If path is "/", set CWD to root (inode 2)
    if (strcmp(path_to_change, "/") == 0) {
        // Here, you would update the *process's* internal CWD inode to 2.
        // This kernel doesn't seem to have a per-process CWD inode yet.
        // For now, we just indicate success if the path is "/".
        *status_ptr = 0;
        if (out_cwd_buffer) {
            strcpy(out_cwd_buffer, "/");
        }
    } else {
        // Try to find the directory within root (inode 2)
        // This is a simplification; `cd` should handle relative paths from *current* CWD.
        struct EXT2Inode root_inode;
        read_inode(2, &root_inode); // Read root inode

        if (find_inode_in_dir(&root_inode, path_to_change, &target_inode_idx)) {
            struct EXT2Inode target_inode;
            read_inode(target_inode_idx, &target_inode);
            if (is_directory(&target_inode)) {
                // If it's a directory, update the process's CWD inode (placeholder)
                // And return success.
                *status_ptr = 0;
                if (out_cwd_buffer) {
                    // This is only for display, not actual kernel CWD tracking
                    strcpy(out_cwd_buffer, path_to_change);
                }
            } else {
                *status_ptr = -3; // Not a directory
            }
        } else {
            *status_ptr = -2; // Path not found
        }
    }
  }
  break;

  case 13: // SYS_GET_CWD - Get Current Working Directory
            // ebx: pointer ke buffer untuk menyimpan CWD
            // ecx: ukuran buffer
            // edx: pointer ke int8_t untuk status return
  {
    char *buffer = (char *)frame.cpu.general.ebx;
    uint32_t buffer_size = frame.cpu.general.ecx;
    int8_t *status_ptr = (int8_t *)frame.cpu.general.edx;

    if (!buffer || buffer_size == 0 || !status_ptr) {
        *status_ptr = -1; // Invalid arguments
        return;
    }

    // TODO: Implement actual kernel-level CWD retrieval here.
    // This involves fetching the current process's CWD from kernel data structures.
    // For now, return a dummy CWD or the last successfully "cd-ed" path in kernel context.
    // Since the kernel doesn't explicitly track CWD per process yet, this is a placeholder.
    if (buffer_size >= 2) { // At least room for "/" and null terminator
        strcpy(buffer, "/"); // Dummy CWD: Always root
        *status_ptr = 0; // Success
    } else {
        *status_ptr = -2; // Buffer too small
    }
  }
  break;

  case 14: // SYS_EXEC_CMD - Syscall baru untuk eksekusi perintah shell
  {
      const char* command_str = (const char*)frame.cpu.general.ebx;
      char* output_buf = (char*)frame.cpu.general.ecx;
      uint32_t buf_size = frame.cpu.general.edx;

      kernel_parse_and_execute_command(command_str, output_buf + 1, buf_size - 1); // +1 untuk status byte
      // Simpan status di byte pertama buffer output
      output_buf[0] = (char)0; // Default OK, diubah oleh fungsi handler jika ada error

      // Jika perintah adalah "exit", kernel akan mematikan proses setelah mengirim output
      if (strcmp(command_str, "exit") == 0) {
          process_destroy(process_get_current_running_pcb_pointer()->metadata.pid);
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