// filepath: /home/timur/Luma_OSjang/src/paging.c
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "header/memory/paging.h"
#include "header/text/framebuffer.h"

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
    .free_page_frame_count = PAGE_FRAME_MAX_COUNT - 1};

void update_page_directory_entry(
    struct PageDirectory *page_dir,
    void *physical_addr,
    void *virtual_addr,
    struct PageDirectoryEntryFlag flag)
{
  if (!page_dir || !virtual_addr)
  {
    return;
  }

  uint32_t page_index = ((uint32_t)virtual_addr >> 22) & 0x3FF;

  // Physical address needs to be divided by 4MB to get the page frame number (>> 22)
  // Ensure physical address is 4MB aligned
  uint32_t phys_addr = (uint32_t)physical_addr;
  page_dir->table[page_index].flag = flag;
  page_dir->table[page_index].lower_address = (phys_addr >> 22) & 0x3FF;

  // Reset other fields to 0 to ensure no old values remain
  page_dir->table[page_index].global_page = 0;
  page_dir->table[page_index].reserved_1 = 0;
  page_dir->table[page_index].pat = 0;
  page_dir->table[page_index].reserved_2 = 0;
  page_dir->table[page_index].higher_address = 0; // Keep as 0 for 32-bit systems

  // Flush TLB for this virtual address
  flush_single_tlb(virtual_addr);
}

void flush_single_tlb(void *virtual_addr)
{
  asm volatile("invlpg (%0)" : /* <Empty> */ : "b"(virtual_addr) : "memory");
}

void flush_tlb_all(void)
{
  // Flush entire TLB by reloading CR3
  uint32_t cr3;
  asm volatile("mov %%cr3, %0" : "=r"(cr3));
  asm volatile("mov %0, %%cr3" : : "r"(cr3) : "memory");
}

/* --- Memory Management --- */
bool paging_allocate_check(uint32_t amount)
{
  uint32_t required_frames = (amount + PAGE_FRAME_SIZE - 1) / PAGE_FRAME_SIZE;
  return page_manager_state.free_page_frame_count >= required_frames;
}

bool paging_allocate_user_page_frame(struct PageDirectory *page_dir, void *virtual_addr)
{
  if (!page_dir)
  {
    return false;
  }

  uint32_t page_index = ((uint32_t)virtual_addr >> 22) & 0x3FF;

  // Don't allow allocation of kernel reserved pages (0x0-0x3FF and 0x300-0x301)
  if (page_index >=768)
  {
    return false; // Reserved for kernel
  }

  // Check if page is already allocated
  if (page_dir->table[page_index].flag.present_bit)
  {
    return false; // Already allocated
  }

  // Find free physical frame
  uint32_t frame_index;
  for (frame_index = 1; frame_index < PAGE_FRAME_MAX_COUNT; frame_index++)
  {
    if (!page_manager_state.page_frame_map[frame_index])
    {
      break;
    }
  }

  if (frame_index >= PAGE_FRAME_MAX_COUNT)
  {
    return false; // No free frames
  }

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
  page_dir->table[page_index].lower_address = frame_index & 0x3FF;
  page_dir->table[page_index].global_page = 0;
  page_dir->table[page_index].pat = 0;
  page_dir->table[page_index].reserved_1 = 0;
  page_dir->table[page_index].reserved_2 = 0;
  page_dir->table[page_index].higher_address = (frame_index>>10) &0x3FF; // Keep as 0 for 32-bit systems

  flush_single_tlb(virtual_addr);
  return true;
}

