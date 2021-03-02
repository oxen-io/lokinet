#ifndef LLARP_EV_LIBUV_HPP
#define LLARP_EV_LIBUV_HPP
#include <ev/ev.hpp>
#include "udp_handle.hpp"
#include <util/thread/logic.hpp>
#include <util/thread/queue.hpp>
#include <util/meta/memfn.hpp>

#include <uvw/loop.h>
#include <uvw/async.h>
#include <uvw/poll.h>
#include <uvw/udp.h>

#include <functional>
#include <map>
#include <vector>

namespace llarp::uv
{
  class UVWakeup;
  class UVRepeater;

  struct Loop final : public llarp::EventLoop
  {
    using Callback = std::function<void()>;

    Loop(size_t queue_size);

    void
    run_loop() override;

    bool
    running() const override;

    void
    call_after_delay(llarp_time_t delay_ms, std::function<void(void)> callback) override;

    void
    tick_event_loop();

    void
    stop() override;

    void
    stopped() override;

    bool
    add_ticker(std::function<void(void)> ticker) override;

    bool
    add_network_interface(
        std::shared_ptr<llarp::vpn::NetworkInterface> netif,
        std::function<void(llarp::net::IPPacket)> handler) override;

    void
    set_logic(const std::shared_ptr<llarp::Logic>& l) override
    {
      l->SetQueuer([this](std::function<void()> f) { call_soon(std::move(f)); });
    }

    void
    call_soon(std::function<void(void)> f) override;

    void
    set_pump_function(std::function<void(void)> pumpll) override;

    std::shared_ptr<llarp::EventLoopWakeup>
    make_waker(std::function<void()> callback) override;

    std::shared_ptr<EventLoopRepeater>
    make_repeater() override;

    std::shared_ptr<llarp::UDPHandle>
    udp(UDPReceiveFunc on_recv) override;

    void
    FlushLogic();

    std::function<void(void)> PumpLL;

    bool
    inEventLoopThread() const override;

   private:
    std::shared_ptr<uvw::Loop> m_Impl;
    std::shared_ptr<uvw::AsyncHandle> m_WakeUp;
    std::atomic<bool> m_Run;
    using AtomicQueue_t = llarp::thread::Queue<std::function<void(void)>>;
    AtomicQueue_t m_LogicCalls;

#ifdef LOKINET_DEBUG
    uint64_t last_time;
    uint64_t loop_run_count;
#endif
    std::atomic<uint32_t> m_nextID;

    std::map<uint32_t, Callback> m_pendingCalls;

    std::unordered_map<int, std::shared_ptr<uvw::PollHandle>> m_Polls;

    std::optional<std::thread::id> m_EventLoopThreadID;
  };

}  // namespace llarp::uv

#endif
