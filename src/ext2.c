#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include "header/stdlib/string.h"
#include "header/filesystem/ext2.h"
#include "header/driver/disk.h"

#ifdef DEBUG_MODE
#define DEBUG_PRINT(...) printf(__VA_ARGS__)
#else
#define DEBUG_PRINT(...) ((void)0)
#endif

static struct EXT2Superblock superblock;
struct EXT2BlockGroupDescriptorTable bgd_table;

uint32_t ceil_div(uint32_t a, uint32_t b)
{
  if (b == 0)
  {
    return 0;
  }

  uint32_t result = a / b;

  if (a % b != 0)
  {
    result++;
  }

  return result;
}

bool is_inode_used(uint32_t inode)
{
  uint32_t group = inode_to_bgd(inode);
  uint32_t local_inode = inode_to_local(inode);
  uint32_t byte_offset = local_inode / 8;
  uint32_t bit_offset = local_inode % 8;

  struct BlockBuffer buffer;
  read_blocks(&buffer, bgd_table.table[group].bg_inode_bitmap, 1);
  return (buffer.buf[byte_offset] & (1 << bit_offset)) != 0;
}

void set_inode_used(uint32_t inode)
{
  uint32_t group = inode_to_bgd(inode);
  uint32_t local_inode = inode_to_local(inode);
  uint32_t byte_offset = local_inode / 8;
  uint32_t bit_offset = local_inode % 8;

  struct BlockBuffer buffer;
  read_blocks(&buffer, bgd_table.table[group].bg_inode_bitmap, 1);
  buffer.buf[byte_offset] |= (1 << bit_offset);
  write_blocks(&buffer, bgd_table.table[group].bg_inode_bitmap, 1);
}

void clear_inode_used(uint32_t inode)
{
  uint32_t group = inode_to_bgd(inode);
  uint32_t local_inode = inode_to_local(inode);
  uint32_t byte_offset = local_inode / 8;
  uint32_t bit_offset = local_inode % 8;

  struct BlockBuffer buffer;
  read_blocks(&buffer, bgd_table.table[group].bg_inode_bitmap, 1);
  buffer.buf[byte_offset] &= ~(1 << bit_offset);
  write_blocks(&buffer, bgd_table.table[group].bg_inode_bitmap, 1);
}

void init_directory_table(struct EXT2Inode *node, uint32_t inode, uint32_t parent_inode)
{
  uint8_t dir_data[BLOCK_SIZE] = {0};
  struct EXT2DirectoryEntry *entry = (struct EXT2DirectoryEntry *)dir_data;

  // Entri untuk "."
  entry->inode = inode;
  entry->name_len = 1;
  entry->file_type = EXT2_FT_DIR;
  entry->rec_len = 12;
  char *name = (char *)(entry + 1);
  name[0] = '.';

  // Entri untuk ".."
  entry = (struct EXT2DirectoryEntry *)(dir_data + entry->rec_len);
  entry->inode = parent_inode;
  entry->name_len = 2;
  entry->file_type = EXT2_FT_DIR;
  entry->rec_len = BLOCK_SIZE - 12;
  name = (char *)(entry + 1);
  name[0] = '.';
  name[1] = '.';

  write_blocks(dir_data, node->i_block[0], 1);
}

/**
 * @brief Mendapatkan nomor blok fisik dari indeks blok logis
 */
uint32_t get_physical_block_from_logical(struct EXT2Inode *inode, uint32_t logical_block_idx)
{
    const uint32_t ptrs_per_block = BLOCK_SIZE / sizeof(uint32_t); // 128 untuk BLOCK_SIZE=512
    
    if (logical_block_idx < 12) {
        // Direct blocks (0-11)
        return inode->i_block[logical_block_idx];
    }
    else if (logical_block_idx < 12 + ptrs_per_block) {
        // Single indirect block (12 sampai 139 untuk BLOCK_SIZE=512)
        if (inode->i_block[12] == 0) return 0;
        
        uint32_t indirect_table[ptrs_per_block];
        read_blocks(indirect_table, inode->i_block[12], 1);
        
        uint32_t indirect_idx = logical_block_idx - 12;
        return indirect_table[indirect_idx];
    }
    else if (logical_block_idx < 12 + ptrs_per_block + (ptrs_per_block * ptrs_per_block)) {
        // Double indirect block (140 sampai 16523 untuk BLOCK_SIZE=512)
        if (inode->i_block[13] == 0) return 0;
        
        uint32_t double_indirect_table[ptrs_per_block];
        read_blocks(double_indirect_table, inode->i_block[13], 1);
        
        uint32_t double_indirect_base = 12 + ptrs_per_block;
        uint32_t double_indirect_offset = logical_block_idx - double_indirect_base;
        uint32_t first_level_idx = double_indirect_offset / ptrs_per_block;
        uint32_t second_level_idx = double_indirect_offset % ptrs_per_block;
        
        if (double_indirect_table[first_level_idx] == 0) return 0;
        
        uint32_t indirect_table[ptrs_per_block];
        read_blocks(indirect_table, double_indirect_table[first_level_idx], 1);
        
        return indirect_table[second_level_idx];
    }
    
    // Triple indirect tidak diimplementasikan untuk filesystem 4MB
    return 0;
}

/**
 * @brief Mengalokasi blok pada indeks logis tertentu
 */
