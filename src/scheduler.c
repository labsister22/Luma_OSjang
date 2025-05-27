#include "./header/scheduler/scheduler.h"
#include "./header/cpu/interrupt.h"

/* --- Scheduler --- */
/**
 * Initialize scheduler before executing init process
 */
void scheduler_init(void)
{
  activate_timer_interrupt();
}

/**
 * Save context to current running process
 *
 * @param ctx Context to save to current running process control block
 */
void scheduler_save_context_to_current_running_pcb(struct Context ctx)
{
  struct ProcessControlBlock *cur_run = &_process_list[process_manager_state.cur_idx];
  cur_run->context.cpu = ctx.cpu;
  cur_run->context.eflags = ctx.eflags;
  cur_run->context.eip = ctx.eip;
}

/**
 * Trigger the scheduler algorithm and context switch to new process
 */
void scheduler_switch_to_next_process(void)
{
  // if (process_manager_state.active_process_count > 1) {
  int idx = (process_manager_state.cur_idx + 1) % PROCESS_COUNT_MAX;
  while (!_process_list[idx].metadata.active)
  {
    idx = (idx + 1) % PROCESS_COUNT_MAX;
  }
  _process_list[process_manager_state.cur_idx].metadata.cur_state = READY;
  struct Context *ctx = &_process_list[idx].context;
  paging_use_page_directory(ctx->page_directory_virtual_addr);
  process_manager_state.cur_idx = idx;
  _process_list[idx].metadata.cur_state = RUNNING;
  process_context_switch(*ctx);
}