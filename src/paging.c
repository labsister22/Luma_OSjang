// filepath: /home/timur/Luma_OSjang/src/paging.c
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "header/memory/paging.h"

__attribute__((aligned(0x1000))) struct PageDirectory _paging_kernel_page_directory = {
    .table = {
        /* Entry 0: Identity mapping for bootloader and initial boot */
        [0] = {
            .flag = {
                .present_bit = 1,
                .write_bit = 1,
                .user_bit = 0,
                .write_through_bit = 0,
                .cache_disable_bit = 0,
                .accessed_bit = 0,
                .dirty_bit = 0,
                .use_pagesize_4_mb = 1,
            },
            .global_page = 0,
            .available = 0,
            .pat_bit = 0,
            .reserved_1 = 0,
            .lower_address = 0, /* Maps physical 0x00000000-0x00400000 to virtual 0x00000000-0x00400000 */
            .reserved_2 = 0,
            .available_2 = 0,
            .higher_address = 0,
            .reserved_3 = 0},
        /* Entry 0x300: Higher-half kernel mapping
           Maps physical 0x00000000-0x00400000 to virtual 0xC0000000-0xC0400000
           This includes framebuffer at 0xB8000 -> 0xC00B8000 */
        [0x300] = {.flag = {
                       .present_bit = 1,
                       .write_bit = 1,
                       .user_bit = 0,
                       .write_through_bit = 0,
                       .cache_disable_bit = 0,
                       .accessed_bit = 0,
                       .dirty_bit = 0,
                       .use_pagesize_4_mb = 1,
                   },
                   .global_page = 0,
                   .available = 0,
                   .pat_bit = 0,
                   .reserved_1 = 0,
                   .lower_address = 0, /* index 0 = 0x00000000 physical address */
                   .reserved_2 = 0,
                   .available_2 = 0,
                   .higher_address = 0,
                   .reserved_3 = 0},
        /* Entry 0x301: Higher-half kernel segment kedua
           Maps physical 0x00400000-0x00800000 to virtual 0xC0400000-0xC0800000 */
        [0x301] = {.flag = {
                       .present_bit = 1,
                       .write_bit = 1,
                       .user_bit = 0,
                       .write_through_bit = 0,
                       .cache_disable_bit = 0,
                       .accessed_bit = 0,
                       .dirty_bit = 0,
                       .use_pagesize_4_mb = 1,
                   },
                   .global_page = 0,
                   .available = 0,
                   .pat_bit = 0,
                   .reserved_1 = 0,
                   .lower_address = 1, /* index 1 = 0x00400000 physical address */
                   .reserved_2 = 0,
                   .available_2 = 0,
                   .higher_address = 0,
                   .reserved_3 = 0},
    }};

static struct PageManagerState page_manager_state = {
    .page_frame_map = {
        [0] = true,
        [1 ... PAGE_FRAME_MAX_COUNT - 1] = false},
    .free_page_frame_count = PAGE_FRAME_MAX_COUNT - 1};

void update_page_directory_entry(
    struct PageDirectory *page_dir,
    void *physical_addr,
    void *virtual_addr,
    struct PageDirectoryEntryFlag flag)
{
  uint32_t page_index = ((uint32_t)virtual_addr >> 22) & 0x3FF;
  page_dir->table[page_index].flag = flag;

  // Physical address needs to be divided by 4MB to get the page frame number (>> 22)
  page_dir->table[page_index].lower_address = ((uint32_t)physical_addr) >> 22;

  // Reset other fields to 0 to ensure no old values remain
  page_dir->table[page_index].global_page = 0;
  page_dir->table[page_index].available = 0;
  page_dir->table[page_index].pat_bit = 0;
  page_dir->table[page_index].reserved_1 = 0;
  page_dir->table[page_index].reserved_2 = 0;
  page_dir->table[page_index].available_2 = 0;
  page_dir->table[page_index].higher_address = 0;
  page_dir->table[page_index].reserved_3 = 0;