uint32_t allocate_logical_block(struct EXT2Inode *inode, uint32_t logical_block_idx, uint32_t preferred_bgd)
{
    const uint32_t ptrs_per_block = BLOCK_SIZE / sizeof(uint32_t);
    
    if (logical_block_idx < 12) {
        // Direct blocks
        if (inode->i_block[logical_block_idx] == 0) {
            inode->i_block[logical_block_idx] = allocate_block(preferred_bgd);
        }
        return inode->i_block[logical_block_idx];
    }
    else if (logical_block_idx < 12 + ptrs_per_block) {
        // Single indirect block
        
        // Alokasi blok untuk indirect table jika belum ada
        if (inode->i_block[12] == 0) {
            inode->i_block[12] = allocate_block(preferred_bgd);
            
            // Inisialisasi indirect table dengan nol
            uint32_t zero_table[ptrs_per_block];
            memset(zero_table, 0, BLOCK_SIZE);
            write_blocks(zero_table, inode->i_block[12], 1);
        }
        
        // Baca indirect table
        uint32_t indirect_table[ptrs_per_block];
        read_blocks(indirect_table, inode->i_block[12], 1);
        
        uint32_t indirect_idx = logical_block_idx - 12;
        
        // Alokasi blok data jika belum ada
        if (indirect_table[indirect_idx] == 0) {
            indirect_table[indirect_idx] = allocate_block(preferred_bgd);
            write_blocks(indirect_table, inode->i_block[12], 1);
        }
        
        return indirect_table[indirect_idx];
    }
    else if (logical_block_idx < 12 + ptrs_per_block + (ptrs_per_block * ptrs_per_block)) {
        // Double indirect block
        
        // Alokasi blok untuk double indirect table jika belum ada
        if (inode->i_block[13] == 0) {
            inode->i_block[13] = allocate_block(preferred_bgd);
            
            uint32_t zero_table[ptrs_per_block];
            memset(zero_table, 0, BLOCK_SIZE);
            write_blocks(zero_table, inode->i_block[13], 1);
        }
        
        // Baca double indirect table
        uint32_t double_indirect_table[ptrs_per_block];
        read_blocks(double_indirect_table, inode->i_block[13], 1);
        
        uint32_t double_indirect_base = 12 + ptrs_per_block;
        uint32_t double_indirect_offset = logical_block_idx - double_indirect_base;
        uint32_t first_level_idx = double_indirect_offset / ptrs_per_block;
        uint32_t second_level_idx = double_indirect_offset % ptrs_per_block;
        
        // Alokasi blok untuk indirect table level kedua jika belum ada
        if (double_indirect_table[first_level_idx] == 0) {
            double_indirect_table[first_level_idx] = allocate_block(preferred_bgd);
            write_blocks(double_indirect_table, inode->i_block[13], 1);
            
            uint32_t zero_table[ptrs_per_block];
            memset(zero_table, 0, BLOCK_SIZE);
            write_blocks(zero_table, double_indirect_table[first_level_idx], 1);
        }
        
        // Baca indirect table level kedua
        uint32_t indirect_table[ptrs_per_block];
        read_blocks(indirect_table, double_indirect_table[first_level_idx], 1);
        
        // Alokasi blok data jika belum ada
        if (indirect_table[second_level_idx] == 0) {
            indirect_table[second_level_idx] = allocate_block(preferred_bgd);
            write_blocks(indirect_table, double_indirect_table[first_level_idx], 1);
        }
        
        return indirect_table[second_level_idx];
    }
    
    return 0; // Triple indirect tidak didukung
}

/**
 * @brief Membaca data dari inode dengan dukungan indirect blocks
 */
void read_inode_data_extended(struct EXT2Inode *inode, void *buf, uint32_t size)
{
    if (inode == NULL || size == 0)
        return;

    uint32_t bytes_read = 0;
    uint32_t logical_block_idx = 0;
    uint32_t total_blocks = ceil_div(inode->i_size, BLOCK_SIZE);
    
    uint8_t *output_buffer = (uint8_t *)buf;

    while (bytes_read < size && logical_block_idx < total_blocks)
    {
        // Dapatkan nomor blok fisik
        uint32_t physical_block = get_physical_block_from_logical(inode, logical_block_idx);
        
        if (physical_block != 0) {
            uint8_t block_buf[BLOCK_SIZE];
            read_blocks(block_buf, physical_block, 1);

            uint32_t to_read = size - bytes_read;
            if (to_read > BLOCK_SIZE)
                to_read = BLOCK_SIZE;
                
            // Untuk blok terakhir, batasi sesuai ukuran file sebenarnya
            uint32_t file_bytes_remaining = inode->i_size - bytes_read;
            if (to_read > file_bytes_remaining)
                to_read = file_bytes_remaining;

            memcpy(output_buffer + bytes_read, block_buf, to_read);
            bytes_read += to_read;
        } else {
            // Blok tidak dialokasi, isi dengan nol (sparse file)
            uint32_t to_read = size - bytes_read;
            if (to_read > BLOCK_SIZE)
                to_read = BLOCK_SIZE;
                
            uint32_t file_bytes_remaining = inode->i_size - bytes_read;
            if (to_read > file_bytes_remaining)
                to_read = file_bytes_remaining;
            
            memset(output_buffer + bytes_read, 0, to_read);
            bytes_read += to_read;
        }
        
        logical_block_idx++;
    }
}

/**
 * @brief Mengalokasi blok untuk inode dengan dukungan indirect blocks
 */
void allocate_node_blocks_extended(void *ptr, struct EXT2Inode *node, uint32_t preferred_bgd)
{
    uint32_t blocks_needed = ceil_div(node->i_size, BLOCK_SIZE);
    uint8_t *data = (uint8_t *)ptr;
    
    // Update jumlah blok yang dialokasikan
    node->i_blocks = blocks_needed;
    
    // Batasi maksimum blok yang didukung untuk filesystem 4MB
    const uint32_t ptrs_per_block = BLOCK_SIZE / sizeof(uint32_t); // 128
    uint32_t max_direct = 12;
    uint32_t max_single_indirect = ptrs_per_block; // 128
    uint32_t max_double_indirect = ptrs_per_block * ptrs_per_block; // 16384
    uint32_t max_supported = max_direct + max_single_indirect + max_double_indirect; // 16524
    
    if (blocks_needed > max_supported) {
        DEBUG_PRINT("Error: File terlalu besar. Max blok yang didukung: %u, diminta: %u\n", 
                   max_supported, blocks_needed);
        blocks_needed = max_supported;
        node->i_blocks = blocks_needed;
    }

    // Alokasi blok secara berurutan
    for (uint32_t logical_block_idx = 0; logical_block_idx < blocks_needed; logical_block_idx++) {
        uint32_t physical_block = allocate_logical_block(node, logical_block_idx, preferred_bgd);
        
        if (physical_block != 0) {
            // Tulis data ke blok
            uint32_t bytes_to_write = node->i_size - (logical_block_idx * BLOCK_SIZE);
            if (bytes_to_write > BLOCK_SIZE)
                bytes_to_write = BLOCK_SIZE;

            uint8_t buffer[BLOCK_SIZE] = {0};
            if (data != NULL) {
                memcpy(buffer, data + (logical_block_idx * BLOCK_SIZE), bytes_to_write);
            }
            write_blocks(buffer, physical_block, 1);
        } else {
            DEBUG_PRINT("Error: Gagal mengalokasi blok logis %u\n", logical_block_idx);
            break;
        }
    }
}

/**
 * @brief Fungsi dealokasi blok dengan dukungan indirect blocks
 */
