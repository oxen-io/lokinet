#include <util/logic.hpp>

#include <util/logger.hpp>
#include <util/mem.h>

namespace llarp
{
  void
  Logic::tick(llarp_time_t now)
  {
    llarp_timer_set_time(this->timer, now);
    llarp_timer_tick_all(this->timer);
    llarp_threadpool_tick(this->thread);
  }

  void
  Logic::tick_async(llarp_time_t now)
  {
    llarp_timer_tick_all_async(this->timer, this->thread, now);
    llarp_threadpool_tick(this->thread);
  }

  void
  Logic::stop_timer()
  {
    llarp_timer_stop(this->timer);
  }

  void
  Logic::queue_job(struct llarp_thread_job job)
  {
    if(job.user && job.work)
      queue_func(std::bind(job.work, job.user));
  }

  void
  Logic::stop()
  {
    llarp::LogDebug("logic thread stop");
    if(this->thread)
    {
      llarp_threadpool_stop(this->thread);
      llarp_threadpool_join(this->thread);
    }
    llarp_free_threadpool(&this->thread);

    llarp::LogDebug("logic timer stop");
    if(this->timer)
      llarp_timer_stop(this->timer);
  }

  void
  Logic::mainloop()
  {
    llarp_timer_run(this->timer, this->thread);
  }

  bool
  Logic::queue_func(std::function< void(void) > f)
  {
    if(!this->thread->QueueFunc(f))
    {
      // try calling it later if the job queue overflows
      this->call_later(1, f);
    }
    return true;
  }

  void
  Logic::call_later(llarp_time_t timeout, std::function< void(void) > func)
  {
    llarp_timer_call_func_later(this->timer, timeout, func);
  }

  uint32_t
  Logic::call_later(const llarp_timeout_job& job)
  {
    llarp_timeout_job j;
    j.user    = job.user;
    j.timeout = job.timeout;
    j.handler = job.handler;
    return llarp_timer_call_later(this->timer, j);
  }

  void
  Logic::cancel_call(uint32_t id)
  {
    llarp_timer_cancel_job(this->timer, id);
  }

  void
  Logic::remove_call(uint32_t id)
  {
    llarp_timer_remove_job(this->timer, id);
  }

  bool
  Logic::can_flush() const
  {
    return ourID == std::this_thread::get_id();
  }

}  // namespace llarp
