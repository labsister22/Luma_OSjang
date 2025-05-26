
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
  int32_t retcode = PROCESS_CREATE_SUCCESS;
  if (process_manager_state.active_process_count >= PROCESS_COUNT_MAX)
  {
    retcode = PROCESS_CREATE_FAIL_MAX_PROCESS_EXCEEDED;
    goto exit_cleanup;
  }

  // Ensure entrypoint is not located at kernel's section at higher half
  if ((uint32_t)request.buf >= KERNEL_VIRTUAL_ADDRESS_BASE)
  {
    retcode = PROCESS_CREATE_FAIL_INVALID_ENTRYPOINT;
    goto exit_cleanup;
  }

  // Check whether memory is enough for the executable and additional frame for user stack
  uint32_t page_frame_count_needed = ceil_div(request.buffer_size + PAGE_FRAME_SIZE, PAGE_FRAME_SIZE);
  if (!paging_allocate_check(page_frame_count_needed) || page_frame_count_needed > PROCESS_PAGE_FRAME_COUNT_MAX)
  {
    retcode = PROCESS_CREATE_FAIL_NOT_ENOUGH_MEMORY;
    goto exit_cleanup;
  }

  // Process PCB
  int32_t p_index = process_list_get_inactive_index();
  struct ProcessControlBlock *new_pcb = &(_process_list[p_index]);
  memcpy(new_pcb->metadata.name, request.name, 8 * sizeof(char));
  struct PageDirectory *cur_active = paging_get_current_page_directory_addr();
  new_pcb->context.page_directory_virtual_addr = paging_create_new_page_directory();
  paging_use_page_directory(new_pcb->context.page_directory_virtual_addr);

  for (uint32_t i = 0; i < page_frame_count_needed; i++)
  {
    new_pcb->memory.virtual_addr_used[i] = request.buf + i * PAGE_FRAME_SIZE;
    paging_allocate_user_page_frame(new_pcb->context.page_directory_virtual_addr, new_pcb->memory.virtual_addr_used[i]);
  }
  new_pcb->memory.page_frame_used_count = page_frame_count_needed;
  read(request);
  new_pcb->context.eip = (uint32_t)request.buf;
  paging_use_page_directory(cur_active);

  new_pcb->context.eflags |= CPU_EFLAGS_BASE_FLAG | CPU_EFLAGS_FLAG_INTERRUPT_ENABLE;
  new_pcb->context.cpu.segment.ds = GDT_USER_DATA_SEGMENT_SELECTOR;
  new_pcb->context.cpu.segment.es = GDT_USER_DATA_SEGMENT_SELECTOR;
  new_pcb->context.cpu.segment.fs = GDT_USER_DATA_SEGMENT_SELECTOR;
  new_pcb->context.cpu.segment.gs = GDT_USER_DATA_SEGMENT_SELECTOR;
  new_pcb->context.cpu.stack.esp = 0x400000;
  new_pcb->metadata.pid = process_generate_new_pid();
  new_pcb->metadata.active = true;
  new_pcb->metadata.cur_state = READY;
  process_manager_state.active_process_count++;
exit_cleanup:
  return retcode;
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