void deallocate_node_blocks_extended(struct EXT2Inode *inode)
{
    if (inode == NULL) return;
    
    const uint32_t ptrs_per_block = BLOCK_SIZE / sizeof(uint32_t);
    
    // Dealokasi direct blocks (0-11)
    for (uint32_t i = 0; i < 12 && i < inode->i_blocks; i++) {
        if (inode->i_block[i] != 0) {
            set_block_free(inode->i_block[i]);
            inode->i_block[i] = 0;
        }
    }

    // Dealokasi single indirect blocks
    if (inode->i_block[12] != 0) {
        uint32_t indirect_table[ptrs_per_block];
        read_blocks(indirect_table, inode->i_block[12], 1);
        
        // Dealokasi semua blok data yang dirujuk
        for (uint32_t i = 0; i < ptrs_per_block; i++) {
            if (indirect_table[i] != 0) {
                set_block_free(indirect_table[i]);
            }
        }
        
        // Dealokasi blok indirect table itu sendiri
        set_block_free(inode->i_block[12]);
        inode->i_block[12] = 0;
    }

    // Dealokasi double indirect blocks
    if (inode->i_block[13] != 0) {
        uint32_t double_indirect_table[ptrs_per_block];
        read_blocks(double_indirect_table, inode->i_block[13], 1);
        
        for (uint32_t i = 0; i < ptrs_per_block; i++) {
            if (double_indirect_table[i] != 0) {
                uint32_t indirect_table[ptrs_per_block];
                read_blocks(indirect_table, double_indirect_table[i], 1);
                
                // Dealokasi semua blok data yang dirujuk
                for (uint32_t j = 0; j < ptrs_per_block; j++) {
                    if (indirect_table[j] != 0) {
                        set_block_free(indirect_table[j]);
                    }
                }
                
                // Dealokasi indirect table level kedua
                set_block_free(double_indirect_table[i]);
            }
        }
        
        // Dealokasi double indirect table itu sendiri
        set_block_free(inode->i_block[13]);
        inode->i_block[13] = 0;
    }

    // Reset semua pointer
    for (uint32_t i = 0; i < 15; i++) {
        inode->i_block[i] = 0;
    }
    inode->i_blocks = 0;
}

void allocate_node_blocks(void *ptr, struct EXT2Inode *node, uint32_t prefered_bgd)
{
  uint32_t blocks_needed = ceil_div(node->i_size, BLOCK_SIZE);
  uint8_t *data = (uint8_t *)ptr;

  // Batasi jumlah blok yang digunakan (hanya direct blocks)
  uint32_t blocks_to_allocate = blocks_needed > 12 ? 12 : blocks_needed;
  node->i_blocks = blocks_to_allocate;

  for (uint32_t i = 0; i < blocks_to_allocate; i++)
  {
    node->i_block[i] = allocate_block(prefered_bgd);

    // Tulis data ke blok ini
    uint32_t bytes_to_write = node->i_size - (i * BLOCK_SIZE);
    if (bytes_to_write > BLOCK_SIZE)
      bytes_to_write = BLOCK_SIZE;

    uint8_t buffer[BLOCK_SIZE] = {0};
    memcpy(buffer, data + (i * BLOCK_SIZE), bytes_to_write);
    write_blocks(buffer, node->i_block[i], 1);
  }
}

int8_t write(struct EXT2DriverRequest request)
{
  // Validasi input dengan bound checking yang lebih ketat
  DEBUG_PRINT("DEBUG write: Entering function\n");
  DEBUG_PRINT("DEBUG write: name_len=%u, buffer_size=%u\n",
              request.name_len, request.buffer_size);

  // Fix: Since name_len is uint8_t (0-255), only check for 0
  if (request.name_len == 0)
  {
    DEBUG_PRINT("Error: Invalid filename length: %u\n", request.name_len);
    return -1;
  }

  // Pastikan name tidak kosong dan valid
  if (request.name[0] == '\0')
  {
    DEBUG_PRINT("Error: Empty filename\n");
    return -1;
  }

  // Validasi buffer
  if (request.buffer_size > 0 && request.buf == NULL)
  {
    DEBUG_PRINT("Error: Buffer is NULL but size is non-zero\n");
    return -1;
  }

  // Perbesar batasan ukuran file karena sekarang mendukung indirect blocks
  // Maksimum file sekarang bisa mencapai ~8MB dengan indirect blocks
  const uint32_t ptrs_per_block = BLOCK_SIZE / sizeof(uint32_t); // 128
  uint32_t max_file_size = (12 + ptrs_per_block + ptrs_per_block * ptrs_per_block) * BLOCK_SIZE;
  
  if (request.buffer_size > max_file_size)
  {
    DEBUG_PRINT("Error: File too large. Max size with indirect blocks: %u bytes\n", max_file_size);
    return -1;
  }

  // Cari direktori parent dengan validasi
  uint32_t parent_inode_idx;
  if (!find_dir(request.parent_inode, &parent_inode_idx))
  {
    DEBUG_PRINT("Error: Parent directory not found\n");
    return 2;
  }

  // Buat salinan nama yang aman dengan bound checking
  char name_copy[256];
  memset(name_copy, 0, sizeof(name_copy)); // Clear buffer first

  // Pastikan kita tidak menyalin lebih dari yang tersedia
  size_t copy_len = request.name_len;
  if (copy_len >= sizeof(name_copy))
  {
    copy_len = sizeof(name_copy) - 1;
  }

  memcpy(name_copy, request.name, copy_len);
  name_copy[copy_len] = '\0'; // Pastikan null-terminated

  DEBUG_PRINT("DEBUG: Using filename: '%s' (len=%zu)\n", name_copy, strlen(name_copy));

  // Baca inode direktori parent
  struct EXT2Inode parent_inode;
  read_inode(parent_inode_idx, &parent_inode);

  // Cek apakah file/direktori dengan nama ini sudah ada
  uint32_t existing_inode;
  if (find_inode_in_dir(&parent_inode, name_copy, &existing_inode))
  {
    DEBUG_PRINT("Error: File/directory already exists\n");
    return 1;
  }

  // Alokasikan inode baru
  uint32_t new_inode = allocate_node();
  if (new_inode == 0)
  {
    DEBUG_PRINT("Error: Failed to allocate new inode\n");
    return -1;
  }

  DEBUG_PRINT("DEBUG: Allocated inode %u\n", new_inode);

  // Siapkan struktur inode baru dengan memset untuk keamanan
  struct EXT2Inode new_node;
  memset(&new_node, 0, sizeof(new_node));

  if (request.is_directory)
  {
      // Buat direktori (masih menggunakan cara lama karena direktori kecil)
      new_node.i_mode = EXT2_S_IFDIR;
      new_node.i_size = BLOCK_SIZE;
      new_node.i_blocks = 1;
      new_node.i_block[0] = allocate_block(inode_to_bgd(new_inode));

      // Inisialisasi entri direktori (. dan ..)
      init_directory_table(&new_node, new_inode, request.parent_inode);
  }
  else
  {
    // Buat file dengan dukungan indirect blocks
    new_node.i_mode = EXT2_S_IFREG;
    new_node.i_size = request.buffer_size;

    // GUNAKAN FUNGSI EXTENDED UNTUK ALOKASI BLOK DENGAN DUKUNGAN INDIRECT
    if (request.buffer_size > 0 && request.buf != NULL)
    {
      DEBUG_PRINT("DEBUG: Using extended allocation for file size: %u bytes\n", request.buffer_size);
      allocate_node_blocks_extended(request.buf, &new_node, inode_to_bgd(new_inode));
      DEBUG_PRINT("DEBUG: Extended allocation completed. Blocks allocated: %u\n", new_node.i_blocks);
    }
    else if (request.buffer_size == 0)
    {
      // File kosong
      new_node.i_blocks = 0;
      DEBUG_PRINT("DEBUG: Created empty file\n");
    }
  }

  // Tulis inode ke disk
  sync_node(&new_node, new_inode);

  // Tambahkan entri ke direktori parent dengan nama yang sudah disalin
  add_inode_to_dir(&parent_inode, new_inode, name_copy);

  // Update counter
  uint32_t group = inode_to_bgd(new_inode);
  bgd_table.table[group].bg_free_inodes_count--;
  superblock.s_free_inodes_count--;

  if (request.is_directory)
  {
    bgd_table.table[group].bg_used_dirs_count++;
  }

  // Sinkronisasi semua perubahan
  sync_superblock();

  DEBUG_PRINT("DEBUG: Write operation successful\n");
  return 0;
}

