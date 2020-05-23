#ifndef LLARP_LOGIC_HPP
#define LLARP_LOGIC_HPP

#include <ev/ev.hpp>
#include <util/mem.h>
#include <util/thread/threadpool.h>
#include <optional>

namespace llarp
{
  class Logic
  {
   public:
    Logic(size_t queueLength = size_t{1024 * 8});

    ~Logic();

    /// stop all operation and wait for that to die
    void
    stop();

    bool
    queue_job(struct llarp_thread_job job);

    void
    Call(std::function<void(void)> func);

    uint32_t
    call_later(llarp_time_t later, std::function<void(void)> func);

    void
    cancel_call(uint32_t id);

    void
    remove_call(uint32_t id);

    size_t
    numPendingJobs() const;

    bool
    can_flush() const;

    void
    SetQueuer(std::function<void(std::function<void(void)>)> q);

    void
    set_event_loop(llarp_ev_loop* loop);

    void
    clear_event_loop();

   private:
    using ID_t = std::thread::id;
    llarp_ev_loop* m_Loop = nullptr;
    std::optional<ID_t> m_ID;
    util::ContentionKiller m_Killer;
    std::function<void(std::function<void(void)>)> m_Queue;
  };
}  // namespace llarp

/// this used to be a macro
template <typename Logic_ptr, typename Func_t>
static bool
LogicCall(const Logic_ptr& logic, Func_t func)
{
  logic->Call(std::move(func));
  return true;
}

#endif
