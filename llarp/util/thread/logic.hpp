#ifndef LLARP_LOGIC_HPP
#define LLARP_LOGIC_HPP

#include <util/mem.h>
#include <util/thread/threadpool.h>
#include <util/thread/timer.hpp>
#include <absl/types/optional.h>

namespace llarp
{
  class Logic
  {
   public:
    struct llarp_threadpool* thread;
    struct llarp_timer_context* timer;
    absl::optional< std::thread::id > id;

    Logic();

    ~Logic();

    /// single threaded tick
    void
    tick(llarp_time_t now);

    /// isolated tick
    void
    tick_async(llarp_time_t now);

    void
    stop_timer();

    void
    stop();

    void
    mainloop();

    void
    queue_job(struct llarp_thread_job job);

    template < typename F >
    bool
    queue_func(F&& func)
    {
      if(can_flush() && thread->impl->jobCount() == thread->impl->capacity())
      {
        // we are in the logic thread and our job queue is full
        // we want to defer this call with a timer job
        call_later(5, func);
        return true;
      }
      // otherwise just add it
      return thread->impl->addJob(func);
    }

    uint32_t
    call_later(const llarp_timeout_job& job);

    void
    call_later(llarp_time_t later, std::function< void(void) > func);

    void
    cancel_call(uint32_t id);

    void
    remove_call(uint32_t id);

    bool
    can_flush() const;

   private:
    size_t m_Jobs = 0;
  };
}  // namespace llarp

#endif