int32_t allocate_block(uint32_t preferred_bgd)
{
  for (uint32_t i = 0; i < GROUPS_COUNT; i++)
  {
    uint32_t group = (preferred_bgd + i) % GROUPS_COUNT;
    struct BlockBuffer buffer;
    read_blocks(&buffer, bgd_table.table[group].bg_block_bitmap, 1);

    for (uint32_t j = 0; j < BLOCK_SIZE * 8; j++)
    {
      uint32_t byte_offset = j / 8;
      uint32_t bit_offset = j % 8;

      if (!(buffer.buf[byte_offset] & (1 << bit_offset)))
      {
        buffer.buf[byte_offset] |= (1 << bit_offset);
        write_blocks(&buffer, bgd_table.table[group].bg_block_bitmap, 1);
        return group * BLOCKS_PER_GROUP + j;
      }
    }
  }
  return -1;
}

void sync_superblock(void)
{
  write_blocks(&superblock, 1, 1);
  write_blocks(&bgd_table, 2, 1);
}

void add_inode_to_dir(struct EXT2Inode *parent_inode, uint32_t inode, const char *name)
{
  // Validasi input
  if (!parent_inode || !name || strlen(name) == 0 || strlen(name) > 255)
  {
    DEBUG_PRINT("Error: Invalid parameters to add_inode_to_dir\n");
    return;
  }

  uint8_t buffer[BLOCK_SIZE];
  read_blocks(buffer, parent_inode->i_block[0], 1);

  // Cari tempat untuk entri baru
  uint32_t offset = 0;
  struct EXT2DirectoryEntry *entry;

  while (offset < BLOCK_SIZE)
  {
    entry = (struct EXT2DirectoryEntry *)(buffer + offset);

    // Jika ini adalah entri terakhir atau kosong
    if (entry->rec_len == 0 || offset + entry->rec_len >= BLOCK_SIZE)
    {
      break;
    }

    offset += entry->rec_len;
  }

  // Hitung ukuran entri baru dengan padding
  uint8_t name_len = strlen(name);
  uint16_t entry_size = sizeof(struct EXT2DirectoryEntry) + name_len;

  // Alignment to 4 bytes
  if (entry_size % 4 != 0)
  {
    entry_size += 4 - (entry_size % 4);
  }

  // Pastikan ada ruang untuk entri baru
  if (offset + entry_size > BLOCK_SIZE)
  {
    DEBUG_PRINT("Error: Not enough space in directory block\n");
    return;
  }

  // Buat entri baru
  struct EXT2DirectoryEntry *new_entry = (struct EXT2DirectoryEntry *)(buffer + offset);
  memset(new_entry, 0, entry_size); // Clear the entry first

  new_entry->inode = inode;
  new_entry->rec_len = BLOCK_SIZE - offset; // Takes rest of the block
  new_entry->name_len = name_len;
  new_entry->file_type = EXT2_FT_REG_FILE; // Assume regular file

  // Salin nama dengan aman
  char *entry_name = (char *)(new_entry + 1);
  memcpy(entry_name, name, name_len);

  // Tulis kembali ke disk
  write_blocks(buffer, parent_inode->i_block[0], 1);

  DEBUG_PRINT("DEBUG: Added directory entry for '%s' with inode %u\n", name, inode);
}

bool is_empty_directory(struct EXT2Inode *inode)
{
  uint8_t buf[BLOCK_SIZE];
  read_blocks(buf, inode->i_block[0], 1);

  uint32_t offset = get_dir_first_child_offset(buf);
  struct EXT2DirectoryEntry *entry = get_directory_entry(buf, offset);

  return entry->inode == 0 || offset >= BLOCK_SIZE;
}

void remove_inode_from_dir(struct EXT2Inode *dir_inode, const char *name)
{
  uint8_t buf[BLOCK_SIZE];
  read_blocks(buf, dir_inode->i_block[0], 1);

  uint32_t offset = 0;
  struct EXT2DirectoryEntry *prev_entry = NULL;
  struct EXT2DirectoryEntry *entry;

  while (offset < BLOCK_SIZE)
  {
    entry = get_directory_entry(buf, offset);
    if (entry->inode != 0)
    {
      char *entry_name = get_entry_name(entry);
      if (strlen(name) == entry->name_len &&
          memcmp(entry_name, name, entry->name_len) == 0)
      {
        if (prev_entry)
          prev_entry->rec_len += entry->rec_len;
        entry->inode = 0;
        break;
      }
    }
    prev_entry = entry;
    offset += entry->rec_len;
  }

  write_blocks(buf, dir_inode->i_block[0], 1);
}

