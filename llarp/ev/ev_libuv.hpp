#ifndef LLARP_EV_LIBUV_HPP
#define LLARP_EV_LIBUV_HPP
#include <ev/ev.hpp>
#include <ev/pipe.hpp>
#include <uv.h>
#include <vector>
#include <functional>
#include <util/thread/logic.hpp>
#include <util/thread/queue.hpp>
#include <util/meta/memfn.hpp>

#include <map>

namespace libuv
{
  struct Loop final : public llarp_ev_loop
  {
    typedef std::function<void(void)> Callback;

    struct PendingTimer
    {
      uint64_t job_id;
      llarp_time_t delay_ms;
      Callback callback;
    };

    Loop();

    bool
    init() override;

    int
    run() override;

    bool
    running() const override;

    void
    update_time() override;

    /// return false on socket error (non blocking)
    bool
    tcp_connect(llarp_tcp_connecter* tcp, const llarp::SockAddr& addr) override;

    int
    tick(int ms) override;

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

    /// deregister event listener
    bool
    close_ev(llarp::ev_io*) override
    {
      return true;
    }

    bool
    tun_listen(llarp_tun_io* tun) override;

    llarp::ev_io*
    create_tun(llarp_tun_io*) override
    {
      return nullptr;
    }

    bool
    tcp_listen(llarp_tcp_acceptor* tcp, const llarp::SockAddr& addr) override;

    bool
    add_pipe(llarp_ev_pkt_pipe* p) override;

    llarp::ev_io*
    bind_tcp(llarp_tcp_acceptor*, const llarp::SockAddr&) override
    {
      return nullptr;
    }

    bool
    add_ticker(std::function<void(void)> ticker) override;

    /// register event listener
    bool
    add_ev(llarp::ev_io*, bool) override
    {
      return false;
    }

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
    FlushLogic();

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

    llarp::thread::Queue<PendingTimer> m_timerQueue;
    llarp::thread::Queue<uint32_t> m_timerCancelQueue;
  };

}  // namespace libuv

#endif
