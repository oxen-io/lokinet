#include <util/thread/logic.hpp>
#include <util/thread/timer.hpp>
#include <util/logging/logger.hpp>
#include <util/mem.h>
#include <util/metrics/metrics.hpp>

#include <future>

namespace llarp
{
  void
  Logic::tick(llarp_time_t now)
  {
    if(m_Queue)
    {
      llarp_timer_set_time(m_Timer, now);
      if(llarp_timer_should_call(m_Timer))
        m_Queue(std::bind(&llarp_timer_tick_all, m_Timer));
      return;
    }
    llarp_timer_tick_all_async(m_Timer, m_Thread, now);
  }

  Logic::Logic(size_t sz)
      : m_Thread(llarp_init_threadpool(1, "llarp-logic", sz))
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

  size_t
  Logic::numPendingJobs() const
  {
    return m_Thread->pendingJobs();
  }

  bool
  Logic::queue_job(struct llarp_thread_job job)
  {
    return job.user && job.work
        && LogicCall(this, std::bind(job.work, job.user));
  }

  void
  Logic::stop()
  {
    llarp::LogDebug("logic thread stop");
    // stop all timers from happening in the future
    LogicCall(this, std::bind(&llarp_timer_stop, m_Timer));
    // stop all operations on threadpool
    llarp_threadpool_stop(m_Thread);
  }

  bool
  Logic::_traceLogicCall(std::function< void(void) > func, const char* tag,
                         int line)
  {
#define TAG (tag ? tag : LOG_TAG)
#define LINE (line ? line : __LINE__)
    // wrap the function so that we ensure that it's always calling stuff one at
    // a time

#if defined(LOKINET_DEBUG)
#define METRIC(action)                                         \
  metrics::integerTick("logic", action, 1, "tag", TAG, "line", \
                       std::to_string(LINE))
#else
#define METRIC(action) \
  do                   \
  {                    \
  } while(false)
#endif

    METRIC("queue");
    auto f = [self = this, func, tag, line]() {
#if defined(LOKINET_DEBUG)
      metrics::TimerGuard g("logic",
                            std::string(TAG) + ":" + std::to_string(LINE));
#endif
      self->m_Killer.TryAccess(func);
    };
    if(can_flush())
    {
      METRIC("fired");
      f();
      return true;
    }
    if(m_Queue)
    {
      m_Queue(f);
      return true;
    }
    if(m_Thread->LooksFull(5))
    {
      LogErrorExplicit(TAG, LINE,
                       "holy crap, we are trying to queue a job "
                       "onto the logic thread but it looks full");
      METRIC("full");
      std::abort();
    }
    auto ret = llarp_threadpool_queue_job(m_Thread, f);
    if(not ret)
    {
      METRIC("dropped");
    }
    return ret;
#undef TAG
#undef LINE
#undef METRIC
  }

  void
  Logic::SetQueuer(std::function< void(std::function< void(void) >) > q)
  {
    m_Queue = q;
    m_Queue([self = this]() { self->m_ID = std::this_thread::get_id(); });
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