bool find_dir(uint32_t inode, uint32_t *out_inode_idx)
{
  struct EXT2Inode target_inode;
  read_inode(inode, &target_inode);
  if (!is_directory(&target_inode))
  {
    return false;
  }
  *out_inode_idx = inode;
  return true;
}

bool find_inode_in_dir(struct EXT2Inode *dir_inode, const char *name, uint32_t *out_inode)
{
  uint8_t buf[BLOCK_SIZE];
  read_blocks(buf, dir_inode->i_block[0], 1);

  uint32_t offset = 0;
  while (offset < BLOCK_SIZE)
  {
    struct EXT2DirectoryEntry *entry = get_directory_entry(buf, offset);
    if (entry->inode != 0)
    {
      char *entry_name = get_entry_name(entry);
      if (strlen(name) == entry->name_len &&
          memcmp(entry_name, name, entry->name_len) == 0)
      {
        *out_inode = entry->inode;
        return true;
      }
    }
    offset += entry->rec_len;
  }
  return false;
}

bool is_directory(struct EXT2Inode *inode)
{
  return (inode->i_mode & EXT2_S_IFDIR) == EXT2_S_IFDIR;
}

void read_inode(uint32_t inode, struct EXT2Inode *out_inode)
{
  // Add bounds checking
  if (out_inode == NULL)
  {
    DEBUG_PRINT("Error: out_inode is NULL\n");
    return;
  }

  if (inode == 0)
  {
    DEBUG_PRINT("Error: Invalid inode number 0\n");
    return;
  }

  uint32_t group = inode_to_bgd(inode);
  uint32_t local_inode = inode_to_local(inode);

  // Add bounds checking for group and local_inode
  if (group >= GROUPS_COUNT)
  {
    DEBUG_PRINT("Error: Group %u exceeds maximum %u\n", group, GROUPS_COUNT);
    return;
  }

  if (local_inode >= INODES_PER_GROUP)
  {
    DEBUG_PRINT("Error: Local inode %u exceeds maximum %u\n", local_inode, INODES_PER_GROUP);
    return;
  }

  struct EXT2InodeTable inode_table;
  memset(&inode_table, 0, sizeof(inode_table)); // Clear the buffer first

  read_blocks(&inode_table, bgd_table.table[group].bg_inode_table, INODES_TABLE_BLOCK_COUNT);
  *out_inode = inode_table.table[local_inode];
}

void read_inode_data(struct EXT2Inode *inode, void *buf, uint32_t size)
{
    // Redirect ke fungsi extended yang mendukung indirect blocks
    read_inode_data_extended(inode, buf, size);
}

void set_block_free(uint32_t block)
{
  uint32_t group = block / superblock.s_blocks_per_group;
  uint32_t local_block = block % superblock.s_blocks_per_group;
  uint32_t byte_offset = local_block / 8;
  uint32_t bit_offset = local_block % 8;

  struct BlockBuffer buffer;
  read_blocks(&buffer, bgd_table.table[group].bg_block_bitmap, 1);
  buffer.buf[byte_offset] &= ~(1 << bit_offset);
  write_blocks(&buffer, bgd_table.table[group].bg_block_bitmap, 1);
}

const uint8_t fs_signature[BLOCK_SIZE] = {
    'C', 'o', 'u', 'r', 's', 'e', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    'D', 'e', 's', 'i', 'g', 'n', 'e', 'd', ' ', 'b', 'y', ' ', ' ', ' ', ' ', ' ',
    'L', 'a', 'b', ' ', 'S', 'i', 's', 't', 'e', 'r', ' ', 'I', 'T', 'B', ' ', ' ',
    'M', 'a', 'd', 'e', ' ', 'w', 'i', 't', 'h', ' ', '<', '3', ' ', ' ', ' ', ' ',
    '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '2', '0', '2', '5', '\n',
    [BLOCK_SIZE - 2] = 'O',
    [BLOCK_SIZE - 1] = 'k'};

uint32_t inode_to_bgd(uint32_t inode)
{
  return (inode - 1) / INODES_PER_GROUP;
}

uint32_t inode_to_local(uint32_t inode)
{
  return (inode - 1) % INODES_PER_GROUP;
}

// Directory entry helper functions
char *get_entry_name(void *entry)
{
  struct EXT2DirectoryEntry *dir_entry = (struct EXT2DirectoryEntry *)entry;
  return (char *)(dir_entry + 1); // Nama mengikuti struktur directory entry
}

struct EXT2DirectoryEntry *get_directory_entry(void *ptr, uint32_t offset)
{
  return (struct EXT2DirectoryEntry *)((uint8_t *)ptr + offset);
}

struct EXT2DirectoryEntry *get_next_directory_entry(struct EXT2DirectoryEntry *entry)
{
  uint8_t *next = (uint8_t *)entry + entry->rec_len;
  return (struct EXT2DirectoryEntry *)next;
}

uint16_t get_entry_record_len(uint8_t name_len)
{
  return sizeof(struct EXT2DirectoryEntry) + name_len;
}

uint32_t get_dir_first_child_offset(void *ptr)
{
  // Dua entri pertama adalah . dan .. di direktori
  struct EXT2DirectoryEntry *entry = get_directory_entry(ptr, 0);
  if (!entry->inode)
    return BLOCK_SIZE;   // Offset tidak valid
  return entry->rec_len; // Lewati entri '.'
}

// Filesystem operations
bool is_empty_storage(void)
{
  uint8_t buffer[BLOCK_SIZE];
  read_blocks(buffer, 1, 1); // Baca superblock
  struct EXT2Superblock *sb = (struct EXT2Superblock *)buffer;
  return sb->s_magic != EXT2_SUPER_MAGIC;
}