bool paging_free_user_page_frame(struct PageDirectory *page_dir, void *virtual_addr)
{
  if (!page_dir)
  {
    return false;
  }
  uint32_t page_index = ((uint32_t)virtual_addr >> 22) & 0x3FF;
  // FIX: Hanya tolak kernel space (>= 3GB), IZINKAN page 0 untuk user
  // Page 0 = alamat 0x00000000-0x003FFFFF (4MB pertama)
  // Page 0x300 = alamat 0xC0000000+ (kernel space)
  if (page_index >= 768) { // 768 * 4MB = 3GB (kernel space start)
    return false; // Tolak kernel space
  }

  // Check if the page is allocated
  if (!page_dir->table[page_index].flag.present_bit)
  {
    return false; // Not allocated
  }

  // FIX: Get frame index from page directory entry
  uint32_t frame_index = page_dir->table[page_index].lower_address | 
                        (page_dir->table[page_index].higher_address << 10);

  // Validate frame index
  if (frame_index >= PAGE_FRAME_MAX_COUNT || frame_index == 0) {
    return false; // Invalid frame or reserved frame
  }

  // Mark frame as free
  page_manager_state.page_frame_map[frame_index] = false;
  page_manager_state.free_page_frame_count++;

  // Clear page directory entry
  page_dir->table[page_index].flag.present_bit = 0;
  page_dir->table[page_index].flag.write_bit = 0;
  page_dir->table[page_index].flag.user_bit = 0;
  page_dir->table[page_index].lower_address = 0;
  page_dir->table[page_index].higher_address = 0;

  flush_single_tlb(virtual_addr);
  return true;
}

// Convert virtual address to physical address for CR3
static uint32_t get_physical_address(void *virtual_addr)
{
  // If running in higher-half kernel, convert virtual to physical
  uint32_t virt = (uint32_t)virtual_addr;
  if (virt >= 0xC0000000)
  {
    // Higher-half virtual address, subtract kernel base
    return virt - 0xC0000000;
  }
  // Already physical or identity mapped
  return virt;
}

// Paging activation function
void paging_activate(struct PageDirectory *page_dir)
{
  if (!page_dir)
  {
    return;
  }

  // Get physical address of page directory
  uint32_t phys_addr = get_physical_address(page_dir);

  // Ensure page directory is 4KB aligned
  if (phys_addr & 0xFFF)
  {
    return; // Not aligned
  }

  // Load page directory into CR3
  asm volatile("mov %0, %%cr3" : : "r"(phys_addr) : "memory");

  // Enable 4MB paging (PSE bit in CR4)
  uint32_t cr4;
  asm volatile("mov %%cr4, %0" : "=r"(cr4));
  cr4 |= 0x00000010; // PSE flag (bit 4)
  asm volatile("mov %0, %%cr4" : : "r"(cr4) : "memory");

  // Enable paging (PG bit in CR0)
  uint32_t cr0;
  asm volatile("mov %%cr0, %0" : "=r"(cr0));
  cr0 |= 0x80000000; // PG flag (bit 31)
  asm volatile("mov %0, %%cr0" : : "r"(cr0) : "memory");

  // Full TLB flush after enabling paging
  flush_tlb_all();
}

// Additional utility functions
uint32_t paging_get_free_frame_count(void)
{
  return page_manager_state.free_page_frame_count;
}

bool paging_is_page_allocated(struct PageDirectory *page_dir, void *virtual_addr)
{
  if (!page_dir || !virtual_addr)
  {
    return false;
  }

  uint32_t page_index = ((uint32_t)virtual_addr >> 22) & 0x3FF;
  return page_dir->table[page_index].flag.present_bit;
}
bool paging_map_user_page(struct PageDirectory *page_dir, void *virtual_addr, void *physical_addr)
{
    if (!page_dir) return false;
    uint32_t page_index = ((uint32_t)virtual_addr >> 22) & 0x3FF;
    if (page_index >= 768) return false;

    if (((uint32_t)physical_addr & 0x3FFFFF) != 0) return false;
    // Set user flags
    struct PageDirectoryEntryFlag flag = {
        .present_bit = 1,
        .write_bit = 1,
        .user_bit = 1,
        .write_through_bit = 0,
        .cache_disable_bit = 0,
        .accessed_bit = 0,
        .dirty_bit = 0,
        .use_pagesize_4_mb = 1
    };

    update_page_directory_entry(page_dir, physical_addr, virtual_addr, flag);
    return true;
}