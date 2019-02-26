#include <ev/ev.h>
#include <util/logic.hpp>
#include <util/mem.hpp>
#include <util/string_view.hpp>

#include <stddef.h>

// apparently current Solaris will emulate epoll.
#if __linux__ || __sun__
#include <ev/ev_epoll.hpp>
#elif defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) \
    || (__APPLE__ && __MACH__)
#include <ev/ev_kqueue.hpp>
#elif defined(_WIN32) || defined(_WIN64) || defined(__NT__)
#include <ev/ev_win32.hpp>
#else
#error No async event loop for your platform, subclass llarp_ev_loop
#endif

// This is dead now isn't it -rick
void
llarp_ev_loop_alloc(struct llarp_ev_loop **ev)
{
#if __linux__ || __sun__
  *ev = new llarp_epoll_loop;
#elif defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) \
    || (__APPLE__ && __MACH__)
  *ev                                = new llarp_kqueue_loop;
#elif defined(_WIN32) || defined(_WIN64) || defined(__NT__)
  *ev                                = new llarp_win32_loop;
#else
// TODO: fall back to a generic select-based event loop
#error no event loop subclass
#endif
  (*ev)->init();
  (*ev)->_now = llarp::time_now_ms();
}

std::unique_ptr< llarp_ev_loop >
llarp_make_ev_loop()
{
#if __linux__ || __sun__
  std::unique_ptr< llarp_ev_loop > r = std::make_unique< llarp_epoll_loop >();
#elif defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) \
    || (__APPLE__ && __MACH__)
  std::unique_ptr< llarp_ev_loop > r = std::make_unique< llarp_kqueue_loop >();
#elif defined(_WIN32) || defined(_WIN64) || defined(__NT__)
  std::unique_ptr< llarp_ev_loop > r = std::make_unique< llarp_win32_loop >();
#else
// TODO: fall back to a generic select-based event loop
#error no event loop subclass
#endif
  r->init();
  r->_now = llarp::time_now_ms();

  return r;
}

void
llarp_ev_loop_free(struct llarp_ev_loop **ev)
{
  delete *ev;
  *ev = nullptr;
#ifdef _WIN32
  exit_tun_loop();
#endif
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
      ev->_now = llarp::time_now_ms();
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
                    const llarp_buffer_t &buf)
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

#ifndef _WIN32
#include <string.h>

bool
llarp_ev_add_tun(struct llarp_ev_loop *loop, struct llarp_tun_io *tun)
{
  // llarp::LogInfo("ev creating tunnel ", tun->ifaddr, " on ", tun->ifname);
  if(strcmp(tun->ifaddr, "") == 0 || strcmp(tun->ifaddr, "auto") == 0)
  {
    std::string ifaddr = llarp::findFreePrivateRange();
    auto pos           = ifaddr.find("/");
    if(pos == std::string::npos)
    {
      llarp::LogWarn("Auto ifaddr didn't return a netmask: ", ifaddr);
      return false;
    }
    int num;
    std::string part = ifaddr.substr(pos + 1);
#if defined(ANDROID) || defined(RPI)
    num = atoi(part.c_str());
#else
    num = std::stoi(part);
#endif
    if(num <= 0)
    {
      llarp::LogError("bad ifaddr netmask value: ", ifaddr);
      return false;
    }
    tun->netmask           = num;
    const std::string addr = ifaddr.substr(0, pos);
    std::copy_n(addr.begin(), std::min(sizeof(tun->ifaddr), addr.size()),
                tun->ifaddr);
    llarp::LogInfo("IfAddr autodetect: ", tun->ifaddr, "/", tun->netmask);
  }
  if(strcmp(tun->ifname, "") == 0 || strcmp(tun->ifname, "auto") == 0)
  {
    std::string ifname = llarp::findFreeLokiTunIfName();
    std::copy_n(ifname.begin(), std::min(sizeof(tun->ifname), ifname.size()),
                tun->ifname);
    llarp::LogInfo("IfName autodetect: ", tun->ifname);
  }
  llarp::LogDebug("Tun Interface will use the following settings:");
  llarp::LogDebug("IfAddr: ", tun->ifaddr);
  llarp::LogDebug("IfName: ", tun->ifname);
  llarp::LogDebug("IfNMsk: ", tun->netmask);
  auto dev  = loop->create_tun(tun);
  tun->impl = dev;
  if(dev)
  {
    return loop->add_ev(dev, false);
  }
  llarp::LogWarn("Loop could not create tun");
  return false;
}
#else
// OK, now it's time to do it my way.
// we're not even going to use the existing llarp::tun
// we still use the llarp_tun_io struct
// since we still need to branch to the
// packet processing functions
bool
llarp_ev_add_tun(llarp_ev_loop *loop, llarp_tun_io *tun)
{
  UNREFERENCED_PARAMETER(loop);
  auto dev = new win32_tun_io(tun);
  tun->impl = dev;
  // We're not even going to add this to the socket event loop
  if(dev)
  {
    dev->setup();
    return dev->add_ev();  // start up tun and add to event queue
  }
  return false;
}
#endif

#ifndef _WIN32
bool
llarp_ev_tun_async_write(struct llarp_tun_io *tun, const llarp_buffer_t &buf)
{
  if(buf.sz > EV_WRITE_BUF_SZ)
  {
    llarp::LogWarn("packet too big, ", buf.sz, " > ", EV_WRITE_BUF_SZ);
    return false;
  }
  return static_cast< llarp::tun * >(tun->impl)->queue_write(buf.base, buf.sz);
}
#else
bool
llarp_ev_tun_async_write(struct llarp_tun_io *tun, const llarp_buffer_t &buf)
{
  if(buf.sz > EV_WRITE_BUF_SZ)
  {
    llarp::LogWarn("packet too big, ", buf.sz, " > ", EV_WRITE_BUF_SZ);
    return false;
  }
  return static_cast< win32_tun_io * >(tun->impl)->queue_write(buf.base,
                                                               buf.sz);
}
#endif

bool
llarp_tcp_conn_async_write(struct llarp_tcp_conn *conn, const llarp_buffer_t &b)
{
  ManagedBuffer buf{b};
  llarp::tcp_conn *impl = static_cast< llarp::tcp_conn * >(conn->impl);
  if(impl->_shouldClose)
  {
    llarp::LogError("write on closed connection");
    return false;
  }
  size_t sz          = buf.underlying.sz;
  buf.underlying.cur = buf.underlying.base;
  while(sz > EV_WRITE_BUF_SZ)
  {
    if(!impl->queue_write(buf.underlying.cur, EV_WRITE_BUF_SZ))
    {
      return false;
    }
    buf.underlying.cur += EV_WRITE_BUF_SZ;
    sz -= EV_WRITE_BUF_SZ;
  }
  return impl->queue_write(buf.underlying.cur, sz);
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
