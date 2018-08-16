#include <llarp/ev.h>
#include <llarp/logic.h>
#include "mem.hpp"

#define EV_TICK_INTERVAL 100

#ifdef __linux__
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
#ifdef __linux__
  *ev = new llarp_epoll_loop;
#endif
#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) \
    || (__APPLE__ && __MACH__)
  *ev = new llarp_kqueue_loop;
#endif
#if defined(_WIN32) || defined(_WIN64) || defined(__NT__)
  *ev = new llarp_win32_loop;
#endif
  // a) I assume that the libre fork of Solaris is still
  // 5.10, and b) the current commercial version is 5.11, naturally.
  // -despair86
#if defined(__SunOS_5_10) || defined(__SunOS_5_11)
  *ev = new llarp_sun_iocp_loop;
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
  while(true)
  {
    if(ev->tick(EV_TICK_INTERVAL) == -1)
      return -1;
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
    llarp_logic_tick(logic);
    llarp_threadpool_tick(tp);
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
  if(ret == -1)
  {
    llarp::LogWarn("sendto failed ", strerror(errno));
    errno = 0;
  }
  return ret;
}

bool
llarp_ev_add_tun(struct llarp_ev_loop *loop, struct llarp_tun_io *tun)
{
  return loop->create_tun(tun);
}

bool
llarp_ev_tun_async_write(struct llarp_tun_io *tun, const void *pkt, size_t sz)
{
  return static_cast< llarp::ev_io * >(tun->impl)->queue_write(pkt, sz);
}