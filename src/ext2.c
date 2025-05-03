
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "header/stdlib/string.h"
#include "header/filesystem/ext2.h"

static struct EXT2Superblock superblock;
struct EXT2BlockGroupDescriptorTable bgd_table;

const uint8_t fs_signature[BLOCK_SIZE] = {
    'C',
    'o',
    'u',
    'r',
    's',
    'e',
    ' ',
    ' ',
    ' ',
    ' ',
    ' ',
    ' ',
    ' ',
    ' ',
    ' ',
    ' ',
    'D',
    'e',
    's',
    'i',
    'g',
    'n',
    'e',
    'd',
    ' ',
    'b',
    'y',
    ' ',
    ' ',
    ' ',
    ' ',
    ' ',
    'L',
    'a',
    'b',
    ' ',
    'S',
    'i',
    's',
    't',
    'e',
    'r',
    ' ',
    'I',
    'T',
    'B',
    ' ',
    ' ',
    'M',
    'a',
    'd',
    'e',
    ' ',
    'w',
    'i',
    't',
    'h',
    ' ',
    '<',
    '3',
    ' ',
    ' ',
    ' ',
    ' ',
    '-',
    '-',
    '-',
    '-',
    '-',
    '-',
    '-',
    '-',
    '-',
    '-',
    '-',
    '2',
    '0',
    '2',
    '5',
    '\n',
    [BLOCK_SIZE - 2] = 'O',
    [BLOCK_SIZE - 1] = 'k',
};

/* =================== MAIN FUNCTION OF EXT32 FILESYSTEM ============================*/

/**
 * @brief get bgd index from inode, inode will starts at index 1
 * @param inode 1 to INODES_PER_GROUP * GROUP_COUNT
 * @return bgd index (0 to GROUP_COUNT - 1)
 */
uint32_t inode_to_bgd(uint32_t inode)
{
  return (inode - 1) / INODES_PER_GROUP;
}

/**
 * @brief get inode local index in the corrresponding bgd
 * @param inode 1 to INODES_PER_GROUP * GROUP_COUNT
 * @return local index
 */
uint32_t inode_to_local(uint32_t inode)
{
  return (inode - 1) % INODES_PER_GROUP;
}

/**
 * @brief create a new directory using given node
 * first item of directory table is its node location (name will be .)
 * second item of directory is its parent location (name will be ..)
 * @param node pointer of inode
 * @param inode inode that already allocated
 * @param parent_inode inode of parent directory (if root directory, the parent is itself)
 */
void init_directory_table(struct EXT2Inode *node, uint32_t inode, uint32_t parent_inode)
{
  uint8_t buf[BLOCK_SIZE];
  memset(buf, 0, BLOCK_SIZE);

  struct EXT2DirectoryEntry *self = (struct EXT2DirectoryEntry *)buf;
  self->inode = inode;
  self->name_len = 1;
  self->file_type = EXT2_FT_DIR;
  self->rec_len = get_entry_record_len(1);
  char *self_name = get_entry_name(self);
  self_name[0] = '.';

  struct EXT2DirectoryEntry *parent = get_next_directory_entry(self);
  parent->inode = parent_inode;
  parent->name_len = 2;
  parent->file_type = EXT2_FT_DIR;
  parent->rec_len = BLOCK_SIZE - self->rec_len;
  char *parent_name = get_entry_name(parent);
  parent_name[0] = '.';
  parent_name[1] = '.';

  write_blocks(node->i_block[0], 1, buf);

  return true;
}
/**
 * @brief check whether filesystem signature is missing or not in boot sector
 *
 * @return true if memcmp(boot_sector, fs_signature) returning inequality
 */
bool is_empty_storage(void)
{
  char boot_sector[BLOCK_SIZE];
  read_blocks(BOOT_SECTOR, 1, boot_sector);
  return memcmp(boot_sector, fs_signature, sizeof(fs_signature)) != 0;
}

/**
 * @brief create a new EXT2 filesystem. Will write fs_signature into boot sector,
 * initialize super block, bgd table, block and inode bitmap, and create root directory
 */
