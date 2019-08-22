#ifndef LLARP_EV_LIBUV_HPP
#define LLARP_EV_LIBUV_HPP
#include <ev/ev.hpp>
#include <ev/pipe.hpp>
#include <uv.h>
#include <vector>
#include <functional>

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

    llarp_time_t
    time_now() const override;

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

    /// register event listener
    bool
    add_ev(llarp::ev_io*, bool) override
    {
      return false;
    }

   private:
    struct DestructLoop
    {
      void
      operator()(uv_loop_t* l) const
      {
        uv_loop_close(l);
      }
    };

    std::unique_ptr< uv_loop_t, DestructLoop > m_Impl;
    uv_timer_t m_TickTimer;
    std::atomic< bool > m_Run;
  };

}  // namespace libuv

#endif