  // Flush TLB for this virtual address
  flush_single_tlb(virtual_addr);
}

void flush_single_tlb(void *virtual_addr)
{
  asm volatile("invlpg (%0)" : /* <Empty> */ : "b"(virtual_addr) : "memory");
}

/* --- Memory Management --- */
bool paging_allocate_check(uint32_t amount)
{
  uint32_t required_frames = (amount + PAGE_FRAME_SIZE - 1) / PAGE_FRAME_SIZE;
  return page_manager_state.free_page_frame_count >= required_frames;
}

bool paging_allocate_user_page_frame(struct PageDirectory *page_dir, void *virtual_addr)
{
  uint32_t page_index = ((uint32_t)virtual_addr >> 22) & 0x3FF;

  // Find free physical frame
  uint32_t frame_index;
  for (frame_index = 0; frame_index < PAGE_FRAME_MAX_COUNT; frame_index++)
  {
    if (!page_manager_state.page_frame_map[frame_index])
      break;
  }

  if (frame_index >= PAGE_FRAME_MAX_COUNT)
    return false;

  // Mark frame as used
  page_manager_state.page_frame_map[frame_index] = true;
  page_manager_state.free_page_frame_count--;

  // Set user flags
  struct PageDirectoryEntryFlag flag = {
      .present_bit = 1,
      .write_bit = 1,
      .user_bit = 1,
      .write_through_bit = 0,
      .cache_disable_bit = 0,
      .accessed_bit = 0,
      .dirty_bit = 0,
      .use_pagesize_4_mb = 1};

  // Update page directory
  page_dir->table[page_index].flag = flag;
  page_dir->table[page_index].lower_address = frame_index;
  page_dir->table[page_index].global_page = 0;
  page_dir->table[page_index].available = 0;
  page_dir->table[page_index].pat_bit = 0;
  page_dir->table[page_index].reserved_1 = 0;
  page_dir->table[page_index].reserved_2 = 0;
  page_dir->table[page_index].available_2 = 0;
  page_dir->table[page_index].higher_address = 0;
  page_dir->table[page_index].reserved_3 = 0;

  flush_single_tlb(virtual_addr);
  return true;
}

bool paging_free_user_page_frame(struct PageDirectory *page_dir, void *virtual_addr)
{
  uint32_t page_index = ((uint32_t)virtual_addr >> 22) & 0x3FF;

  // Check if the page is allocated
  if (!page_dir->table[page_index].flag.present_bit)
    return false;

  // Get the physical frame index
  uint32_t frame_index = page_dir->table[page_index].lower_address;

  // Mark frame as free
  page_manager_state.page_frame_map[frame_index] = false;
  page_manager_state.free_page_frame_count++;

  // Clear page directory entry (set all to 0)
  page_dir->table[page_index].flag.present_bit = 0;
  page_dir->table[page_index].flag.write_bit = 0;
  page_dir->table[page_index].flag.user_bit = 0;
  page_dir->table[page_index].flag.write_through_bit = 0;
  page_dir->table[page_index].flag.cache_disable_bit = 0;
  page_dir->table[page_index].flag.accessed_bit = 0;
  page_dir->table[page_index].flag.dirty_bit = 0;
  page_dir->table[page_index].flag.use_pagesize_4_mb = 0;
  page_dir->table[page_index].global_page = 0;
  page_dir->table[page_index].available = 0;
  page_dir->table[page_index].pat_bit = 0;
  page_dir->table[page_index].reserved_1 = 0;
  page_dir->table[page_index].lower_address = 0;
  page_dir->table[page_index].reserved_2 = 0;
  page_dir->table[page_index].available_2 = 0;
  page_dir->table[page_index].higher_address = 0;
  page_dir->table[page_index].reserved_3 = 0;

  flush_single_tlb(virtual_addr);
  return true;
}