void create_ext2(void)
{
  char boot_sector[BLOCK_SIZE];
  memset(boot_sector, 0, BLOCK_SIZE);
  memcpy(boot_sector, fs_signature, sizeof(fs_signature));
  write_blocks(BOOT_SECTOR, 1, boot_sector);

  //struct EXT2Superblock superblock;

  // Init superblock
  superblock.s_inodes_count = INODES_PER_GROUP * GROUPS_COUNT;
  superblock.s_blocks_count = (DISK_SPACE / BLOCK_SIZE);
  superblock.s_r_blocks_count = 0;
  superblock.s_free_blocks_count = superblock.s_blocks_count - 3; // boot + sb + bgdt
  superblock.s_free_inodes_count = superblock.s_inodes_count - 1; // root already used
  superblock.s_first_data_block = 1;
  superblock.s_first_ino = 1;
  superblock.s_blocks_per_group = BLOCKS_PER_GROUP;
  superblock.s_frags_per_group = BLOCKS_PER_GROUP;
  superblock.s_inodes_per_group = INODES_PER_GROUP;
  superblock.s_magic = EXT2_SUPER_MAGIC;
  superblock.s_prealloc_blocks = 0;
  superblock.s_prealloc_dir_blocks = 0;

  write_blocks(1, 1, &superblock);

  // Init Block Group Descriptor Table
  memset(&bgd_table, 0, sizeof(bgd_table));
  write_blocks(2, 1, &bgd_table);

  // Root directory
  struct EXT2Inode root_inode;
  memset(&root_inode, 0, sizeof(struct EXT2Inode));
  root_inode.i_mode = EXT2_S_IFDIR;
  root_inode.i_size = BLOCK_SIZE;
  root_inode.i_blocks = 1;
  root_inode.i_block[0] = 3; // Assume block 3 kosong untuk root directory
  init_directory_table(&root_inode, 1, 1);

  // Write root inode
  struct EXT2InodeTable inode_table;
  memset(&inode_table, 0, sizeof(inode_table));
  inode_table.table[0] = root_inode;
  write_blocks(4, INODES_TABLE_BLOCK_COUNT, &inode_table);
}

/**
 * @brief Initialize file system driver state, if is_empty_storage() then create_ext2()
 * Else, read and cache super block (located at block 1) and bgd table (located at block 2) into state
 */
void initialize_filesystem_ext2(void)
{
  if (is_empty_storage())
  {
    create_ext2();
  }
  else
  {
    read_blocks(1, 1, &superblock);
    read_blocks(2, 1, &bgd_table);
  }
}

/**
 * @brief check whether a directory table has children or not
 * @param inode of a directory table
 * @return true if first_child_entry->inode = 0
 */
bool is_directory_empty(uint32_t inode)
{
  struct EXT2InodeTable inode_table;
  read_blocks(4, INODES_TABLE_BLOCK_COUNT, &inode_table);
  struct EXT2Inode *dir_inode = &inode_table.table[inode_to_local(inode)];

  uint8_t buf[BLOCK_SIZE];
  read_blocks(dir_inode->i_block[0], 1, buf);

  uint32_t first_child_offset = get_dir_first_child_offset(buf);
  struct EXT2DirectoryEntry *first_child = get_directory_entry(buf, first_child_offset);

  return first_child->inode == 0;
}

/* =============================== CRUD FUNC ======================================== */

/**
 * @brief EXT2 Folder / Directory read
 * @param request buf point to struct EXT2 Directory
 * @return Error code: 0 success - 1 not a folder - 2 not found - 3 parent folder invalid - -1 unknown
 */
