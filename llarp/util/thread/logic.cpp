#include <util/thread/logic.hpp>
#include <util/logging/logger.hpp>
#include <util/mem.h>

#include <future>

namespace llarp
{
  void
  Logic::stop()
  {
    llarp::LogDebug("logic thread stop");
  }

  void
  Logic::Call(std::function<void(void)> func)
  {
    m_Queue(std::move(func));
  }

  void
  Logic::SetQueuer(std::function<void(std::function<void(void)>)> q)
  {
    m_Queue = std::move(q);
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

  void
  Logic::set_event_loop(EventLoop* loop)
  {
    m_Loop = loop;
    SetQueuer([loop](std::function<void(void)> work) { loop->call_soon(work); });
  }

  void
  Logic::clear_event_loop()
  {
    m_Loop = nullptr;
  }

}  // namespace llarp