void create_ext2(void)
{
  // Implementasi untuk membuat filesystem EXT2 baru
  // Inisialisasi superblock
  memset(&superblock, 0, sizeof(superblock));
  superblock.s_magic = EXT2_SUPER_MAGIC;

  // Atur nilai penting lainnya
  superblock.s_inodes_count = INODES_PER_GROUP * GROUPS_COUNT;
  superblock.s_blocks_count = BLOCKS_PER_GROUP * GROUPS_COUNT;
  superblock.s_free_blocks_count = superblock.s_blocks_count - 9; // Boot(0), super(1), BGD(2), block_bitmap(3), inode_bitmap(4), inode_table(5-8)
  superblock.s_free_inodes_count = superblock.s_inodes_count - 1; // Root inode will be allocated
  superblock.s_first_data_block = 0;
  superblock.s_log_block_size = 0; // 0 means 1024 byte blocks
  superblock.s_log_frag_size = 0;  // Same as block size
  superblock.s_blocks_per_group = BLOCKS_PER_GROUP;
  superblock.s_frags_per_group = BLOCKS_PER_GROUP; // Same as blocks for simplicity
  superblock.s_inodes_per_group = INODES_PER_GROUP;
  superblock.s_mtime = 0;
  superblock.s_wtime = 0;
  superblock.s_mnt_count = 0;
  superblock.s_max_mnt_count = 20;
  superblock.s_state = 1;  // Clean
  superblock.s_errors = 1; // Continue on errors
  superblock.s_minor_rev_level = 0;
  superblock.s_lastcheck = 0;
  superblock.s_checkinterval = 0;
  superblock.s_creator_os = 0; // Linux
  superblock.s_rev_level = 0;  // Original format
  superblock.s_def_resuid = 0;
  superblock.s_def_resgid = 0;
  superblock.s_first_ino = 11; // First non-reserved inode

  // Tulis superblock
  write_blocks(&superblock, 1, 1);

  // Inisialisasi BGD table
  memset(&bgd_table, 0, sizeof(bgd_table));
  for (uint32_t i = 0; i < GROUPS_COUNT; i++)
  {
    bgd_table.table[i].bg_block_bitmap = 3 + i * (1 + 1 + INODES_TABLE_BLOCK_COUNT);
    bgd_table.table[i].bg_inode_bitmap = 4 + i * (1 + 1 + INODES_TABLE_BLOCK_COUNT);
    bgd_table.table[i].bg_inode_table = 5 + i * (1 + 1 + INODES_TABLE_BLOCK_COUNT);
    bgd_table.table[i].bg_free_blocks_count = BLOCKS_PER_GROUP - 9; // Account for reserved blocks
    bgd_table.table[i].bg_free_inodes_count = INODES_PER_GROUP;
  }

  // Tulis BGD table
  write_blocks(&bgd_table, 2, 1);

  // Inisialisasi block dan inode bitmap
  uint8_t block_bitmap[BLOCK_SIZE] = {0};
  uint8_t inode_bitmap[BLOCK_SIZE] = {0};

  // Mark reserved blocks as used (boot sector, superblock, BGD table, bitmaps, inode table)
  // Blocks 0-8 are reserved for: boot(0), super(1), bgd(2), block_bitmap(3), inode_bitmap(4), inode_table(5-8)
  for (uint32_t i = 0; i < 9; i++)
  {
    uint32_t byte_offset = i / 8;
    uint32_t bit_offset = i % 8;
    block_bitmap[byte_offset] |= (1 << bit_offset);
  }

  for (uint32_t i = 0; i < GROUPS_COUNT; i++)
  {
    write_blocks(block_bitmap, bgd_table.table[i].bg_block_bitmap, 1);
    write_blocks(inode_bitmap, bgd_table.table[i].bg_inode_bitmap, 1);
  }

  // Buat root directory (inode 2)
  struct EXT2Inode root_inode = {0};
  root_inode.i_mode = EXT2_S_IFDIR;
  root_inode.i_size = BLOCK_SIZE;
  root_inode.i_blocks = 1;

  // Alokasi block untuk root directory
  root_inode.i_block[0] = allocate_block(0);

  // Inisialisasi directory table untuk root
  uint8_t dir_data[BLOCK_SIZE] = {0};
  struct EXT2DirectoryEntry *entry = (struct EXT2DirectoryEntry *)dir_data;

  // Entri "."
  entry->inode = 2; // Root inode
  entry->name_len = 1;
  entry->file_type = EXT2_FT_DIR;
  entry->rec_len = 12; // Size of entry + 1 byte name + padding
  char *name = (char *)(entry + 1);
  name[0] = '.';

  // Entri ".."
  entry = (struct EXT2DirectoryEntry *)(dir_data + entry->rec_len);
  entry->inode = 2; // Parent is also root
  entry->name_len = 2;
  entry->file_type = EXT2_FT_DIR;
  entry->rec_len = BLOCK_SIZE - 12; // Rest of the block
  name = (char *)(entry + 1);
  name[0] = '.';
  name[1] = '.';

  // Tulis directory data
  write_blocks(dir_data, root_inode.i_block[0], 1);

  // Tulis root inode
  struct EXT2InodeTable inode_table = {0};
  inode_table.table[1] = root_inode; // Inode 2 adalah index 1 dalam table
  write_blocks(&inode_table, bgd_table.table[0].bg_inode_table, INODES_TABLE_BLOCK_COUNT);

  // Set bitmap untuk root inode dan blok yang digunakan
  set_inode_used(2);

  // Sync
  sync_superblock();
}

void initialize_filesystem_ext2(void)
{
  if (is_empty_storage())
  {
    create_ext2();
  }
  else
  {
    // Baca superblock dan BGD table
    read_blocks(&superblock, 1, 1);
    read_blocks(&bgd_table, 2, 1);
  }
}

uint32_t allocate_node(void)
{
  for (uint32_t i = 0; i < GROUPS_COUNT; i++)
  {
    struct BlockBuffer buffer;
    read_blocks(&buffer, bgd_table.table[i].bg_inode_bitmap, 1);

    for (uint32_t j = 0; j < INODES_PER_GROUP; j++)
    {
      uint32_t byte_offset = j / 8;
      uint32_t bit_offset = j % 8;

      if (!(buffer.buf[byte_offset] & (1 << bit_offset)))
      {
        buffer.buf[byte_offset] |= (1 << bit_offset);
        write_blocks(&buffer, bgd_table.table[i].bg_inode_bitmap, 1);
        return i * INODES_PER_GROUP + j + 1;
      }
    }
  }
  return 0; // Tidak ada inode kosong
}

void sync_node(struct EXT2Inode *node, uint32_t inode)
{
  uint32_t group = inode_to_bgd(inode);
  uint32_t local_inode = inode_to_local(inode);

  struct EXT2InodeTable inode_table;
  read_blocks(&inode_table, bgd_table.table[group].bg_inode_table, INODES_TABLE_BLOCK_COUNT);

  inode_table.table[local_inode] = *node;

  write_blocks(&inode_table, bgd_table.table[group].bg_inode_table, INODES_TABLE_BLOCK_COUNT);
}