int8_t read_directory(struct EXT2DriverRequest *prequest)
{
  struct EXT2InodeTable inode_table;
  read_blocks(4, INODES_TABLE_BLOCK_COUNT, &inode_table);

  struct EXT2Inode *dir_inode = &inode_table.table[inode_to_local(prequest->parent_inode)];

  if ((dir_inode->i_mode & EXT2_S_IFDIR) == 0)
    return 1; // Not a folder

  uint8_t buf[BLOCK_SIZE];
  read_blocks(dir_inode->i_block[0], 1, buf);

  uint32_t offset = 0;
  while (offset < BLOCK_SIZE)
  {
    struct EXT2DirectoryEntry *entry = get_directory_entry(buf, offset);
    if (entry->inode != 0)
    {
      char *entry_name = get_entry_name(entry);
      if (entry->name_len == prequest->name_len &&
          memcmp(entry_name, prequest->name, entry->name_len) == 0)
      {
        struct EXT2Inode *target_inode = &inode_table.table[inode_to_local(entry->inode)];

        if ((target_inode->i_mode & EXT2_S_IFDIR) == 0)
          return 1;

        if (prequest->buffer_size < BLOCK_SIZE)
          return -1;

        memcpy(prequest->buf, buf, BLOCK_SIZE);
        return 0;
      }
    }
    offset += entry->rec_len;
  }
  return 2;
}

/**
 * @brief EXT2 read, read a file from file system
 * @param request All attribute will be used except is_dir for read, buffer_size will limit reading count
 * @return Error code: 0 success - 1 not a file - 2 not enough buffer - 3 not found - 4 parent folder invalid - -1 unknown
 */
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

/**
 * @brief EXT2 write, write a file or a folder to file system
 *
 * @param All attribute will be used for write except is_dir, buffer_size == 0 then create a folder / directory. It is possible that exist file with name same as a folder
 * @return Error code: 0 success - 1 file/folder already exist - 2 invalid parent folder - -1 unknown
 */
int8_t write(struct EXT2DriverRequest *request)
{
  uint32_t parent_inode_idx;
  if (!find_dir(request->parent_inode, &parent_inode_idx))
    return 2;

  struct EXT2Inode parent_inode;
  read_inode(parent_inode_idx, &parent_inode);

  uint32_t temp;
  if (find_inode_in_dir(&parent_inode, request->name, &temp))
    return 1;

  uint32_t inode_idx = allocate_node();
  struct EXT2Inode new_inode;
  memset(&new_inode, 0, sizeof(new_inode));

  if (request->buffer_size > 0)
  {
    new_inode.i_mode = EXT2_S_IFREG;
    new_inode.i_size = request->buffer_size;
    new_inode.i_blocks = ceil_div(request->buffer_size, BLOCK_SIZE);
    allocate_node_blocks(request->buf, &new_inode, inode_idx / INODES_PER_GROUP);
  }
  else
  {
    new_inode.i_mode = EXT2_S_IFDIR;
    new_inode.i_size = BLOCK_SIZE;
    new_inode.i_blocks = 1;
    // Inisialisasi direktori kosong (mengandung "." dan "..")
    init_directory_table(&new_inode, inode_idx, parent_inode_idx);
  }

  sync_node(&new_inode, inode_idx);
  add_inode_to_dir(&parent_inode, inode_idx, request->name);
  sync_node(&parent_inode, parent_inode_idx);
  return 0;
}

/**
 * @brief EXT2 delete, delete a file or empty directory in file system
 *  @param request buf and buffer_size is unused, is_dir == true means delete folder (possible file with name same as folder)
 * @return Error code: 0 success - 1 not found - 2 folder is not empty - 3 parent folder invalid -1 unknown
 */
int8_t delete(struct EXT2DriverRequest request)
{
  uint32_t parent_inode_idx;
  if (!find_dir(request.parent_inode, &parent_inode_idx))
    return 3;

  struct EXT2Inode parent_inode;
  read_inode(parent_inode_idx, &parent_inode);

  uint32_t target_inode_idx;
  if (!find_inode_in_dir(&parent_inode, request.name, &target_inode_idx))
    return 1;

  struct EXT2Inode target_inode;
  read_inode(target_inode_idx, &target_inode);

  if (request.is_directory && !is_directory(&target_inode))
    return 1;
  if (request.is_directory && !is_empty_directory(&target_inode))
    return 2;

  deallocate_node(target_inode_idx);
  remove_inode_from_dir(&parent_inode, request.name);
  sync_node(&parent_inode, parent_inode_idx);
  return 0;
}

