#include <util/thread/logic.hpp>
#include <util/logging/logger.hpp>
#include <util/mem.h>

#include <future>

namespace llarp
{
  Logic::Logic(size_t sz) : m_Thread(llarp_init_threadpool(1, "llarp-logic", sz))
  {
    llarp_threadpool_start(m_Thread);
    /// set thread id
    std::promise<ID_t> result;
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
  }

  size_t
  Logic::numPendingJobs() const
  {
    return m_Thread->pendingJobs();
  }

  bool
  Logic::queue_job(struct llarp_thread_job job)
  {
    return job.user && job.work && LogicCall(this, std::bind(job.work, job.user));
  }

  void
  Logic::stop()
  {
    llarp::LogDebug("logic thread stop");
    // stop all operations on threadpool
    llarp_threadpool_stop(m_Thread);
  }

  bool
  Logic::_traceLogicCall(std::function<void(void)> func, const char* tag, int line)
  {
    // wrap the function so that we ensure that it's always calling stuff one at
    // a time

    auto f = [self = this, func]() {
      if (self->m_Queue)
      {
        func();
      }
      else
      {
        self->m_Killer.TryAccess(func);
      }
    };
    if (can_flush())
    {
      f();
      return true;
    }
    if (m_Queue)
    {
      m_Queue(f);
      return true;
    }
    if (m_Thread->LooksFull(5))
    {
      LogErrorExplicit(
          tag ? tag : LOG_TAG,
          line ? line : __LINE__,
          "holy crap, we are trying to queue a job "
          "onto the logic thread but it looks full");
      std::abort();
    }
    auto ret = llarp_threadpool_queue_job(m_Thread, f);
    if (not ret)
    {
    }
    return ret;
  }

  void
  Logic::SetQueuer(std::function<void(std::function<void(void)>)> q)
  {
    m_Queue = q;
    m_Queue([self = this]() { self->m_ID = std::this_thread::get_id(); });
  }

  uint32_t
  Logic::call_later(llarp_time_t timeout, std::function<void(void)> func)
  {
    auto loop = m_Loop;
    if (loop != nullptr)
    {
      return loop->call_after_delay(timeout, func);
    }
    return 0;
  }

  void
  Logic::cancel_call(uint32_t id)
  {
    auto loop = m_Loop;
    if (loop != nullptr)
    {
      loop->cancel_delayed_call(id);
    }
  }

  void
  Logic::remove_call(uint32_t id)
  {
    auto loop = m_Loop;
    if (loop != nullptr)
    {
      loop->cancel_delayed_call(id);
    }
  }

  bool
  Logic::can_flush() const
  {
    return m_ID.value() == std::this_thread::get_id();
  }

  void
  Logic::set_event_loop(llarp_ev_loop* loop)
  {
    m_Loop = loop;
  }

  void
  Logic::clear_event_loop()
  {
    m_Loop = nullptr;
  }

}  // namespace llarp