void deallocate_node(uint32_t inode)
{
    DEBUG_PRINT("DEBUG: Deallocating inode %u using extended function\n", inode);
    
    // Baca inode
    struct EXT2Inode node;
    read_inode(inode, &node);

    // GUNAKAN FUNGSI EXTENDED UNTUK DEALOKASI DENGAN DUKUNGAN INDIRECT BLOCKS
    deallocate_node_blocks_extended(&node);

    // Tandai inode sebagai tidak terpakai
    clear_inode_used(inode);

    // Update counter free inodes
    uint32_t group = inode_to_bgd(inode);
    bgd_table.table[group].bg_free_inodes_count++;
    superblock.s_free_inodes_count++;
    
    DEBUG_PRINT("DEBUG: Successfully deallocated inode %u\n", inode);
}

void deallocate_block(uint32_t block_num)
{
    if (block_num == 0) return; // Tidak ada yang perlu didealokasi
    
    // Tentukan block group dari nomor blok
    uint32_t group = (block_num - superblock.s_first_data_block) / superblock.s_blocks_per_group;
    uint32_t local_block = (block_num - superblock.s_first_data_block) % superblock.s_blocks_per_group;
    
    // Hitung offset byte dan bit dalam bitmap
    uint32_t byte_offset = local_block / 8;
    uint32_t bit_offset = local_block % 8;
    
    // Baca block bitmap
    struct BlockBuffer buffer;
    read_blocks(&buffer, bgd_table.table[group].bg_block_bitmap, 1);
    
    // Clear bit untuk menandai blok sebagai tidak terpakai
    buffer.buf[byte_offset] &= ~(1 << bit_offset);
    
    // Tulis kembali bitmap yang sudah diupdate
    write_blocks(&buffer, bgd_table.table[group].bg_block_bitmap, 1);
    
    // Update counter blok bebas di block group descriptor
    bgd_table.table[group].bg_free_blocks_count++;
    
    // Update counter blok bebas di superblock
    superblock.s_free_blocks_count++;
    
    DEBUG_PRINT("Deallocated block %u in group %u\n", block_num, group);
}

/**
 * @brief Dealokasi indirect block dan semua blok yang direferensikan
 */
void deallocate_indirect_block(uint32_t indirect_block, uint32_t level)
{
    if (indirect_block == 0) return;
    
    const uint32_t ptrs_per_block = BLOCK_SIZE / sizeof(uint32_t);
    uint32_t indirect_table[ptrs_per_block];
    
    // Baca indirect table
    read_blocks(indirect_table, indirect_block, 1);
    
    // Dealokasi semua blok yang direferensikan
    for (uint32_t i = 0; i < ptrs_per_block; i++) {
        if (indirect_table[i] != 0) {
            if (level > 1) {
                // Recursive dealokasi untuk double/triple indirect
                deallocate_indirect_block(indirect_table[i], level - 1);
            } else {
                // Dealokasi blok data langsung
                deallocate_block(indirect_table[i]);
            }
        }
    }
    
    // Dealokasi indirect block itu sendiri
    deallocate_block(indirect_block);
}

/**
 * @brief Dealokasi semua blok yang digunakan oleh inode
 */
void deallocate_inode_blocks(struct EXT2Inode *inode)
{
    if (inode == NULL) return;
    
    uint32_t total_blocks = ceil_div(inode->i_size, BLOCK_SIZE);
    const uint32_t ptrs_per_block = BLOCK_SIZE / sizeof(uint32_t);
    
    // Dealokasi direct blocks (0-11)
    for (uint32_t i = 0; i < 12 && i < total_blocks; i++) {
        if (inode->i_block[i] != 0) {
            deallocate_block(inode->i_block[i]);
            inode->i_block[i] = 0;
        }
    }
    
    // Dealokasi single indirect block (12)
    if (total_blocks > 12 && inode->i_block[12] != 0) {
        deallocate_indirect_block(inode->i_block[12], 1);
        inode->i_block[12] = 0;
    }
    
    // Dealokasi double indirect block (13)
    if (total_blocks > 12 + ptrs_per_block && inode->i_block[13] != 0) {
        deallocate_indirect_block(inode->i_block[13], 2);
        inode->i_block[13] = 0;
    }
    
    // Dealokasi triple indirect block (14) - jika diperlukan
    if (total_blocks > 12 + ptrs_per_block + (ptrs_per_block * ptrs_per_block) && inode->i_block[14] != 0) {
        deallocate_indirect_block(inode->i_block[14], 3);
        inode->i_block[14] = 0;
    }
    
    // Reset block count
    inode->i_blocks = 0;
    inode->i_size = 0;
    
    DEBUG_PRINT("Deallocated all blocks for inode\n");
}

/**
 * @brief Dealokasi blok logis tertentu dari inode (untuk truncate/resize)
 */
void deallocate_logical_block_range(struct EXT2Inode *inode, uint32_t start_logical_block, uint32_t end_logical_block)
{
    const uint32_t ptrs_per_block = BLOCK_SIZE / sizeof(uint32_t);
    
    for (uint32_t logical_idx = start_logical_block; logical_idx <= end_logical_block; logical_idx++) {
        uint32_t physical_block = get_physical_block_from_logical(inode, logical_idx);
        
        if (physical_block != 0) {
            deallocate_block(physical_block);
            
            // Clear referensi di struktur inode
            if (logical_idx < 12) {
                // Direct block
                inode->i_block[logical_idx] = 0;
            }
            else if (logical_idx < 12 + ptrs_per_block) {
                // Single indirect block
                if (inode->i_block[12] != 0) {
                    uint32_t indirect_table[ptrs_per_block];
                    read_blocks(indirect_table, inode->i_block[12], 1);
                    
                    uint32_t indirect_idx = logical_idx - 12;
                    indirect_table[indirect_idx] = 0;
                    
                    write_blocks(indirect_table, inode->i_block[12], 1);
                    
                    // Cek apakah semua entry dalam indirect table kosong
                    bool all_empty = true;
                    for (uint32_t i = 0; i < ptrs_per_block; i++) {
                        if (indirect_table[i] != 0) {
                            all_empty = false;
                            break;
                        }
                    }
                    
                    // Jika semua entry kosong, dealokasi indirect block juga
                    if (all_empty) {
                        deallocate_block(inode->i_block[12]);
                        inode->i_block[12] = 0;
                    }
                }
            }
            else if (logical_idx < 12 + ptrs_per_block + (ptrs_per_block * ptrs_per_block)) {
                // Double indirect block - implementasi similar tapi lebih kompleks
                // Untuk kesederhanaan, implementasi dasar tanpa optimasi pembersihan
                // indirect table yang kosong
            }
        }
    }
    
    // Update block count (perkiraan sederhana)
    uint32_t remaining_blocks = 0;
    for (uint32_t i = 0; i < 15; i++) {
        if (inode->i_block[i] != 0) {
            if (i < 12) {
                remaining_blocks++;
            } else {
                // Untuk indirect blocks, perlu perhitungan lebih detail
                // Ini implementasi sederhana
                remaining_blocks += ceil_div(inode->i_size, BLOCK_SIZE) - start_logical_block;
                break;
            }
        }
    }
    inode->i_blocks = remaining_blocks * (BLOCK_SIZE / 512); // i_blocks dalam unit 512-byte
}

