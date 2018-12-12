#include <ev.h>
#include <logic.hpp>
#include <mem.hpp>
#include <string_view.hpp>

#include <stddef.h>


#define EV_TICK_INTERVAL 100

// apparently current Solaris will emulate epoll.
#if __linux__ || __sun__
#include "ev_epoll.hpp"
#elif defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) \
    || (__APPLE__ && __MACH__)
#include "ev_kqueue.hpp"
#elif defined(_WIN32) || defined(_WIN64) || defined(__NT__)
#include "ev_win32.hpp"
#else
#error No async event loop for your platform, subclass llarp_ev_loop
#endif

void
llarp_ev_loop_alloc(struct llarp_ev_loop **ev)
{
#if __linux__ || __sun__
  *ev = new llarp_epoll_loop;
#elif defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) \
    || (__APPLE__ && __MACH__)
  *ev = new llarp_kqueue_loop;
#elif defined(_WIN32) || defined(_WIN64) || defined(__NT__)
  *ev = new llarp_win32_loop;
#else
#error no event loop subclass
#endif
  (*ev)->init();
  (*ev)->_now = llarp::time_now_ms();
}

void
llarp_ev_loop_free(struct llarp_ev_loop **ev)
{
  delete *ev;
  *ev = nullptr;
}

int
llarp_ev_loop_run(struct llarp_ev_loop *ev, llarp::Logic *logic)
{
  while(ev->running())
  {
    ev->_now = llarp::time_now_ms();
    ev->tick(EV_TICK_INTERVAL);
    if(ev->running())
      logic->tick(ev->_now);
  }
  return 0;
}

int
llarp_fd_promise_wait_for_value(struct llarp_fd_promise *p)
{
  return p->Get();
}

void
llarp_ev_loop_run_single_process(struct llarp_ev_loop *ev,
                                 struct llarp_threadpool *tp,
                                 llarp::Logic *logic)
{
  while(ev->running())
  {
    ev->_now = llarp::time_now_ms();
    ev->tick(EV_TICK_INTERVAL);
    if(ev->running())
    {
      logic->tick_async(ev->_now);
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

llarp_time_t
llarp_ev_loop_time_now_ms(struct llarp_ev_loop *loop)
{
  if(loop)
    return loop->_now;
  return llarp::time_now_ms();
}

void
llarp_ev_loop_stop(struct llarp_ev_loop *loop)
{
  loop->stop();
}

int
llarp_ev_udp_sendto(struct llarp_udp_io *udp, const sockaddr *to,
                    llarp_buffer_t buf)
{
  auto ret =
      static_cast< llarp::ev_io * >(udp->impl)->sendto(to, buf.base, buf.sz);
#ifndef _WIN32
  if(ret == -1 && errno != 0)
  {
#else
  if(ret == -1 && WSAGetLastError())
  {
#endif

#ifndef _WIN32
    llarp::LogWarn("sendto failed ", strerror(errno));
    errno = 0;
  }
#else
    char ebuf[1024];
    int err = WSAGetLastError();
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, err, LANG_NEUTRAL, ebuf,
                  1024, nullptr);
    llarp::LogWarn("sendto failed: ", ebuf);
    WSASetLastError(0);
  }
#endif
  return ret;
}

bool
llarp_ev_add_tun(struct llarp_ev_loop *loop, struct llarp_tun_io *tun)
{
  auto dev  = loop->create_tun(tun);
  tun->impl = dev;
  if(dev)
    return loop->add_ev(dev, false);
  return false;
}

bool
llarp_tcp_conn_async_write(struct llarp_tcp_conn *conn, llarp_buffer_t buf)
{
  llarp::tcp_conn *impl = static_cast< llarp::tcp_conn * >(conn->impl);
  if(impl->_shouldClose)
  {
    llarp::LogError("write on closed connection");
    return false;
  }
  size_t sz = buf.sz;
  buf.cur   = buf.base;
  while(sz > EV_WRITE_BUF_SZ)
  {
    if(!impl->queue_write(buf.cur, EV_WRITE_BUF_SZ))
      return false;
    buf.cur += EV_WRITE_BUF_SZ;
    sz -= EV_WRITE_BUF_SZ;
  }
  return impl->queue_write(buf.cur, sz);
}

void
llarp_tcp_async_try_connect(struct llarp_ev_loop *loop,
                            struct llarp_tcp_connecter *tcp)
{
  tcp->loop = loop;
  llarp::string_view addr_str, port_str;
  // try parsing address
  const char *begin = tcp->remote;
  const char *ptr   = strstr(tcp->remote, ":");
  // get end of address

  if(ptr == nullptr)
  {
    llarp::LogError("bad address: ", tcp->remote);
    if(tcp->error)
      tcp->error(tcp);
    return;
  }
  const char *end = ptr;
  while(*end && ((end - begin) < static_cast< ptrdiff_t >(sizeof tcp->remote)))
  {
    ++end;
  }
  addr_str = llarp::string_view(begin, ptr - begin);
  ++ptr;
  port_str = llarp::string_view(ptr, end - ptr);
  // actually parse address
  llarp::Addr addr(addr_str, port_str);

  if(!loop->tcp_connect(tcp, addr))
  {
    llarp::LogError("async connect failed");
    if(tcp->error)
      tcp->error(tcp);
  }
}

bool
llarp_tcp_serve(struct llarp_ev_loop *loop, struct llarp_tcp_acceptor *tcp,
                const struct sockaddr *bindaddr)
{
  tcp->loop          = loop;
  llarp::ev_io *impl = loop->bind_tcp(tcp, bindaddr);
  if(impl)
  {
    return loop->add_ev(impl, false);
  }
  return false;
}

void
llarp_tcp_acceptor_close(struct llarp_tcp_acceptor *tcp)
{
  llarp::ev_io *impl = static_cast< llarp::ev_io * >(tcp->user);
  tcp->impl          = nullptr;
  tcp->loop->close_ev(impl);
  if(tcp->closed)
    tcp->closed(tcp);
  // dont free acceptor because it may be stack allocated
}

bool
llarp_ev_tun_async_write(struct llarp_tun_io *tun, llarp_buffer_t buf)
{
  if(buf.sz > EV_WRITE_BUF_SZ)
  {
    llarp::LogWarn("packet too big, ", buf.sz, " > ", EV_WRITE_BUF_SZ);
    return false;
  }
  return static_cast< llarp::tun * >(tun->impl)->queue_write(buf.base, buf.sz);
}

void
llarp_tcp_conn_close(struct llarp_tcp_conn *conn)
{
  static_cast< llarp::tcp_conn * >(conn->impl)->_shouldClose = true;
}

namespace llarp
{
  bool
  tcp_conn::tick()
  {
    if(_shouldClose)
    {
      if(tcp.closed)
        tcp.closed(&tcp);
      return false;
    }
    else if(tcp.tick)
      tcp.tick(&tcp);
    return true;
  }

}  // namespace llarp
