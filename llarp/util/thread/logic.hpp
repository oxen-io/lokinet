#ifndef LLARP_LOGIC_HPP
#define LLARP_LOGIC_HPP

#include <ev/ev.hpp>
#include <util/mem.h>
#include <util/thread/threadpool.h>
#include <absl/types/optional.h>

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

    bool
    _traceLogicCall(std::function< void(void) > func, const char* filename,
                    int lineo);

    uint32_t
    call_later(llarp_time_t later, std::function< void(void) > func);

    void
    cancel_call(uint32_t id);

    void
    remove_call(uint32_t id);

    size_t
    numPendingJobs() const;

    bool
    can_flush() const;

    void
    SetQueuer(std::function< void(std::function< void(void) >) > q);

    void
    set_event_loop(llarp_ev_loop* loop);

    void
    clear_event_loop();

   private:
    using ID_t = std::thread::id;
    llarp_threadpool* const m_Thread;
    llarp_ev_loop* m_Loop;
    absl::optional< ID_t > m_ID;
    util::ContentionKiller m_Killer;
    std::function< void(std::function< void(void) >) > m_Queue;
  };
}  // namespace llarp

#ifndef LogicCall
#if defined(LOKINET_DEBUG)
#ifdef LOG_TAG
#define LogicCall(l, ...) l->_traceLogicCall(__VA_ARGS__, LOG_TAG, __LINE__)
#else
#define LogicCall(l, ...) l->_traceLogicCall(__VA_ARGS__, __FILE__, __LINE__)
#endif
#else
#define LogicCall(l, ...) l->_traceLogicCall(__VA_ARGS__, 0, 0)
#endif
#endif
#endif
