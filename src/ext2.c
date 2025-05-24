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
  return (a + b - 1) / b;
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

// Tambahkan sebelum fungsi write()

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
  // Validasi input
  DEBUG_PRINT("DEBUG write: Entering function\n");
  DEBUG_PRINT("DEBUG write: name_len=%u, buffer_size=%u\n",
              request.name_len, request.buffer_size);

  // Perbaikan: Validasi name dengan lebih tepat
  if (request.name_len == 0 || request.name_len >= 254)
  {
    DEBUG_PRINT("Error: Invalid filename length\n");
    return -1;
  }

  // Pastikan name tidak kosong dengan memeriksa karakter pertama
  if (request.name[0] == '\0')
  {
    DEBUG_PRINT("Error: Empty filename\n");
    return -1;
  }

  if (request.buffer_size > 0 && request.buf == NULL)
  {
    DEBUG_PRINT("Error: Buffer is NULL but size is non-zero\n");
    return -1;
  }

  // Cari direktori parent
  uint32_t parent_inode_idx;
  if (!find_dir(request.parent_inode, &parent_inode_idx))
  {
    DEBUG_PRINT("Error: Parent directory not found\n");
    return 2;
  }

  // Buat salinan nama untuk mencegah masalah
  char name_copy[256] = {0};
  memcpy(name_copy, request.name, request.name_len);
  name_copy[request.name_len] = '\0'; // Pastikan null-terminated

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

  // Batasi ukuran file untuk mencegah masalah
  uint32_t buffer_size = request.buffer_size;
  if (buffer_size > 12 * BLOCK_SIZE)
  {
    DEBUG_PRINT("Warning: File size limited to %u bytes\n", 12 * BLOCK_SIZE);
    buffer_size = 12 * BLOCK_SIZE;
  }

  // Alokasikan inode baru
  uint32_t new_inode = allocate_node();
  if (new_inode == 0)
  {
    DEBUG_PRINT("Error: Failed to allocate new inode\n");
    return -1;
  }

  DEBUG_PRINT("DEBUG: Allocated inode %u\n", new_inode);

  // Siapkan struktur inode baru
  struct EXT2Inode new_node;
  memset(&new_node, 0, sizeof(new_node));

  if (request.is_directory)
  {
    // Buat direktori
    new_node.i_mode = EXT2_S_IFDIR;
    new_node.i_size = BLOCK_SIZE;
    new_node.i_blocks = 1;
    new_node.i_block[0] = allocate_block(inode_to_bgd(new_inode));

    // Inisialisasi entri direktori (. dan ..)
    init_directory_table(&new_node, new_inode, request.parent_inode);
  }
  else
  {
    // Buat file
    new_node.i_mode = EXT2_S_IFREG;
    new_node.i_size = buffer_size;
    new_node.i_blocks = ceil_div(buffer_size, BLOCK_SIZE);

    // Alokasikan blok dan tulis data
    if (buffer_size > 0)
    {
      allocate_node_blocks(request.buf, &new_node, inode_to_bgd(new_inode));
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

void add_inode_to_dir(struct EXT2Inode *dir_inode, uint32_t inode, const char *name)
{
  uint8_t buf[BLOCK_SIZE];
  read_blocks(buf, dir_inode->i_block[0], 1);

  uint32_t offset = 0;
  struct EXT2DirectoryEntry *entry;

  while (offset < BLOCK_SIZE)
  {
    entry = get_directory_entry(buf, offset);
    if (entry->inode == 0 || offset + entry->rec_len >= BLOCK_SIZE)
      break;
    offset += entry->rec_len;
  }

  entry->inode = inode;
  entry->name_len = strlen(name);
  entry->file_type = EXT2_FT_REG_FILE;
  entry->rec_len = BLOCK_SIZE - offset;

  char *entry_name = get_entry_name(entry);
  memcpy(entry_name, name, entry->name_len);

  write_blocks(buf, dir_inode->i_block[0], 1);
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
  uint32_t group = inode_to_bgd(inode);
  uint32_t local_inode = inode_to_local(inode);

  struct EXT2InodeTable inode_table;
  read_blocks(&inode_table, bgd_table.table[group].bg_inode_table, INODES_TABLE_BLOCK_COUNT);
  *out_inode = inode_table.table[local_inode];
}

void read_inode_data(struct EXT2Inode *inode, void *buf, uint32_t size)
{
  if (inode == NULL || buf == NULL || size == 0)
    return;
  uint32_t bytes_read = 0;
  uint32_t block_idx = 0;

  while (bytes_read < size && block_idx < inode->i_blocks)
  {
    uint8_t block_buf[BLOCK_SIZE];
    read_blocks(block_buf, inode->i_block[block_idx], 1);

    uint32_t to_read = size - bytes_read;
    if (to_read > BLOCK_SIZE)
      to_read = BLOCK_SIZE;

    memcpy((uint8_t *)buf + bytes_read, block_buf, to_read);
    bytes_read += to_read;
    block_idx++;
  }
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
// Tambahkan implementasi fungsi-fungsi yang hilang:

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
  superblock.s_free_blocks_count = superblock.s_blocks_count - 3; // Boot, superblock, BGD
  superblock.s_free_inodes_count = superblock.s_inodes_count - 1; // Root inode
  superblock.s_first_data_block = 0;
  superblock.s_blocks_per_group = BLOCKS_PER_GROUP;
  superblock.s_inodes_per_group = INODES_PER_GROUP;

  // Tulis superblock
  write_blocks(&superblock, 1, 1);

  // Inisialisasi BGD table
  memset(&bgd_table, 0, sizeof(bgd_table));
  for (uint32_t i = 0; i < GROUPS_COUNT; i++)
  {
    bgd_table.table[i].bg_block_bitmap = 3 + i * (1 + 1 + INODES_TABLE_BLOCK_COUNT);
    bgd_table.table[i].bg_inode_bitmap = 4 + i * (1 + 1 + INODES_TABLE_BLOCK_COUNT);
    bgd_table.table[i].bg_inode_table = 5 + i * (1 + 1 + INODES_TABLE_BLOCK_COUNT);
    bgd_table.table[i].bg_free_blocks_count = BLOCKS_PER_GROUP;
    bgd_table.table[i].bg_free_inodes_count = INODES_PER_GROUP;
  }

  // Tulis BGD table
  write_blocks(&bgd_table, 2, 1);

  // Inisialisasi block dan inode bitmap
  uint8_t bitmap[BLOCK_SIZE] = {0};
  for (uint32_t i = 0; i < GROUPS_COUNT; i++)
  {
    write_blocks(bitmap, bgd_table.table[i].bg_block_bitmap, 1);
    write_blocks(bitmap, bgd_table.table[i].bg_inode_bitmap, 1);
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

// Tambahkan sebelum fungsi deallocate_node()

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
  // Baca inode
  struct EXT2Inode node;
  read_inode(inode, &node);

  // Dealokasi blok data yang digunakan
  for (uint32_t i = 0; i < node.i_blocks && i < 12; i++)
  {
    if (node.i_block[i] != 0)
    {
      set_block_free(node.i_block[i]);
    }
  }

  // Tandai inode sebagai tidak terpakai
  clear_inode_used(inode);

  // Update counter free inodes
  uint32_t group = inode_to_bgd(inode);
  bgd_table.table[group].bg_free_inodes_count++;
  superblock.s_free_inodes_count++;
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

// Tambahkan implementasi fungsi ini di src/ext2.c, sebaiknya di akhir file:

int8_t read_directory(struct EXT2DriverRequest *request)
{
  // Validasi input
  if (request == NULL || request->name_len == 0)
  {
    // Hapus pengecekan request->name == NULL karena name adalah array
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
  uint32_t inode_idx;
  if (!find_dir(request.parent_inode, &inode_idx))
    return 4;

  struct EXT2Inode dir_inode;
  read_inode(inode_idx, &dir_inode);

  uint32_t target_inode;
  if (!find_inode_in_dir(&dir_inode, request.name, &target_inode))
    return 3;

  struct EXT2Inode file_inode;
  read_inode(target_inode, &file_inode);

  if (is_directory(&file_inode))
    return 1;
  if (file_inode.i_size > request.buffer_size)
    return 2;

  read_inode_data(&file_inode, request.buf, request.buffer_size);
  return 0;
}