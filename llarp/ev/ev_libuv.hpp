#ifndef LLARP_EV_LIBUV_HPP
#define LLARP_EV_LIBUV_HPP
#include <ev/ev.hpp>
#include <uv.h>
#include <vector>
#include <functional>
#include <util/thread/logic.hpp>
#include <util/thread/queue.hpp>
#include <util/meta/memfn.hpp>

#include <map>

namespace libuv
{
  class UVWakeup;

  struct Loop final : public llarp::EventLoop
  {
    typedef std::function<void(void)> Callback;

    struct PendingTimer
    {
      uint64_t job_id;
      llarp_time_t delay_ms;
      Callback callback;
    };

    Loop(size_t queue_size);

    bool
    init() override;

    int
    run() override;

    bool
    running() const override;

    void
    update_time() override;

    uint32_t
    call_after_delay(llarp_time_t delay_ms, std::function<void(void)> callback) override;

    void
    cancel_delayed_call(uint32_t job_id) override;

    void
    process_timer_queue();

    void
    process_cancel_queue();

    void
    do_timer_job(uint64_t job_id);

    void
    stop() override;

    void
    stopped() override;

    void
    CloseAll();

    bool
    udp_listen(llarp_udp_io* l, const llarp::SockAddr& src) override;

    bool
    udp_close(llarp_udp_io* l) override;

    bool
    add_ticker(std::function<void(void)> ticker) override;

    bool
    add_network_interface(
        std::shared_ptr<llarp::vpn::NetworkInterface> netif,
        std::function<void(llarp::net::IPPacket)> handler) override;

    void
    set_logic(std::shared_ptr<llarp::Logic> l) override
    {
      m_Logic = l;
      m_Logic->SetQueuer(llarp::util::memFn(&Loop::call_soon, this));
    }

    std::shared_ptr<llarp::Logic> m_Logic;

    void
    call_soon(std::function<void(void)> f) override;

    void
    register_poll_fd_readable(int fd, Callback callback) override;

    void
    deregister_poll_fd_readable(int fd) override;

    void
    set_pump_function(std::function<void(void)> pumpll) override;

    llarp::EventLoopWakeup*
    make_event_loop_waker(std::function<void()> callback) override;

    void
    delete_waker(int idx);

    void
    FlushLogic();

    std::function<void(void)> PumpLL;

   private:
    uv_loop_t m_Impl;
    uv_timer_t* m_TickTimer;
    uv_async_t m_WakeUp;
    std::atomic<bool> m_Run;
    using AtomicQueue_t = llarp::thread::Queue<std::function<void(void)>>;
    AtomicQueue_t m_LogicCalls;

#ifdef LOKINET_DEBUG
    uint64_t last_time;
    uint64_t loop_run_count;
#endif
    std::atomic<uint32_t> m_nextID;

    std::map<uint32_t, Callback> m_pendingCalls;

    std::unordered_map<int, uv_poll_t> m_Polls;

    llarp::thread::Queue<PendingTimer> m_timerQueue;
    llarp::thread::Queue<uint32_t> m_timerCancelQueue;
    std::optional<std::thread::id> m_EventLoopThreadID;

    int m_NumWakers;
    std::unordered_map<int, UVWakeup*> m_Wakers;
  };

}  // namespace libuv

#endif
