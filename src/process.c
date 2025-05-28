
#include "header/process/process.h"
#include "header/memory/paging.h"
#include "header/stdlib/string.h"
#include "header/cpu/gdt.h"

struct ProcessControlBlock _process_list[PROCESS_COUNT_MAX];

struct ProcessState process_manager_state = {
    .active_process_count = 0,
    .cur_idx = -1,
    .total_pid = 0,
};

void getActivePCB(struct ProcessControlBlock *ptr, int *count)
{
  *count = 0;
  for (int i = 0; i < PROCESS_COUNT_MAX; i++)
  {
    if (_process_list[i].metadata.active)
    {
      ptr[*count] = _process_list[i];
      (*count)++;
    }
  }
}

int process_list_get_inactive_index()
{
  if (process_manager_state.active_process_count == 0)
  {
    return 0;
  }
  int idx = process_manager_state.cur_idx - 1;
  if (idx == -1)
  {
    idx = PROCESS_COUNT_MAX - 1;
  }
  while (_process_list[idx].metadata.active == true)
  {
    idx--;
    if (idx == -1)
    {
      idx = PROCESS_COUNT_MAX - 1;
    }
  }
  return idx;
}
int process_generate_new_pid()
{
  process_manager_state.total_pid++;
  return process_manager_state.total_pid;
}

bool release_memory(struct ProcessControlBlock *pcb)
{
  for (uint32_t i = 0; i < pcb->memory.page_frame_used_count; i++)
  {
    void *addr = pcb->memory.virtual_addr_used[i];
    paging_free_user_page_frame(pcb->context.page_directory_virtual_addr, addr);
    pcb->memory.virtual_addr_used[i] = NULL;
  }
  struct PageDirectory *cur_run = paging_get_current_page_directory_addr();
  paging_use_page_directory(pcb->context.page_directory_virtual_addr);
  paging_free_page_directory(pcb->context.page_directory_virtual_addr);
  if (cur_run != pcb->context.page_directory_virtual_addr)
  {
    paging_use_page_directory(cur_run);
  }
  pcb->memory.page_frame_used_count = 0;

  return true;
}

int32_t process_create_user_process(struct EXT2DriverRequest request)
{
    if (process_manager_state.active_process_count >= PROCESS_COUNT_MAX) {
      return PROCESS_CREATE_FAIL_MAX_PROCESS_EXCEEDED;
    }

    // Find empty PCB slot
    int32_t index_process_unused = -1;
    for (int32_t i = 0; i < PROCESS_COUNT_MAX; i++)
    {
        if (!_process_list[i].metadata.active)
        {
            index_process_unused = i;
            break;
        }
    }

    if (index_process_unused == -1) {
        return PROCESS_CREATE_FAIL_MAX_PROCESS_EXCEEDED;
    }

    struct ProcessControlBlock *new_pcb = &_process_list[index_process_unused];
    
    // CRITICAL: Ensure buffer address is valid and aligned
    if (request.buf == NULL) {
        return PROCESS_CREATE_FAIL_INVALID_ENTRYPOINT;
    }
    uint32_t addr = (uint32_t)request.buf;
    if (addr < 0x400000 || addr >= 0x800000) {  // Must be in 4MB-8MB range
        return PROCESS_CREATE_FAIL_INVALID_ENTRYPOINT;
    }
    uint32_t page_frame_count_needed = (request.buffer_size + PAGE_FRAME_SIZE - 1) / PAGE_FRAME_SIZE;
    if (page_frame_count_needed > PROCESS_PAGE_FRAME_COUNT_MAX) {
        return PROCESS_CREATE_FAIL_NOT_ENOUGH_MEMORY;
    }

    // Create new page directory
    struct PageDirectory *cur_active = paging_get_current_page_directory_addr();
    new_pcb->context.page_directory_virtual_addr = paging_create_new_page_directory();
    
    if (new_pcb->context.page_directory_virtual_addr == NULL) {
      return PROCESS_CREATE_FAIL_NOT_ENOUGH_MEMORY;
    }

    // Switch to new page directory
    paging_use_page_directory(new_pcb->context.page_directory_virtual_addr);

    // Allocate and map memory pages
    for (uint32_t i = 0; i < page_frame_count_needed; i++)
    {
        void *virtual_addr = request.buf + i * PAGE_FRAME_SIZE;
        new_pcb->memory.virtual_addr_used[i] = virtual_addr;
        
        // CRITICAL: Ensure page allocation succeeds
        if (!paging_allocate_user_page_frame(new_pcb->context.page_directory_virtual_addr, virtual_addr)) {
            // Allocation failed, cleanup
            for (uint32_t j = 0; j < i; j++) {
                paging_free_user_page_frame(new_pcb->context.page_directory_virtual_addr, new_pcb->memory.virtual_addr_used[j]);
            }
            paging_use_page_directory(cur_active);
            return PROCESS_CREATE_FAIL_NOT_ENOUGH_MEMORY;
        }
    }
    new_pcb->memory.page_frame_used_count = page_frame_count_needed;

    // Load program into allocated memory
    read(request);

    // CRITICAL: Set entry point to shell buffer address
    new_pcb->context.eip = (uint32_t)request.buf;  // Should be 0x400000, not 0x0
    
    // Switch back to kernel page directory
    paging_use_page_directory(cur_active);

    // Setup process context
    new_pcb->context.eflags |= CPU_EFLAGS_BASE_FLAG | CPU_EFLAGS_FLAG_INTERRUPT_ENABLE;
    new_pcb->context.cpu.segment.ds = GDT_USER_DATA_SEGMENT_SELECTOR;
    new_pcb->context.cpu.segment.es = GDT_USER_DATA_SEGMENT_SELECTOR;
    new_pcb->context.cpu.segment.fs = GDT_USER_DATA_SEGMENT_SELECTOR;
    new_pcb->context.cpu.segment.gs = GDT_USER_DATA_SEGMENT_SELECTOR;
    
    new_pcb->metadata.pid = process_generate_new_pid();
    new_pcb->metadata.active = true;
    new_pcb->metadata.cur_state = READY;
    process_manager_state.active_process_count++;

    return PROCESS_CREATE_SUCCESS;
}

struct ProcessControlBlock *process_get_current_running_pcb_pointer(void)
{
  if (process_manager_state.cur_idx == -1)
  {
    return NULL;
  }
  return &_process_list[process_manager_state.cur_idx];
}

void qemu_exit()
{
  asm volatile("outw %0, %1" : : "a"((uint16_t)0x2000), "Nd"((uint16_t)0x604));
}

/**
 * Destroy process then release page directory and process control block
 *
 * @param pid Process ID to delete
 * @return    True if process destruction success
 */
bool process_destroy(uint32_t pid)
{
  for (int i = 0; i < PROCESS_COUNT_MAX; i++)
  {
    if (_process_list[i].metadata.pid == pid)
    {
      if (!release_memory(&_process_list[i]))
      {
        return false;
      }

      // release pcb;
      if (process_manager_state.active_process_count == 1)
      {
        asm volatile("outw %0, %1" : : "a"((uint16_t)0x2000), "Nd"((uint16_t)0x604));
      }

      _process_list[i].metadata.active = false;
      process_manager_state.active_process_count--;

      return true;
    }
  }

  return false;
}
