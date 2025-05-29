#include "header/cpu/interrupt.h"
#include "header/cpu/portio.h"
#include "header/driver/keyboard.h"
#include "header/cpu/gdt.h"
#include "header/filesystem/test_ext2.h"
#include "header/filesystem/ext2.h"
#include "header/text/framebuffer.h"
#include "header/driver/cmos.h"
#include "header/process/process.h"

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
int8_t copy_file(struct EXT2DriverRequest *src_request, struct EXT2DriverRequest *dst_request) {
    // Validasi input parameter
    if (!src_request || !dst_request) {
        return -1; // Invalid parameters
    }
    
    if (src_request->name_len == 0 || dst_request->name_len == 0) {
        return -1; // Invalid file names
    }
    
    // 1. Cari source file dalam parent directory
    struct EXT2Inode parent_inode;
    read_inode(src_request->parent_inode, &parent_inode);
    
    // Pastikan parent adalah directory
    if (!(parent_inode.i_mode & EXT2_S_IFDIR)) {
        return -1; // Parent is not a directory
    }
    
    uint8_t dir_block[BLOCK_SIZE];
    uint32_t source_inode_idx = 0;
    bool source_found = false;
    
    // Cari source file
    for (uint32_t block_idx = 0; block_idx < parent_inode.i_blocks && block_idx < 12; block_idx++) {
        if (parent_inode.i_block[block_idx] == 0) continue;
        
        read_blocks(dir_block, parent_inode.i_block[block_idx], 1);
        
        uint32_t offset = 0;
        while (offset < BLOCK_SIZE) {
            struct EXT2DirectoryEntry *entry = (struct EXT2DirectoryEntry *)(dir_block + offset);
            
            if (entry->inode == 0 || entry->rec_len == 0) break;
            if (offset + entry->rec_len > BLOCK_SIZE) break;
            
            // Bandingkan nama dengan source
            char *entry_name = (char *)(entry + 1);
            if (entry->name_len == src_request->name_len) {
                bool names_match = true;
                for (uint8_t i = 0; i < entry->name_len; i++) {
                    if (entry_name[i] != src_request->name[i]) {
                        names_match = false;
                        break;
                    }
                }
                if (names_match) {
                    source_inode_idx = entry->inode;
                    source_found = true;
                    
                    // Pastikan source bukan directory
                    if (entry->file_type == EXT2_FT_DIR) {
                        return -3; // Cannot copy directory
                    }
                    break;
                }
            }
            
            offset += entry->rec_len;
        }
        
        if (source_found) break;
    }
    
    if (!source_found) {
        return -1; // Source file not found
    }
    
    // 2. Cek apakah destination file sudah ada
    bool dest_exists = false;
    
    // Reset untuk pencarian destination
    for (uint32_t block_idx = 0; block_idx < parent_inode.i_blocks && block_idx < 12; block_idx++) {
        if (parent_inode.i_block[block_idx] == 0) continue;
        
        read_blocks(dir_block, parent_inode.i_block[block_idx], 1);
        
        uint32_t offset = 0;
        while (offset < BLOCK_SIZE) {
            struct EXT2DirectoryEntry *entry = (struct EXT2DirectoryEntry *)(dir_block + offset);
            
            if (entry->inode == 0 || entry->rec_len == 0) break;
            if (offset + entry->rec_len > BLOCK_SIZE) break;
            
            // Bandingkan nama dengan destination
            char *entry_name = (char *)(entry + 1);
            if (entry->name_len == dst_request->name_len) {
                bool names_match = true;
                for (uint8_t i = 0; i < entry->name_len; i++) {
                    if (entry_name[i] != dst_request->name[i]) {
                        names_match = false;
                        break;
                    }
                }
                if (names_match) {
                    dest_exists = true;
                    break;
                }
            }
            
            offset += entry->rec_len;
        }
        
        if (dest_exists) break;
    }
    
    if (dest_exists) {
        return -2; // Destination already exists
    }
    
    // 3. Baca source file inode dan data
    struct EXT2Inode source_inode;
    read_inode(source_inode_idx, &source_inode);
    
    // Pastikan source adalah file reguler
    if (source_inode.i_mode & EXT2_S_IFDIR) {
        return -3; // Source is a directory
    }
    
    // 4. Alokasi inode baru untuk destination
    uint32_t new_inode_idx = allocate_node();
    if (new_inode_idx == 0) {
        return -4; // Failed to allocate inode
    }
    
    // 5. Copy source inode properties ke destination inode
    struct EXT2Inode dest_inode;
    // Copy semua properties dari source
    for (int i = 0; i < (int)sizeof(struct EXT2Inode); i++) {
        ((uint8_t*)&dest_inode)[i] = ((uint8_t*)&source_inode)[i];
    }
    
    // Reset block pointers untuk destination
    for (int i = 0; i < 15; i++) {
        dest_inode.i_block[i] = 0;
    }
    
    // 6. Copy data blocks dari source ke destination
    uint32_t blocks_to_copy = (source_inode.i_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    if (blocks_to_copy > source_inode.i_blocks) {
        blocks_to_copy = source_inode.i_blocks;
    }
    
    uint8_t file_block[BLOCK_SIZE];
    uint32_t bgd_idx = inode_to_bgd(new_inode_idx);
    
    for (uint32_t i = 0; i < blocks_to_copy && i < 12; i++) { // Direct blocks only
        if (source_inode.i_block[i] == 0) continue;
        
        // Alokasi block baru untuk destination
        int32_t new_block = allocate_block(bgd_idx);
        if (new_block < 0) {
            // Cleanup: dealokasi inode dan blocks yang sudah dialokasi
            clear_inode_used(new_inode_idx);
            for (uint32_t j = 0; j < i; j++) {
                if (dest_inode.i_block[j] != 0) {
                    set_block_free(dest_inode.i_block[j]);
                }
            }
            return -5; // Failed to allocate blocks
        }
        
        // Copy data dari source block ke destination block
        read_blocks(file_block, source_inode.i_block[i], 1);
        write_blocks(file_block, new_block, 1);
        
        dest_inode.i_block[i] = new_block;
    }
    
    // 7. Sync destination inode ke disk
    sync_node(&dest_inode, new_inode_idx);
    
    // 8. Tambahkan entry untuk destination file ke parent directory
    bool entry_added = false;
    
    for (uint32_t block_idx = 0; block_idx < parent_inode.i_blocks && block_idx < 12; block_idx++) {
        if (parent_inode.i_block[block_idx] == 0) continue;
        
        read_blocks(dir_block, parent_inode.i_block[block_idx], 1);
        
        uint32_t offset = 0;
        struct EXT2DirectoryEntry *last_entry = NULL;
        
        // Cari entry terakhir dalam block
        while (offset < BLOCK_SIZE) {
            struct EXT2DirectoryEntry *entry = (struct EXT2DirectoryEntry *)(dir_block + offset);
            
            if (entry->inode == 0 || entry->rec_len == 0) break;
            if (offset + entry->rec_len > BLOCK_SIZE) break;
            
            last_entry = entry;
            offset += entry->rec_len;
        }
        
        if (last_entry) {
            // Hitung space yang dibutuhkan untuk entry baru
            uint16_t new_entry_size = get_entry_record_len(dst_request->name_len);
            uint16_t actual_last_size = get_entry_record_len(last_entry->name_len);
            uint16_t available_space = last_entry->rec_len - actual_last_size;
            
            if (available_space >= new_entry_size) {
                // Ada cukup space, buat entry baru
                uint32_t last_entry_offset = (uint8_t*)last_entry - dir_block;
                
                // Sesuaikan rec_len dari entry terakhir
                last_entry->rec_len = actual_last_size;
                
                // Buat entry baru
                uint32_t new_entry_offset = last_entry_offset + actual_last_size;
                struct EXT2DirectoryEntry *new_entry = (struct EXT2DirectoryEntry *)(dir_block + new_entry_offset);
                
                new_entry->inode = new_inode_idx;
                new_entry->name_len = dst_request->name_len;
                new_entry->file_type = EXT2_FT_REG_FILE; // Regular file
                new_entry->rec_len = BLOCK_SIZE - new_entry_offset; // Sisa space di block
                
                // Copy nama destination
                char *new_name_ptr = (char *)(new_entry + 1);
                for (uint8_t i = 0; i < dst_request->name_len; i++) {
                    new_name_ptr[i] = dst_request->name[i];
                }
                
                // Tulis block yang sudah dimodifikasi
                write_blocks(dir_block, parent_inode.i_block[block_idx], 1);
                entry_added = true;
                break;
            }
        }
    }
    
    if (!entry_added) {
        // Cleanup jika gagal menambahkan entry
        clear_inode_used(new_inode_idx);
        for (uint32_t i = 0; i < blocks_to_copy && i < 12; i++) {
            if (dest_inode.i_block[i] != 0) {
                set_block_free(dest_inode.i_block[i]);
            }
        }
        return -8; // Parent directory full
    }
    
    // 9. Sync parent directory dan superblock
    sync_node(&parent_inode, src_request->parent_inode);
    sync_superblock();
    
    return 0; // Success
}
int8_t create_directory(struct EXT2DriverRequest *request) {
    // Validasi input parameter
    if (!request || request->name_len == 0) {
        return -1; // Invalid parameter
    }
    
    // 1. Validasi parent directory exists dan valid
    struct EXT2Inode parent_inode;
    read_inode(request->parent_inode, &parent_inode);
    
    // Pastikan parent adalah directory
    if (!(parent_inode.i_mode & EXT2_S_IFDIR)) {
        return -2; // Parent is not a directory
    }
    
    // 2. Cek apakah file/directory dengan nama yang sama sudah ada
    // Baca directory entries secara manual untuk pengecekan yang lebih akurat
    uint8_t dir_block[BLOCK_SIZE];
    bool name_exists = false;
    
    // Periksa setiap block dari parent directory
    for (uint32_t block_idx = 0; block_idx < parent_inode.i_blocks && block_idx < 12; block_idx++) {
        if (parent_inode.i_block[block_idx] == 0) continue;
        
        read_blocks(dir_block, parent_inode.i_block[block_idx], 1);
        
        uint32_t offset = 0;
        while (offset < BLOCK_SIZE) {
            struct EXT2DirectoryEntry *entry = (struct EXT2DirectoryEntry *)(dir_block + offset);
            
            if (entry->inode == 0 || entry->rec_len == 0) break;
            if (offset + entry->rec_len > BLOCK_SIZE) break;
            
            // Bandingkan nama
            char *entry_name = (char *)(entry + 1);
            if (entry->name_len == request->name_len) {
                bool names_match = true;
                for (uint8_t i = 0; i < entry->name_len; i++) {
                    if (entry_name[i] != request->name[i]) {
                        names_match = false;
                        break;
                    }
                }
                if (names_match) {
                    name_exists = true;
                    break;
                }
            }
            
            offset += entry->rec_len;
        }
        
        if (name_exists) break;
    }
    
    if (name_exists) {
        return -3; // Directory/file already exists
    }
    
    // 3. Alokasi inode baru untuk directory
    uint32_t new_inode_idx = allocate_node();
    if (new_inode_idx == 0) {
        return -4; // Failed to allocate inode
    }
    
    // 4. Initialize inode untuk directory
    struct EXT2Inode new_dir_inode;
    // Clear semua field terlebih dahulu
    for (int i = 0; i < (int)sizeof(struct EXT2Inode); i++) {
        ((uint8_t*)&new_dir_inode)[i] = 0;
    }

    // Set directory properties
    new_dir_inode.i_mode = EXT2_S_IFDIR; // Set sebagai directory
    new_dir_inode.i_size = BLOCK_SIZE; // Ukuran minimal directory (1 block)
    new_dir_inode.i_blocks = 1; // Menggunakan 1 block
    
    // 5. Alokasi block untuk directory entries
    uint32_t bgd_idx = inode_to_bgd(new_inode_idx);
    int32_t new_block = allocate_block(bgd_idx);
    if (new_block < 0) {
        // Gagal alokasi block, dealokasi inode
        clear_inode_used(new_inode_idx);
        return -5; // Failed to allocate block
    }
    
    new_dir_inode.i_block[0] = new_block;
    
    // 6. Buat directory table dengan entries "." dan ".."
    //uint8_t dir_block[BLOCK_SIZE];
    // Clear block terlebih dahulu
    for (int i = 0; i < BLOCK_SIZE; i++) {
        dir_block[i] = 0;
    }
    
    uint32_t offset = 0;
    
    // Entry untuk "." (current directory)
    struct EXT2DirectoryEntry *current_entry = (struct EXT2DirectoryEntry *)(dir_block + offset);
    current_entry->inode = new_inode_idx;
    current_entry->name_len = 1;
    current_entry->file_type = EXT2_FT_DIR;
    current_entry->rec_len = get_entry_record_len(1);
    
    // Tulis nama "."
    char *name_ptr = (char *)(current_entry + 1);
    name_ptr[0] = '.';
    
    offset += current_entry->rec_len;
    
    // Entry untuk ".." (parent directory)
    struct EXT2DirectoryEntry *parent_entry = (struct EXT2DirectoryEntry *)(dir_block + offset);
    parent_entry->inode = request->parent_inode;
    parent_entry->name_len = 2;
    parent_entry->file_type = EXT2_FT_DIR;
    // Set rec_len ke sisa block untuk mengisi space yang tersisa
    parent_entry->rec_len = BLOCK_SIZE - offset;
    
    // Tulis nama ".."
    name_ptr = (char *)(parent_entry + 1);
    name_ptr[0] = '.';
    name_ptr[1] = '.';
    
    // 7. Tulis directory block ke disk
    write_blocks(dir_block, new_block, 1);
    
    // 8. Sync inode baru ke disk
    sync_node(&new_dir_inode, new_inode_idx);
    
    // 9. Tambahkan entry baru ke parent directory secara manual
    // Cari block dengan space yang cukup atau alokasi block baru
    bool entry_added = false;
    
    for (uint32_t block_idx = 0; block_idx < parent_inode.i_blocks && block_idx < 12; block_idx++) {
        if (parent_inode.i_block[block_idx] == 0) continue;
        
        read_blocks(dir_block, parent_inode.i_block[block_idx], 1);
        
        uint32_t offset = 0;
        struct EXT2DirectoryEntry *last_entry = NULL;
        
        // Cari entry terakhir dalam block
        while (offset < BLOCK_SIZE) {
            struct EXT2DirectoryEntry *entry = (struct EXT2DirectoryEntry *)(dir_block + offset);
            
            if (entry->inode == 0 || entry->rec_len == 0) break;
            if (offset + entry->rec_len > BLOCK_SIZE) break;
            
            last_entry = entry;
            offset += entry->rec_len;
        }
        
        if (last_entry) {
            // Hitung space yang dibutuhkan untuk entry baru
            uint16_t new_entry_size = get_entry_record_len(request->name_len);
            uint16_t actual_last_size = get_entry_record_len(last_entry->name_len);
            uint16_t available_space = last_entry->rec_len - actual_last_size;
            
            if (available_space >= new_entry_size) {
                // Ada cukup space, buat entry baru
                uint32_t last_entry_offset = (uint8_t*)last_entry - dir_block;
                
                // Sesuaikan rec_len dari entry terakhir
                last_entry->rec_len = actual_last_size;
                
                // Buat entry baru
                uint32_t new_entry_offset = last_entry_offset + actual_last_size;
                struct EXT2DirectoryEntry *new_entry = (struct EXT2DirectoryEntry *)(dir_block + new_entry_offset);
                
                new_entry->inode = new_inode_idx;
                new_entry->name_len = request->name_len;
                new_entry->file_type = EXT2_FT_DIR; // Pastikan ini adalah directory!
                new_entry->rec_len = BLOCK_SIZE - new_entry_offset; // Sisa space di block
                
                // Copy nama
                char *new_name_ptr = (char *)(new_entry + 1);
                for (uint8_t i = 0; i < request->name_len; i++) {
                    new_name_ptr[i] = request->name[i];
                }
                
                // Tulis block yang sudah dimodifikasi
                write_blocks(dir_block, parent_inode.i_block[block_idx], 1);
                entry_added = true;
                break;
            }
        }
    }
    
    // Jika belum bisa menambahkan entry, mungkin perlu alokasi block baru untuk parent
    if (!entry_added) {
        // Untuk sederhananya, return error jika parent directory penuh
        // Dalam implementasi lengkap, kita harus alokasi block baru untuk parent
        clear_inode_used(new_inode_idx);
        set_block_free(new_block);
        return -6; // Parent directory full
    }
    
    // 10. Sync parent directory inode ke disk (update i_size jika diperlukan)
    sync_node(&parent_inode, request->parent_inode);
    
    // 11. Update superblock untuk mencerminkan penggunaan resource baru
    sync_superblock();
    
    return 0; // Success
}
int8_t delete_file(struct EXT2DriverRequest *request) {
    // Validasi input parameter
    if (!request || request->name_len == 0) {
        return -1; // Invalid parameter
    }
    
    // 1. Validasi parent directory exists dan valid
    struct EXT2Inode parent_inode;
    read_inode(request->parent_inode, &parent_inode);
    
    // Pastikan parent adalah directory
    if (!(parent_inode.i_mode & EXT2_S_IFDIR)) {
        return -2; // Parent is not a directory
    }
    
    // 2. Cari file yang akan dihapus dalam parent directory
    uint8_t dir_block[BLOCK_SIZE];
    uint32_t target_inode_idx = 0;
    uint32_t target_block_idx = 0;
    uint32_t target_entry_offset = 0;
    bool file_found = false;
    
    // Periksa setiap block dari parent directory
    for (uint32_t block_idx = 0; block_idx < parent_inode.i_blocks && block_idx < 12; block_idx++) {
        if (parent_inode.i_block[block_idx] == 0) continue;
        
        read_blocks(dir_block, parent_inode.i_block[block_idx], 1);
        
        uint32_t offset = 0;
        while (offset < BLOCK_SIZE) {
            struct EXT2DirectoryEntry *entry = (struct EXT2DirectoryEntry *)(dir_block + offset);
            
            if (entry->inode == 0 || entry->rec_len == 0) break;
            if (offset + entry->rec_len > BLOCK_SIZE) break;
            
            // Bandingkan nama
            char *entry_name = (char *)(entry + 1);
            if (entry->name_len == request->name_len) {
                bool names_match = true;
                for (uint8_t i = 0; i < entry->name_len; i++) {
                    if (entry_name[i] != request->name[i]) {
                        names_match = false;
                        break;
                    }
                }
                if (names_match) {
                    // Pastikan ini adalah file, bukan directory
                    if (entry->file_type == EXT2_FT_DIR) {
                        return -6; // Target is a directory, not a file
                    }
                    
                    target_inode_idx = entry->inode;
                    target_block_idx = block_idx;
                    target_entry_offset = offset;
                    file_found = true;
                    break;
                }
            }
            
            offset += entry->rec_len;
        }
        
        if (file_found) break;
    }
    
    if (!file_found) {
        return -3; // File not found
    }
    
    // 3. Baca inode file yang akan dihapus
    struct EXT2Inode file_inode;
    read_inode(target_inode_idx, &file_inode);
    
    // 4. Dealokasi semua block yang digunakan oleh file
    for (uint32_t i = 0; i < file_inode.i_blocks && i < 12; i++) {
        if (file_inode.i_block[i] != 0) {
            deallocate_block(file_inode.i_block[i]);
        }
    }
    
    // 5. Dealokasi inode
    clear_inode_used(target_inode_idx);
    
    // 6. Hapus entry dari parent directory
    read_blocks(dir_block, parent_inode.i_block[target_block_idx], 1);
    
    struct EXT2DirectoryEntry *target_entry = (struct EXT2DirectoryEntry *)(dir_block + target_entry_offset);
    uint16_t entry_rec_len = target_entry->rec_len;
    
    // Cari entry sebelumnya untuk menggabungkan rec_len
    if (target_entry_offset > 0) {
        // Ada entry sebelumnya, gabungkan rec_len
        uint32_t prev_offset = 0;
        struct EXT2DirectoryEntry *prev_entry = NULL;
        
        while (prev_offset < target_entry_offset) {
            struct EXT2DirectoryEntry *entry = (struct EXT2DirectoryEntry *)(dir_block + prev_offset);
            if (prev_offset + entry->rec_len == target_entry_offset) {
                prev_entry = entry;
                break;
            }
            prev_offset += entry->rec_len;
        }
        
        if (prev_entry) {
            prev_entry->rec_len += entry_rec_len;
        }
    } else {
        // Entry pertama, cari entry berikutnya
        if (target_entry_offset + entry_rec_len < BLOCK_SIZE) {
            struct EXT2DirectoryEntry *next_entry = (struct EXT2DirectoryEntry *)(dir_block + target_entry_offset + entry_rec_len);
            if (next_entry->inode != 0) {
                // Copy next entry ke posisi current entry
                uint32_t copy_size = BLOCK_SIZE - (target_entry_offset + entry_rec_len);
                for (uint32_t i = 0; i < copy_size; i++) {
                    dir_block[target_entry_offset + i] = dir_block[target_entry_offset + entry_rec_len + i];
                }
                
                // Update rec_len dari entry yang dipindah
                struct EXT2DirectoryEntry *moved_entry = (struct EXT2DirectoryEntry *)(dir_block + target_entry_offset);
                moved_entry->rec_len += entry_rec_len;
            } else {
                // Tidak ada entry berikutnya, clear entry
                target_entry->inode = 0;
                target_entry->rec_len = entry_rec_len;
                target_entry->name_len = 0;
                target_entry->file_type = 0;
            }
        }
    }
    
    // 7. Tulis kembali directory block
    write_blocks(dir_block, parent_inode.i_block[target_block_idx], 1);
    
    return 0; // Success
}

int8_t delete_directory(struct EXT2DriverRequest *request) {
    // Validasi input parameter
    if (!request || request->name_len == 0) {
        return -1; // Invalid parameter
    }
    
    // 1. Validasi parent directory exists dan valid
    struct EXT2Inode parent_inode;
    read_inode(request->parent_inode, &parent_inode);
    
    // Pastikan parent adalah directory
    if (!(parent_inode.i_mode & EXT2_S_IFDIR)) {
        return -2; // Parent is not a directory
    }
    
    // 2. Cari directory yang akan dihapus dalam parent directory
    uint8_t dir_block[BLOCK_SIZE];
    uint32_t target_inode_idx = 0;
    uint32_t target_block_idx = 0;
    uint32_t target_entry_offset = 0;
    bool dir_found = false;
    
    // Periksa setiap block dari parent directory
    for (uint32_t block_idx = 0; block_idx < parent_inode.i_blocks && block_idx < 12; block_idx++) {
        if (parent_inode.i_block[block_idx] == 0) continue;
        
        read_blocks(dir_block, parent_inode.i_block[block_idx], 1);
        
        uint32_t offset = 0;
        while (offset < BLOCK_SIZE) {
            struct EXT2DirectoryEntry *entry = (struct EXT2DirectoryEntry *)(dir_block + offset);
            
            if (entry->inode == 0 || entry->rec_len == 0) break;
            if (offset + entry->rec_len > BLOCK_SIZE) break;
            
            // Bandingkan nama
            char *entry_name = (char *)(entry + 1);
            if (entry->name_len == request->name_len) {
                bool names_match = true;
                for (uint8_t i = 0; i < entry->name_len; i++) {
                    if (entry_name[i] != request->name[i]) {
                        names_match = false;
                        break;
                    }
                }
                if (names_match) {
                    // Pastikan ini adalah directory, bukan file
                    if (entry->file_type != EXT2_FT_DIR) {
                        return -6; // Target is a file, not a directory
                    }
                    
                    target_inode_idx = entry->inode;
                    target_block_idx = block_idx;
                    target_entry_offset = offset;
                    dir_found = true;
                    break;
                }
            }
            
            offset += entry->rec_len;
        }
        
        if (dir_found) break;
    }
    
    if (!dir_found) {
        return -3; // Directory not found
    }
    
    // 3. Baca inode directory yang akan dihapus
    struct EXT2Inode target_dir_inode;
    read_inode(target_inode_idx, &target_dir_inode);
    
    // 4. Pastikan directory kosong (hanya berisi "." dan "..")
    uint8_t target_dir_block[BLOCK_SIZE];
    bool is_empty = true;
    
    for (uint32_t block_idx = 0; block_idx < target_dir_inode.i_blocks && block_idx < 12; block_idx++) {
        if (target_dir_inode.i_block[block_idx] == 0) continue;
        
        read_blocks(target_dir_block, target_dir_inode.i_block[block_idx], 1);
        
        uint32_t offset = 0;
        while (offset < BLOCK_SIZE) {
            struct EXT2DirectoryEntry *entry = (struct EXT2DirectoryEntry *)(target_dir_block + offset);
            
            if (entry->inode == 0 || entry->rec_len == 0) break;
            if (offset + entry->rec_len > BLOCK_SIZE) break;
            
            // Skip "." dan ".." entries
            char *entry_name = (char *)(entry + 1);
            bool is_current_dir = (entry->name_len == 1 && entry_name[0] == '.');
            bool is_parent_dir = (entry->name_len == 2 && entry_name[0] == '.' && entry_name[1] == '.');
            
            if (!is_current_dir && !is_parent_dir) {
                is_empty = false;
                break;
            }
            
            offset += entry->rec_len;
        }
        
        if (!is_empty) break;
    }
    
    if (!is_empty) {
        return -7; // Directory is not empty
    }
    
    // 5. Dealokasi semua block yang digunakan oleh directory
    for (uint32_t i = 0; i < target_dir_inode.i_blocks && i < 12; i++) {
        if (target_dir_inode.i_block[i] != 0) {
            deallocate_block(target_dir_inode.i_block[i]);
        }
    }
    
    // 6. Dealokasi inode
    clear_inode_used(target_inode_idx);
    
    // 7. Hapus entry dari parent directory (sama seperti delete_file)
    read_blocks(dir_block, parent_inode.i_block[target_block_idx], 1);
    
    struct EXT2DirectoryEntry *target_entry = (struct EXT2DirectoryEntry *)(dir_block + target_entry_offset);
    uint16_t entry_rec_len = target_entry->rec_len;
    
    // Cari entry sebelumnya untuk menggabungkan rec_len
    if (target_entry_offset > 0) {
        // Ada entry sebelumnya, gabungkan rec_len
        uint32_t prev_offset = 0;
        struct EXT2DirectoryEntry *prev_entry = NULL;
        
        while (prev_offset < target_entry_offset) {
            struct EXT2DirectoryEntry *entry = (struct EXT2DirectoryEntry *)(dir_block + prev_offset);
            if (prev_offset + entry->rec_len == target_entry_offset) {
                prev_entry = entry;
                break;
            }
            prev_offset += entry->rec_len;
        }
        
        if (prev_entry) {
            prev_entry->rec_len += entry_rec_len;
        }
    } else {
        // Entry pertama, cari entry berikutnya
        if (target_entry_offset + entry_rec_len < BLOCK_SIZE) {
            struct EXT2DirectoryEntry *next_entry = (struct EXT2DirectoryEntry *)(dir_block + target_entry_offset + entry_rec_len);
            if (next_entry->inode != 0) {
                // Copy next entry ke posisi current entry
                uint32_t copy_size = BLOCK_SIZE - (target_entry_offset + entry_rec_len);
                for (uint32_t i = 0; i < copy_size; i++) {
                    dir_block[target_entry_offset + i] = dir_block[target_entry_offset + entry_rec_len + i];
                }
                
                // Update rec_len dari entry yang dipindah
                struct EXT2DirectoryEntry *moved_entry = (struct EXT2DirectoryEntry *)(dir_block + target_entry_offset);
                moved_entry->rec_len += entry_rec_len;
            } else {
                // Tidak ada entry berikutnya, clear entry
                target_entry->inode = 0;
                target_entry->rec_len = entry_rec_len;
                target_entry->name_len = 0;
                target_entry->file_type = 0;
            }
        }
    }
    
    // 8. Tulis kembali directory block
    write_blocks(dir_block, parent_inode.i_block[target_block_idx], 1);
    
    return 0; // Success
}
int8_t move_file(struct EXT2DriverRequest *src_request, struct EXT2DriverRequest *dst_request) {
    // Validasi input
    if (!src_request || !dst_request) {
        return -1; // Invalid parameters
    }
    
    // Validasi nama file
    if (src_request->name_len == 0 || dst_request->name_len == 0) {
        return -1; // Invalid parameters
    }
    
    // 1. Cari source file/directory di parent directory
    struct EXT2Inode parent_inode;
    read_inode(src_request->parent_inode, &parent_inode);
    
    // Verifikasi parent adalah directory
    if (!is_directory(&parent_inode)) {
        return -1; // Parent is not a directory
    }
    
    // Buat salinan nama source yang aman
    char src_name[256];
    size_t src_name_len = src_request->name_len;
    if (src_name_len >= sizeof(src_name)) {
        src_name_len = sizeof(src_name) - 1;
    }
    memcpy(src_name, src_request->name, src_name_len);
    src_name[src_name_len] = '\0';
    
    // Cari inode source
    uint32_t src_inode_num = 0;
    if (!find_inode_in_dir(&parent_inode, src_name, &src_inode_num)) {
        return -2; // Source file not found
    }
    
    // 2. Cek apakah destination sudah ada
    char dst_name[256];
    size_t dst_name_len = dst_request->name_len;
    if (dst_name_len >= sizeof(dst_name)) {
        dst_name_len = sizeof(dst_name) - 1;
    }
    memcpy(dst_name, dst_request->name, dst_name_len);
    dst_name[dst_name_len] = '\0';
    
    uint32_t dst_inode_num = 0;
    if (find_inode_in_dir(&parent_inode, dst_name, &dst_inode_num)) {
        return -3; // Destination already exists
    }
    
    // 3. Baca source inode untuk validasi
    struct EXT2Inode src_inode;
    read_inode(src_inode_num, &src_inode);
    
    // Jika source adalah directory, cek apakah kosong (untuk keamanan)
    if (is_directory(&src_inode)) {
        if (!is_empty_directory(&src_inode)) {
            // Untuk implementasi sederhana, hanya izinkan move directory kosong
            // Atau implementasi yang lebih kompleks bisa move seluruh isi directory
            return -5; // Directory not empty
        }
    }
    
    // 4. RENAME/MOVE operation
    // Karena kita dalam direktori yang sama, ini adalah operasi rename
    // Hapus entry lama dan buat entry baru dengan nama baru
    
    // Hapus entry lama dari directory
    remove_inode_from_dir(&parent_inode, src_name);
    
    // Tambah entry baru dengan nama baru
    add_inode_to_dir(&parent_inode, src_inode_num, dst_name);
    
    // Update file type dalam directory entry jika diperlukan
    // (add_inode_to_dir sudah mengurus ini)
    
    return 0; // Success
}
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
  case 17: // SYS_READ_FILE
{
    struct EXT2DriverRequest *request = (struct EXT2DriverRequest *)frame.cpu.general.ebx;
    int8_t *result = (int8_t *)frame.cpu.general.ecx;
    
    *result = read(*request);
    break;
}
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

  case 21:
  {
    struct EXT2Inode* out_inode = (struct EXT2Inode*) frame.cpu.general.ebx;
    uint32_t inode_idx = frame.cpu.general.ecx;
    read_inode(inode_idx, out_inode); // Pastikan ada fungsi read_inode di ext2.c
    break;
    
  }
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
  case 23: // SYS_READ_BLOCK
  {
    uint8_t* buffer = (uint8_t*) frame.cpu.general.ebx;
    uint32_t block_num = frame.cpu.general.ecx;
    uint32_t count = frame.cpu.general.edx;
    read_blocks(buffer, block_num, count); // Pastikan fungsi ini ada
    break;
  }
  
  case 24:
  { struct EXT2DriverRequest *request = (struct EXT2DriverRequest *)frame.cpu.general.ebx;
    int8_t *result = (int8_t *)frame.cpu.general.ecx;
    
    *result = create_directory(request);
    break;
  }
  case 25: // SYS_DELETE_FILE - Delete file
  {
      struct EXT2DriverRequest *request = (struct EXT2DriverRequest *)frame.cpu.general.ebx;
      int8_t *result = (int8_t *)frame.cpu.general.ecx;
      
      *result = delete_file(request);
  }
  break;

  case 26: // SYS_DELETE_DIR - Delete directory
  {
      struct EXT2DriverRequest *request = (struct EXT2DriverRequest *)frame.cpu.general.ebx;
      int8_t *result = (int8_t *)frame.cpu.general.ecx;
      
      *result = delete_directory(request);
  }
  break;
  case 28: // SYS_COPY_FILE
  {
    struct EXT2DriverRequest *src_request = (struct EXT2DriverRequest *)frame.cpu.general.ebx;
    struct EXT2DriverRequest *dst_request = (struct EXT2DriverRequest *)frame.cpu.general.ecx;
    int8_t *result = (int8_t *)frame.cpu.general.edx;
    
    *result = copy_file(src_request, dst_request);
    break;
  }
  case 29: // SYS_MOVE_FILE
  {
      struct EXT2DriverRequest *src_request = (struct EXT2DriverRequest *)frame.cpu.general.ebx;
      struct EXT2DriverRequest *dst_request = (struct EXT2DriverRequest *)frame.cpu.general.ecx;
      int8_t *result = (int8_t *)frame.cpu.general.edx;
      
      *result = move_file(src_request, dst_request);
      break;
  }
  // Tambahkan case baru di syscall handler:

  case 30: // SYS_EXEC - Create new user process
  {
      struct EXT2DriverRequest *request = (struct EXT2DriverRequest *)frame.cpu.general.ebx;
      int32_t *result = (int32_t *)frame.cpu.general.ecx;
      
      *result = process_create_user_process(*request);
      break;
  }

  case 31: // SYS_GET_PROCESS_INFO - Get process information
  {
      struct ProcessControlBlock *pcb_array = (struct ProcessControlBlock *)frame.cpu.general.ebx;
      int *count = (int *)frame.cpu.general.ecx;
      
      getActivePCB(pcb_array, count);
      break;
  }

  case 32: // SYS_KILL_PROCESS - Terminate process by PID
  {
      uint32_t pid = frame.cpu.general.ebx;
      bool *result = (bool *)frame.cpu.general.ecx;
      
      *result = process_destroy(pid);
      break;
  }

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
