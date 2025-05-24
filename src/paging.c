#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "header/memory/paging.h"
#include "header/stdlib/string.h"

__attribute__((aligned(0x1000))) static struct PageDirectory page_directory_list[PAGING_DIRECTORY_TABLE_MAX_COUNT] = {0};

static struct
{
  bool page_directory_used[PAGING_DIRECTORY_TABLE_MAX_COUNT];
  int page_dir_free;
} page_directory_manager = {
    .page_directory_used = {false},
    .page_dir_free = PAGING_DIRECTORY_TABLE_MAX_COUNT,
};

__attribute__((aligned(0x1000))) struct PageDirectory _paging_kernel_page_directory = {
    .table = {
        [0] = {
            .flag.present_bit = 1,
            .flag.write_bit = 1,
            .flag.use_pagesize_4_mb = 1,
            .lower_address = 0,
        },
        [0x300] = {
            .flag.present_bit = 1,
            .flag.write_bit = 1,
            .flag.use_pagesize_4_mb = 1,
            .lower_address = 0,
        },
    }};

static struct PageManagerState page_manager_state = {
    .page_frame_map = {
        [0] = true,
        [1 ... PAGE_FRAME_MAX_COUNT - 1] = false},
    .free_page_frame_count = PAGE_FRAME_MAX_COUNT - 1,
    .next_free_frame = 1,
};

void update_page_directory_entry(
    struct PageDirectory *page_dir,
    void *physical_addr,
    void *virtual_addr,
    struct PageDirectoryEntryFlag flag)
{
  uint32_t page_index = ((uint32_t)virtual_addr >> 22) & 0x3FF;
  page_dir->table[page_index].flag = flag;
  page_dir->table[page_index].lower_address = ((uint32_t)physical_addr >> 22) & 0x3FF;
  flush_single_tlb(virtual_addr);
}

void flush_single_tlb(void *virtual_addr)
{
  asm volatile("invlpg (%0)" : /* <Empty> */ : "b"(virtual_addr) : "memory");
}

/* --- Memory Management --- */
// TODO: Implement
bool paging_allocate_check(uint32_t amount)
{
  uint32_t req_page = amount / PAGE_FRAME_SIZE;
  if (amount % PAGE_FRAME_SIZE != 0)
  {
    req_page++;
  }
  return req_page <= page_manager_state.free_page_frame_count;
}

bool paging_allocate_user_page_frame(struct PageDirectory *page_dir, void *virtual_addr)
{
  /**
   * TODO: Find free physical frame and map virtual frame into it
   * - Find free physical frame in page_manager_state.page_frame_map[] using any strategies
   * - Mark page_manager_state.page_frame_map[]
   * - Update page directory with user flags:
   *     > present bit    true
   *     > write bit      true
   *     > user bit       true
   *     > pagesize 4 mb  true
   */
  if (page_manager_state.free_page_frame_count == 0)
  {
    return false;
  }

  int phy_mem_free = page_manager_state.next_free_frame;
  uint32_t phy_addr_free = PAGE_FRAME_SIZE * (phy_mem_free);
  page_manager_state.free_page_frame_count--;
  page_manager_state.page_frame_map[phy_mem_free] = true;

  for (int i = phy_mem_free + 1; i < PAGE_FRAME_MAX_COUNT; i++)
  {
    if (page_manager_state.page_frame_map[i] == false)
    {
      page_manager_state.next_free_frame = i;
      break;
    }
  }

  struct PageDirectoryEntryFlag flag = {
      .present_bit = 1,
      .write_bit = 1,
      .user_bit = 1,
      .use_pagesize_4_mb = 1,
  };
  update_page_directory_entry(
      page_dir,
      (void *)phy_addr_free,
      virtual_addr,
      flag);
  return true;
}

bool paging_free_user_page_frame(struct PageDirectory *page_dir, void *virtual_addr)
{
  /*
   * TODO: Deallocate a physical frame from respective virtual address
   * - Use the page_dir.table values to check mapped physical frame
   * - Remove the entry by setting it into 0
   */
  uint32_t page_index = ((uint32_t)virtual_addr >> 22) & 0x3FF;
  uint16_t phy_mapped = page_dir->table[page_index].lower_address;

  if (page_dir->table[page_index].flag.present_bit == 0)
  {
    return false;
  }
  page_manager_state.free_page_frame_count++;
  page_manager_state.page_frame_map[phy_mapped] = false;
  if (page_manager_state.next_free_frame > phy_mapped)
  {
    page_manager_state.next_free_frame = phy_mapped;
  }
  struct PageDirectoryEntryFlag flag = {
      .present_bit = 0,
      .write_bit = 1,
      .user_bit = 1,
      .use_pagesize_4_mb = 1,
  };
  update_page_directory_entry(
      page_dir,
      (void *)0,
      virtual_addr,
      flag);
  return true;
}

struct PageDirectory *paging_create_new_page_directory(void)
{
  /*
   * TODO: Get & initialize empty page directory from page_directory_list
   * - Iterate page_directory_list[] & get unused page directory
   * - Mark selected page directory as used
   * - Create new page directory entry for kernel higher half with flag:
   *     > present bit    true
   *     > write bit      true
   *     > pagesize 4 mb  true
   *     > lower address  0
   * - Set page_directory.table[0x300] with kernel page directory entry
   * - Return the page directory address
   */

  if (page_directory_manager.page_dir_free == 0)
  {
    return NULL;
  }
  int idx;
  for (int i = 0; i < PAGING_DIRECTORY_TABLE_MAX_COUNT; i++)
  {
    if (page_directory_manager.page_directory_used[i] == false)
    {
      idx = i;
      break;
    }
  }
  page_directory_manager.page_directory_used[idx] = true;
  page_directory_manager.page_dir_free--;

  struct PageDirectoryEntry kernel_entry = {
      .flag.present_bit = 1,
      .flag.write_bit = 1,
      .flag.use_pagesize_4_mb = 1,
      .lower_address = 0,
  };

  page_directory_list[idx].table[0x300] = kernel_entry;
  return &page_directory_list[idx];
}

bool paging_free_page_directory(struct PageDirectory *page_dir)
{
  /**
   * TODO: Iterate & clear page directory values
   * - Iterate page_directory_list[] & check &page_directory_list[] == page_dir
   * - If matches, mark the page directory as unusued and clear all page directory entry
   * - Return true
   */

  for (int i = 0; i < PAGING_DIRECTORY_TABLE_MAX_COUNT; i++)
  {
    if (&page_directory_list[i] == page_dir)
    {
      page_directory_manager.page_directory_used[i] = false;
      for (int j = 0; j < PAGE_ENTRY_COUNT; j++)
      {
        if (j == 0x300)
        {
          continue;
        }
        if (page_directory_list[i].table[j].flag.present_bit == true)
        {
          paging_free_user_page_frame(&page_directory_list[i],
                                      (void *)(j << 22));
        };
      }
      return true;
    }
  }
  return false;
}