/**
 * @brief Helper function untuk mengecek apakah blok sudah dialokasi
 */
bool is_block_allocated(uint32_t block_num)
{
    if (block_num == 0) return false;
    
    uint32_t group = (block_num - superblock.s_first_data_block) / superblock.s_blocks_per_group;
    uint32_t local_block = (block_num - superblock.s_first_data_block) % superblock.s_blocks_per_group;
    
    uint32_t byte_offset = local_block / 8;
    uint32_t bit_offset = local_block % 8;
    
    struct BlockBuffer buffer;
    read_blocks(&buffer, bgd_table.table[group].bg_block_bitmap, 1);
    
    return (buffer.buf[byte_offset] & (1 << bit_offset)) != 0;
}

int8_t delete(struct EXT2DriverRequest request)
{
  uint32_t inode_idx;
  if (!find_dir(request.parent_inode, &inode_idx))
    return 3;

  struct EXT2Inode dir_inode;
  read_inode(inode_idx, &dir_inode);

  uint32_t target_inode;
  if (!find_inode_in_dir(&dir_inode, request.name, &target_inode))
    return 1;

  struct EXT2Inode file_inode;
  read_inode(target_inode, &file_inode);

  if (request.is_directory)
  {
    // Cek apakah direktori kosong
    if (!is_empty_directory(&file_inode))
      return 2;
  }

  // Hapus dari parent directory
  remove_inode_from_dir(&dir_inode, request.name);

  // Dealokasi node
  deallocate_node(target_inode);

  sync_superblock();
  return 0;
}

int8_t read_directory(struct EXT2DriverRequest *request)
{
  // Validasi input
  if (request == NULL || request->name_len == 0)
  {
    return -1;
  }

  // Cari direktori parent
  uint32_t parent_inode_idx;
  if (!find_dir(request->parent_inode, &parent_inode_idx))
  {
    return 3; // Parent directory not found
  }

  // Buat salinan nama untuk mencegah masalah
  char name_copy[256];
  memcpy(name_copy, request->name, request->name_len);
  name_copy[request->name_len] = '\0'; // Pastikan null-terminated

  // Baca inode direktori parent
  struct EXT2Inode parent_inode;
  read_inode(parent_inode_idx, &parent_inode);

  // Cari inode di direktori
  uint32_t target_inode;
  if (!find_inode_in_dir(&parent_inode, name_copy, &target_inode))
  {
    return 4; // Directory entry not found
  }

  // Baca inode target
  struct EXT2Inode target;
  read_inode(target_inode, &target);

  // Verifikasi bahwa ini adalah direktori
  if (!is_directory(&target))
  {
    return 5; // Not a directory
  }

  return 0; // Success
}

int8_t read(struct EXT2DriverRequest request)
{
  DEBUG_PRINT("DEBUG read: Entering function\n");
  DEBUG_PRINT("DEBUG read: parent_inode=%u, name='%s', buffer_size=%u\n", 
              request.parent_inode, request.name, request.buffer_size);

  // Validasi input
  if (request.name[0] == '\0')
  {
    DEBUG_PRINT("Error: Empty filename\n");
    return -1;
  }

  if (request.buffer_size == 0)
  {
    DEBUG_PRINT("Error: Invalid buffer or buffer size\n");
    return -1;
  }

  // Cari inode direktori parent
  uint32_t parent_inode_idx = request.parent_inode;
  struct EXT2Inode parent_inode;
  
  // Baca inode parent
  read_inode(parent_inode_idx, &parent_inode);
  
  // Verifikasi bahwa parent adalah direktori
  if (!is_directory(&parent_inode))
  {
    DEBUG_PRINT("Error: Parent is not a directory\n");
    return 3;
  }

  // Buat salinan nama yang aman
  char name_copy[256];
  size_t name_len = strlen(request.name);
  if (name_len >= sizeof(name_copy))
  {
    name_len = sizeof(name_copy) - 1;
  }
  memcpy(name_copy, request.name, name_len);
  name_copy[name_len] = '\0';

  // Cari entry file di direktori parent
  uint32_t found_inode = 0;
  if (!find_inode_in_dir(&parent_inode, name_copy, &found_inode))
  {
    DEBUG_PRINT("Error: File not found: '%s'\n", name_copy);
    return 3; // File not found
  }

  DEBUG_PRINT("DEBUG: Found file with inode: %u\n", found_inode);

  // Baca inode file
  struct EXT2Inode file_inode;
  read_inode(found_inode, &file_inode);

  // Jika direktori, return error
  if (is_directory(&file_inode))
  {
    DEBUG_PRINT("Error: Target is a directory, not a file\n");
    return 1; // Is directory
  }

  // Cek apakah buffer cukup besar
  if (file_inode.i_size > request.buffer_size)
  {
    DEBUG_PRINT("Error: Buffer too small. File size: %u, Buffer size: %u\n", 
                file_inode.i_size, request.buffer_size);
    return 2; // Buffer too small
  }

  // GUNAKAN FUNGSI EXTENDED UNTUK MEMBACA DATA DENGAN DUKUNGAN INDIRECT BLOCKS
  DEBUG_PRINT("DEBUG: Reading file data using extended function. File size: %u bytes\n", file_inode.i_size);
  
  if (file_inode.i_size > 0)
  {
    read_inode_data_extended(&file_inode, request.buf, file_inode.i_size);
    DEBUG_PRINT("DEBUG: Successfully read %u bytes using extended read function\n", file_inode.i_size);
  }
  else
  {
    DEBUG_PRINT("DEBUG: File is empty, no data to read\n");
  }

  return 0; // Success
}