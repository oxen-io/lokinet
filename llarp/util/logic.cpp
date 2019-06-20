#include <util/logic.hpp>

#include <util/logger.hpp>
#include <util/mem.hpp>

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
    if((job.user != nullptr) && (job.work != nullptr)) {
      queue_func(std::bind(job.work, job.user));
}
  }

  void
  Logic::stop()
  {
    llarp::LogDebug("logic thread stop");
    if(this->thread != nullptr)
    {
      llarp_threadpool_stop(this->thread);
      llarp_threadpool_join(this->thread);
    }
    llarp_free_threadpool(&this->thread);

    llarp::LogDebug("logic timer stop");
    if(this->timer != nullptr) {
      llarp_timer_stop(this->timer);
}
  }

  void
  Logic::mainloop()
  {
    llarp_timer_run(this->timer, this->thread);
  }

  bool
  Logic::queue_func(const std::function< void(void) >& f)
  {
    size_t left = 1000;
    while(!this->thread->QueueFunc(f))
    {
      // our queue is full
      if(this->can_flush())
      {
        // we can flush the queue here so let's do it
        this->tick(llarp::time_now_ms());
      }
      else
      {
        // wait a bit and retry queuing because we are not in the same thread as
        // we are calling the jobs in
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
      }
      left--;
      if(left == 0) {  // too many retries
        return false;
}
    }
    return true;
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
    return (ourPID != 0) && ourPID == ::getpid();
  }

}  // namespace llarp
