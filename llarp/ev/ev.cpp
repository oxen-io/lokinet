#include <ev/ev.h>
#include <util/mem.hpp>
#include <util/string_view.hpp>
#include <util/thread/logic.hpp>
#include <net/net_addr.hpp>

#include <cstddef>
#include <cstring>

// We libuv now
#include <ev/ev_libuv.hpp>
#if defined(_WIN32) || defined(_WIN64) || defined(__NT__)
#define SHUT_RDWR SD_BOTH
#include <ev/ev_win32.hpp>
#endif

llarp_ev_loop_ptr
llarp_make_ev_loop()
{
  llarp_ev_loop_ptr r = std::make_shared< libuv::Loop >();
  r->init();
  r->update_time();
  return r;
}

void
llarp_ev_loop_run_single_process(llarp_ev_loop_ptr ev,
                                 std::shared_ptr< llarp::Logic > logic)
{
  while(ev->running())
  {
    ev->update_time();
    ev->tick(EV_TICK_INTERVAL);
    llarp::LogContext::Instance().logStream->Tick(ev->time_now());
  }
  logic->clear_event_loop();
  ev->stopped();
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
llarp_ev_loop_time_now_ms(const llarp_ev_loop_ptr &loop)
{
  if(loop)
    return loop->time_now();
  return llarp::time_now_ms();
}

void
llarp_ev_loop_stop(const llarp_ev_loop_ptr &loop)
{
  loop->stop();
}

int
llarp_ev_udp_sendto(struct llarp_udp_io *udp, const sockaddr *to,
                    const llarp_buffer_t &buf)
{
  return udp->sendto(udp, to, buf.base, buf.sz);
}

bool
llarp_ev_add_tun(struct llarp_ev_loop *loop, struct llarp_tun_io *tun)
{
  if(tun->ifaddr[0] == 0 || strcmp(tun->ifaddr, "auto") == 0)
  {
    LogError("invalid ifaddr on tun: ", tun->ifaddr);
    return false;
  }
  if(tun->ifname[0] == 0 || strcmp(tun->ifname, "auto") == 0)
  {
    LogError("invalid ifname on tun: ", tun->ifname);
    return false;
  }
#if !defined(_WIN32)
  return loop->tun_listen(tun);
#else
  UNREFERENCED_PARAMETER(loop);
  auto dev  = new win32_tun_io(tun);
  tun->impl = dev;
  // We're not even going to add this to the socket event loop
  if(dev)
  {
    dev->setup();
    return dev->add_ev(loop);  // start up tun and add to event queue
  }
  llarp::LogWarn("Loop could not create tun");
  return false;
#endif
}

bool
llarp_ev_tun_async_write(struct llarp_tun_io *tun, const llarp_buffer_t &buf)
{
  if(buf.sz > EV_WRITE_BUF_SZ)
  {
    llarp::LogWarn("packet too big, ", buf.sz, " > ", EV_WRITE_BUF_SZ);
    return false;
  }
#ifndef _WIN32
  return tun->writepkt(tun, buf.base, buf.sz);
#else
  return static_cast< win32_tun_io * >(tun->impl)->queue_write(buf.base,
                                                               buf.sz);
#endif
}

bool
llarp_tcp_conn_async_write(struct llarp_tcp_conn *conn, const llarp_buffer_t &b)
{
  ManagedBuffer buf{b};

  size_t sz          = buf.underlying.sz;
  buf.underlying.cur = buf.underlying.base;
  while(sz > EV_WRITE_BUF_SZ)
  {
    ssize_t amount = conn->write(conn, buf.underlying.cur, EV_WRITE_BUF_SZ);
    if(amount <= 0)
    {
      llarp::LogError("write underrun");
      llarp_tcp_conn_close(conn);
      return false;
    }
    buf.underlying.cur += amount;
    sz -= amount;
  }
  return conn->write(conn, buf.underlying.cur, sz) > 0;
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
  tcp->loop = loop;
  return loop->tcp_listen(tcp, bindaddr);
}

void
llarp_tcp_acceptor_close(struct llarp_tcp_acceptor *tcp)
{
  tcp->close(tcp);
}

void
llarp_tcp_conn_close(struct llarp_tcp_conn *conn)
{
  conn->close(conn);
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
      ::shutdown(fd, SHUT_RDWR);
      return false;
    }
    if(tcp.tick)
      tcp.tick(&tcp);
    return true;
  }

}  // namespace llarp
