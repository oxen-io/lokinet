#include <llarp/logic.h>
#include <llarp/mem.h>

struct llarp_logic {
  struct llarp_alloc * mem;
  struct llarp_threadpool* thread;
  struct llarp_timer_context* timer;
};

struct llarp_logic* llarp_init_logic(struct llarp_alloc * mem) {
  struct llarp_logic* logic = mem->alloc(mem, sizeof(struct llarp_logic), 8);
  if (logic) {
    logic->mem = mem;
    logic->thread = llarp_init_threadpool(1);
    logic->timer = llarp_init_timer();
  }
  return logic;
};

void llarp_free_logic(struct llarp_logic** logic) {
  if (*logic) {
    struct llarp_alloc * mem = (*logic)->mem;
    llarp_free_threadpool(&(*logic)->thread);
    llarp_free_timer(&(*logic)->timer);
    mem->free(mem, *logic);
    *logic = NULL;
  }
}

void llarp_logic_stop(struct llarp_logic* logic) {
  llarp_timer_stop(logic->timer);
  llarp_threadpool_stop(logic->thread);
  llarp_threadpool_join(logic->thread);
}

void llarp_logic_mainloop(struct llarp_logic* logic) {
  llarp_threadpool_start(logic->thread);
  llarp_timer_run(logic->timer, logic->thread);
}

void llarp_logic_queue_job(struct llarp_logic * logic, struct llarp_thread_job job)
{
  llarp_threadpool_queue_job(logic->thread, job);
}

uint32_t llarp_logic_call_later(struct llarp_logic* logic, struct llarp_timeout_job job)
{
  return llarp_timer_call_later(logic->timer, job);
}

void llarp_logic_cancel_call(struct llarp_logic * logic, uint32_t id)
{
  llarp_timer_cancel(logic->timer, id);
}
  
