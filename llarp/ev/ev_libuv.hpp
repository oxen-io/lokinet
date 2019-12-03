#ifndef LLARP_EV_LIBUV_HPP
#define LLARP_EV_LIBUV_HPP
#include <ev/ev.hpp>
#include <ev/pipe.hpp>
#include <uv.h>
#include <vector>
#include <functional>
#include <util/thread/logic.hpp>

namespace libuv
{
  struct Loop final : public llarp_ev_loop
  {
    bool
    init() override;

    int
    run() override
    {
      return -1;
    }

    bool
    running() const override;

    void
    update_time() override;

    /// return false on socket error (non blocking)
    bool
    tcp_connect(llarp_tcp_connecter* tcp, const sockaddr* addr) override;

    int
    tick(int ms) override;

    void
    stop() override;

    void
    stopped() override;

    void
    CloseAll();

    bool
    udp_listen(llarp_udp_io* l, const sockaddr* src) override;

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
    tcp_listen(llarp_tcp_acceptor* tcp, const sockaddr* addr) override;

    bool
    add_pipe(llarp_ev_pkt_pipe* p) override;

    llarp::ev_io*
    bind_tcp(llarp_tcp_acceptor*, const sockaddr*) override
    {
      return nullptr;
    }

    bool
    add_ticker(std::function< void(void) > ticker) override;

    /// register event listener
    bool
    add_ev(llarp::ev_io*, bool) override
    {
      return false;
    }

    void
    set_logic(std::shared_ptr< llarp::Logic > l) override
    {
      m_Logic = l;
    }

    std::shared_ptr< llarp::Logic > m_Logic;

   private:
    uv_loop_t m_Impl;
    uv_timer_t m_TickTimer;
    std::atomic< bool > m_Run;

#ifdef LOKINET_DEBUG
    uint64_t last_time;
    uint64_t loop_run_count;
#endif
  };

}  // namespace libuv

#endif