/* =============================== MEMORY ==========================================*/

/**
 * @brief get a free inode from the disk, assuming it is always
 * available
 * @return new inode
 */
uint32_t allocate_node(void)
{
  //struct EXT2Superblock superblock;
  for (uint32_t i = 0; i < superblock.s_inodes_count; i++)
  {
    if (!is_inode_used(i))
    {
      set_inode_used(i);
      superblock.s_free_inodes_count--;
      sync_superblock();
      return i;
    }
  }
  return (uint32_t)-1; // seharusnya tidak terjadi
}

/**
 * @brief deallocate node from the disk, will also deallocate its used blocks
 * also all of the blocks of indirect blocks if necessary
 * @param inode that needs to be deallocated
 */
void deallocate_node(uint32_t inode)
{
  //struct EXT2Superblock superblock;
  struct EXT2Inode node;
  read_inode(inode, &node);
  deallocate_blocks(node.i_block, node.i_blocks);
  clear_inode_used(inode);
  superblock.s_free_inodes_count++;
  sync_superblock();
}

/**
 * @brief deallocate node blocks
 * @param locations node->block
 * @param blocks number of blocks
 */
void deallocate_blocks(void *loc, uint32_t blocks)
{
  uint32_t *blocks_arr = (uint32_t *)loc;
  uint32_t dummy = 0;
  for (uint32_t i = 0; i < blocks; i++)
  {
    deallocate_block(&blocks_arr[i], 1, NULL, 0, &dummy, false);
  }
}

/**
 * @brief deallocate block from the disk
 * @param locations block locations
 * @param blocks number of blocks
 * @param bitmap block bitmap
 * @param depth depth of the block
 * @param last_bgd last bgd that is used
 * @param bgd_loaded whether bgd is loaded or not
 * @return new last bgd
 */
uint32_t deallocate_block(uint32_t *locations, uint32_t blocks,
                          struct BlockBuffer *bitmap, uint32_t depth,
                          uint32_t *last_bgd, bool bgd_loaded)
{
  //struct EXT2Superblock superblock;
  for (uint32_t i = 0; i < blocks; i++)
  {
    uint32_t block = locations[i];
    if (block)
    {
      set_block_free(block);
      superblock.s_free_blocks_count++;
    }
  }
  sync_superblock();
  return *last_bgd;
}

/**
 * @brief write node->block in the given node, will allocate
 * at least node->blocks number of blocks, if first 12 item of node-> block
 * is not enough, will use indirect blocks
 * @param ptr the buffer that needs to be written
 * @param node pointer of the node
 * @param preffered_bgd it is located at the node inode bgd
 *
 * @attention only implement until doubly indirect block, if you want to implement triply indirect block please increase the storage size to at least 256MB
 */
void allocate_node_blocks(void *ptr, struct EXT2Inode *node, uint32_t prefered_bgd)
{
  //struct EXT2Superblock superblock;
  uint32_t block_needed = ceil_div(node->i_size, BLOCK_SIZE);
  uint8_t *data = (uint8_t *)ptr;

  for (uint32_t i = 0; i < block_needed && i < 12; i++)
  {
    node->i_block[i] = allocate_block(prefered_bgd);
    write_blocks(node->i_block[i], 1, data + i * BLOCK_SIZE);
    superblock.s_free_blocks_count--;
  }
  sync_superblock();
  // Belum handle indirect/double indirect (TODO)
}

/**
 * @brief update the node to the disk
 * @param node pointer of node
 * @param inode location of the node
 */
void sync_node(struct EXT2Inode *node, uint32_t inode)
{
  uint32_t group = inode / INODES_PER_GROUP;
  uint32_t offset = inode % INODES_PER_GROUP;
  struct EXT2InodeTable table;
  read_blocks(bgd_table.table[group].bg_inode_table, INODES_TABLE_BLOCK_COUNT, &table);
  table.table[offset] = *node;
  write_blocks(bgd_table.table[group].bg_inode_table, INODES_TABLE_BLOCK_COUNT, &table);
}
