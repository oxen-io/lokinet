#include <llarp/ev.h>
#include <llarp/logic.h>
#include "mem.hpp"

#define EV_TICK_INTERVAL 100

// apparently current Solaris will emulate epoll.
#if __linux__ || __sun__
#include "ev_epoll.hpp"
#endif
#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) \
    || (__APPLE__ && __MACH__)
#include "ev_kqueue.hpp"
#endif
#if defined(_WIN32) || defined(_WIN64) || defined(__NT__)
#include "ev_win32.hpp"
#endif

void
llarp_ev_loop_alloc(struct llarp_ev_loop **ev)
{
#if __linux__ || __sun__
  *ev = new llarp_epoll_loop;
#endif
#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) \
    || (__APPLE__ && __MACH__)
  *ev = new llarp_kqueue_loop;
#endif
#if defined(_WIN32) || defined(_WIN64) || defined(__NT__)
  *ev = new llarp_win32_loop;
#endif
  (*ev)->init();
}

void
llarp_ev_loop_free(struct llarp_ev_loop **ev)
{
  delete *ev;
  *ev = nullptr;
}

int
llarp_ev_loop_run(struct llarp_ev_loop *ev, struct llarp_logic *logic)
{
  while(ev->running())
  {
    ev->tick(EV_TICK_INTERVAL);
    if(ev->running())
      llarp_logic_tick(logic);
  }
  return 0;
}

void
llarp_ev_loop_run_single_process(struct llarp_ev_loop *ev,
                                 struct llarp_threadpool *tp,
                                 struct llarp_logic *logic)
{
  while(ev->running())
  {
    ev->tick(EV_TICK_INTERVAL);
    if(ev->running())
    {
      llarp_logic_tick_async(logic);
      llarp_threadpool_tick(tp);
    }
  }
}

int
llarp_ev_add_udp(struct llarp_ev_loop *ev, struct llarp_udp_io *udp,
                 const struct sockaddr *src)
{
  udp->parent = ev;
  if(ev->udp_listen(udp, src))
    return 0;
  return -1;
}

int
llarp_ev_close_udp(struct llarp_udp_io *udp)
{
  if(udp->parent->udp_close(udp))
    return 0;
  return -1;
}

void
llarp_ev_loop_stop(struct llarp_ev_loop *loop)
{
  loop->stop();
}

int
llarp_ev_udp_sendto(struct llarp_udp_io *udp, const sockaddr *to,
                    const void *buf, size_t sz)
{
  auto ret = static_cast< llarp::ev_io * >(udp->impl)->sendto(to, buf, sz);
  if(ret == -1 || errno)
  {
    llarp::LogWarn("sendto failed ", strerror(errno));
    errno = 0;
  }
  return ret;
}

bool
llarp_ev_add_tun(struct llarp_ev_loop *loop, struct llarp_tun_io *tun)
{
  auto dev  = loop->create_tun(tun);
  tun->impl = dev;
  if(dev)
  {
    return loop->add_ev(dev, true);
  }
  return false;
}

bool
llarp_ev_tun_async_write(struct llarp_tun_io *tun, const void *pkt, size_t sz)
{
  // TODO: queue write
  return static_cast< llarp::ev_io * >(tun->impl)->do_write((void *)pkt, sz);
}

bool
llarp_tcp_serve(struct llarp_tcp_acceptor *tcp, const struct sockaddr *bindaddr)
{
  // TODO: implement me
  return false;
}

void
llarp_tcp_acceptor_close(struct llarp_tcp_acceptor *tcp)
{
  // TODO: implement me
}

void
llarp_tcp_conn_close(struct llarp_tcp_conn *conn)
{
  if(!conn)
    return;
  llarp::ev_io *impl = static_cast< llarp::ev_io * >(conn->impl);
  conn->impl         = nullptr;
  // deregister
  conn->loop->close_ev(impl);
  // close fd and delete impl
  delete impl;
  // call hook if needed
  if(conn->closed)
    conn->closed(conn);
  // delete
  delete conn;
}
