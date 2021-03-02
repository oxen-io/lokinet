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

    // Calls the given function once, after the given delay.
    void
    call_later(llarp_time_t later, std::function<void(void)> func);

    // Calls the given function repeatedly, forever, as long as the event loop lasts; the initial
    // call will be after the given delay.
    void
    call_forever(llarp_time_t repeat, std::function<void(void)> func);

    // Created a repeated timer, like call_forever(repeat, func), but ties the lifetime of the
    // callback to `owner`: callbacks will be invoked so long as `owner` remains alive, but
    // thereafter the callback will be destroyed.  Intended to be used as:
    //
    //     logic->call_every(100ms, shared_from_this(), [this] { some_func(); });
    //
    template <typename Callable>
    void
    call_every(llarp_time_t repeat, std::weak_ptr<void> owner, Callable f)
    {
      auto repeater = m_Loop->make_repeater();
      auto& r = *repeater;
      r.start(
          repeat,
          [repeater = std::move(repeater), owner = std::move(owner), f = std::move(f)]() mutable {
            if (auto ptr = owner.lock())
              f();
            else
              repeater.reset();  // Remove timer on destruction (we should be the only thing holder
                                 // the repeater)
          });
    }

    // Wraps a lambda with a lambda that triggers it to be called via Logic::Call()
    // when invoked.  E.g.:
    //
    //     auto x = logic->make_caller([] (int a) { std::cerr << a; });
    //     x(42);
    //     x(99);
    //
    // will schedule two calls of the inner lambda (with different arguments) in the logic thread.
    // Arguments are forwarded to the inner lambda (allowing moving arguments into it).
    template <typename Callable>
    auto
    make_caller(Callable&& f)
    {
      return [this, f = std::forward<Callable>(f)](auto&&... args) {
        // This shared pointer in a pain in the ass but needed because this lambda is going into a
        // std::function that only accepts copyable lambdas.  I *want* to simply capture:
        //     args=std::make_tuple(std::forward<decltype(args)>(args)...)
        // but that fails if any given args aren't copyable.  Dammit.
        auto args_tuple_ptr = std::make_shared<std::tuple<std::decay_t<decltype(args)>...>>(
            std::forward<decltype(args)>(args)...);
        Call([f, args = std::move(args_tuple_ptr)]() mutable {
          // Moving away the tuple args here is okay because this lambda will only be invoked once
          std::apply(f, std::move(*args));
        });
      };
    }

    void
    SetQueuer(std::function<void(std::function<void(void)>)> q);

    EventLoop*
    event_loop()
    {
      return m_Loop;
    }

    void
    set_event_loop(EventLoop* loop);

    void
    clear_event_loop();

    bool
    inLogicThread() const
    {
      return m_Loop and m_Loop->inEventLoopThread();
    }

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
