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

  void
  Logic::call_later(llarp_time_t timeout, std::function<void(void)> func)
  {
    Call([this, timeout, f = std::move(func)]() mutable {
      m_Loop->call_after_delay(timeout, std::move(f));
    });
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
