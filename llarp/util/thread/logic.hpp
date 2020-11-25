#ifndef LLARP_LOGIC_HPP
#define LLARP_LOGIC_HPP

#include <ev/ev.hpp>
#include <util/mem.h>

namespace llarp
{
  class Logic
  {
   public:
    /// stop all operation and wait for that to die
    void
    stop();

    void
    Call(std::function<void(void)> func);

    uint32_t
    call_later(llarp_time_t later, std::function<void(void)> func);

    void
    cancel_call(uint32_t id);

    void
    remove_call(uint32_t id);

    void
    SetQueuer(std::function<void(std::function<void(void)>)> q);

    void
    set_event_loop(EventLoop* loop);

    void
    clear_event_loop();

   private:
    EventLoop* m_Loop = nullptr;
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
