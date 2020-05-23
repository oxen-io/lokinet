#include <util/thread/logic.hpp>
#include <util/logging/logger.hpp>
#include <util/mem.h>

#include <future>

namespace llarp
{
  Logic::Logic(size_t)
  {
  }

  Logic::~Logic()
  {
  }

  size_t
  Logic::numPendingJobs() const
  {
    return 0;
  }

  bool
  Logic::queue_job(struct llarp_thread_job job)
  {
    if (job.user && job.work)
    {
      LogicCall(this, std::bind(job.work, job.user));
      return true;
    }
    return false;
  }

  void
  Logic::stop()
  {
    llarp::LogDebug("logic thread stop");
  }

  void
  Logic::Call(std::function<void(void)> func)
  {
    if (can_flush())
    {
      func();
    }
    else
    {
      m_Queue(std::move(func));
    }
  }

  void
  Logic::SetQueuer(std::function<void(std::function<void(void)>)> q)
  {
    m_Queue = std::move(q);
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
    return *m_ID == std::this_thread::get_id();
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
