#include <util/thread/logic.hpp>
#include <util/thread/timer.hpp>
#include <util/logging/logger.hpp>
#include <util/mem.h>

#include <future>

namespace llarp
{
  void
  Logic::tick(llarp_time_t now)
  {
    llarp_timer_tick_all_async(m_Timer, m_Thread, now);
  }

  Logic::Logic()
      : m_Thread(llarp_init_threadpool(1, "llarp-logic"))
      , m_Timer(llarp_init_timer())
  {
    llarp_threadpool_start(m_Thread);
    /// set thread id
    std::promise< ID_t > result;
    // queue setting id and try to get the result back
    llarp_threadpool_queue_job(m_Thread, [&]() {
      m_ID.emplace(std::this_thread::get_id());
      result.set_value(m_ID.value());
    });
    // get the result back
    ID_t spawned = result.get_future().get();
    LogDebug("logic thread spawned on ", spawned);
  }

  Logic::~Logic()
  {
    delete m_Thread;
    llarp_free_timer(m_Timer);
  }

  bool
  Logic::queue_job(struct llarp_thread_job job)
  {
    return job.user && job.work && queue_func(std::bind(job.work, job.user));
  }

  void
  Logic::stop()
  {
    llarp::LogDebug("logic thread stop");
    // stop all timers from happening in the future
    queue_func(std::bind(&llarp_timer_stop, m_Timer));
    // stop all operations on threadpool
    llarp_threadpool_stop(m_Thread);
  }

  bool
  Logic::queue_func(std::function< void(void) > func)
  {
    // wrap the function so that we ensure that it's always calling stuff one at
    // a time
    auto f = [self = this, func]() { self->m_Killer.TryAccess(func); };
    if(m_Thread->LooksFull(5))
    {
      LogWarn(
          "holy crap, we are trying to queue a job onto the logic thread but "
          "it looks full");
      if(can_flush())
      {
        // we are calling in the logic thread and our queue looks full
        // defer call to a later time so we don't die like a little bitch
        call_later(m_Thread->GuessJobLatency() / 2, f);
        return true;
      }
    }
    return llarp_threadpool_queue_job(m_Thread, f);
  }

  void
  Logic::call_later(llarp_time_t timeout, std::function< void(void) > func)
  {
    llarp_timer_call_func_later(m_Timer, timeout, func);
  }

  uint32_t
  Logic::call_later(const llarp_timeout_job& job)
  {
    llarp_timeout_job j;
    j.user    = job.user;
    j.timeout = job.timeout;
    j.handler = job.handler;
    return llarp_timer_call_later(m_Timer, j);
  }

  void
  Logic::cancel_call(uint32_t id)
  {
    llarp_timer_cancel_job(m_Timer, id);
  }

  void
  Logic::remove_call(uint32_t id)
  {
    llarp_timer_remove_job(m_Timer, id);
  }

  bool
  Logic::can_flush() const
  {
    return m_ID.value() == std::this_thread::get_id();
  }

}  // namespace llarp
