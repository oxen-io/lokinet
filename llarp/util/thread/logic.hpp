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
      const std::function< void(void) > f = std::move(func);
      if(not thread->impl->tryAddJob(f))
      {
        if(can_flush())
        {
          llarp_timer_call_func_later(timer, 5, f);
          return true;
        }
      }
      return thread->impl->addJob(f);
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
