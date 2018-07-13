#include <llarp/logic.h>
#include <llarp/mem.h>
#include "logger.hpp"

struct llarp_logic*
llarp_init_logic()
{
  llarp_logic* logic = new llarp_logic;
  if(logic)
  {
    logic->thread = llarp_init_threadpool(1, "llarp-logic");
    logic->timer  = llarp_init_timer();
  }
  return logic;
};

struct llarp_logic*
llarp_init_single_process_logic(struct llarp_threadpool* tp)
{
  llarp_logic* logic = new llarp_logic;
  if(logic)
  {
    logic->thread = tp;
    logic->timer  = llarp_init_timer();
  }
  return logic;
}

void
llarp_logic_tick(struct llarp_logic* logic)
{
  llarp_timer_tick_all(logic->timer, logic->thread);
}

void
llarp_free_logic(struct llarp_logic** logic)
{
  if(*logic)
  {
    // llarp_free_timer(&(*logic)->timer);
    delete *logic;
  }
  *logic = nullptr;
}

void
llarp_logic_stop_timer(struct llarp_logic* logic)
{
  if(logic->timer)
    llarp_timer_stop(logic->timer);
}

void
llarp_logic_stop(struct llarp_logic* logic)
{
  llarp::LogDebug("logic thread stop");
  if(logic->thread)
  {
    llarp_threadpool_stop(logic->thread);
    llarp_threadpool_join(logic->thread);
  }
  llarp_free_threadpool(&logic->thread);

  llarp::LogDebug("logic timer stop");
  if(logic->timer)
    llarp_timer_stop(logic->timer);
}

void
llarp_logic_mainloop(struct llarp_logic* logic)
{
  llarp_timer_run(logic->timer, logic->thread);
}

void
llarp_logic_queue_job(struct llarp_logic* logic, struct llarp_thread_job job)
{
  llarp_thread_job j;
  j.user = job.user;
  j.work = job.work;
  llarp_threadpool_queue_job(logic->thread, j);
}

uint32_t
llarp_logic_call_later(struct llarp_logic* logic, struct llarp_timeout_job job)
{
  llarp_timeout_job j;
  j.user    = job.user;
  j.timeout = job.timeout;
  j.handler = job.handler;
  return llarp_timer_call_later(logic->timer, j);
}

void
llarp_logic_cancel_call(struct llarp_logic* logic, uint32_t id)
{
  llarp_timer_cancel_job(logic->timer, id);
}

void
llarp_logic_remove_call(struct llarp_logic* logic, uint32_t id)
{
  llarp_timer_remove_job(logic->timer, id);
}
