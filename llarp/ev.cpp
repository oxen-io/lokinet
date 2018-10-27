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
    return loop->add_ev(dev);
  }
  return false;
}

bool
llarp_tcp_conn_async_write(struct llarp_tcp_conn *conn, const void *pkt,
                           size_t sz)
{
  const byte_t *ptr       = (const byte_t *)pkt;
  llarp::tcp_conn *impl   = static_cast< llarp::tcp_conn * >(conn->impl);
  if(impl->_shouldClose)
    return false;
  while(sz > EV_WRITE_BUF_SZ)
  {
    if(!impl->queue_write((const byte_t *)ptr, EV_WRITE_BUF_SZ))
      return false;
    ptr += EV_WRITE_BUF_SZ;
    sz -= EV_WRITE_BUF_SZ;
  }
  return impl->queue_write(ptr, sz);
}

bool
llarp_tcp_serve(struct llarp_ev_loop *loop, struct llarp_tcp_acceptor *tcp,
                const struct sockaddr *bindaddr)
{
  tcp->loop          = loop;
  llarp::ev_io *impl = loop->bind_tcp(tcp, bindaddr);
  if(impl)
  {
    tcp->impl = impl;
    return loop->add_ev(impl);
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
llarp_ev_tun_async_write(struct llarp_tun_io *tun, const void *buf, size_t sz)
{
  if(sz > EV_WRITE_BUF_SZ)
  {
    llarp::LogWarn("packet too big, ", sz, " > ", EV_WRITE_BUF_SZ);
    return false;
  }
  return static_cast< llarp::ev_io * >(tun->impl)->queue_write(
      (const byte_t *)buf, sz);
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
      if(tcp && tcp->closed)
        tcp->closed(tcp);
      return false;
    }
    else if(tcp->tick)
      tcp->tick(tcp);
    return true;
  }

  int
  tcp_serv::read(void *, size_t)
  {
    int new_fd = ::accept(fd, nullptr, nullptr);
    if(new_fd == -1)
    {
      llarp::LogError("failed to accept on ", fd, ":", strerror(errno));
      return -1;
    }

    llarp_tcp_conn *conn = new llarp_tcp_conn;
    // zero out callbacks
    conn->tick   = nullptr;
    conn->closed = nullptr;
    conn->read   = nullptr;
    // build handler
    llarp::tcp_conn *connimpl = new tcp_conn(new_fd, conn);
    conn->impl                = connimpl;
    conn->loop                = loop;
    if(loop->add_ev(connimpl, true))
    {
      // call callback
      if(tcp->accepted)
        tcp->accepted(tcp, conn);
      return 0;
    }
    // cleanup error
    delete conn;
    delete connimpl;
    return -1;
  }

}  // namespace llarp

  
llarp::ev_io*
llarp_ev_loop::bind_tcp(llarp_tcp_acceptor* tcp, const sockaddr* bindaddr)
{
  int fd = ::socket(bindaddr->sa_family, SOCK_STREAM, 0);
  if(fd == -1)
    return nullptr;
  socklen_t sz = sizeof(sockaddr_in);
  if(bindaddr->sa_family == AF_INET6)
  {
      sz = sizeof(sockaddr_in6);
  }
  else if(bindaddr->sa_family == AF_UNIX)
  {
    sz = sizeof(sockaddr_un);
  }
  if(::bind(fd, bindaddr, sz) == -1)
  {
    ::close(fd);
    return nullptr;
  }
  if(::listen(fd, 5) == -1)
  {
    ::close(fd);
    return nullptr;
  }
  llarp::ev_io* serv = new llarp::tcp_serv(this, fd, tcp);
  tcp->impl          = serv;
  return serv;
}
