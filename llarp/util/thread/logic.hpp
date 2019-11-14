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
    Logic();

    ~Logic();

    /// trigger times as needed
    void
    tick(llarp_time_t now);

    /// stop all operation and wait for that to die
    void
    stop();

    bool
    queue_job(struct llarp_thread_job job);

    bool
    queue_func(std::function< void(void) > func);

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
    using ID_t = std::thread::id;
    llarp_threadpool* const m_Thread;
    llarp_timer_context* const m_Timer;
    absl::optional< ID_t > m_ID;
    util::ContentionKiller m_Killer;
  };
}  // namespace llarp

#endif
