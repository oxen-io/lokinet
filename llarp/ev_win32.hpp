#ifndef EV_WIN32_HPP
#define EV_WIN32_HPP
#include <llarp/buffer.h>
#include <llarp/net.h>
#include <windows.h>
#include <cstdio>
#include <llarp/net.hpp>
#include "ev.hpp"
#include "logger.hpp"

#ifdef sizeof
#undef sizeof
#endif

namespace llarp
{
  int
  tcp_conn::read(byte_t* buf, size_t sz)
  {
    if(_shouldClose)
      return -1;

    ssize_t amount = uread(fd.socket, (char*)buf, sz);

    if(amount > 0)
    {
      if(tcp.read)
        tcp.read(&tcp, llarp::InitBuffer(buf, amount));
    }
    else
    {
      // error
      _shouldClose = true;
      return -1;
    }
    return 0;
  }

  void
  tcp_conn::flush_write()
  {
    connected();
    ev_io::flush_write();
  }

  ssize_t
  tcp_conn::do_write(void* buf, size_t sz)
  {
    if(_shouldClose)
      return -1;
    return uwrite(fd.socket, (char*)buf, sz);
  }

  void
  tcp_conn::connect()
  {
    socklen_t slen = sizeof(sockaddr_in);
    if(_addr.ss_family == AF_UNIX)
      slen = 115;
    else if(_addr.ss_family == AF_INET6)
      slen = sizeof(sockaddr_in6);
    int result = ::connect(fd.socket, (const sockaddr*)&_addr, slen);
    if(result == 0)
    {
      llarp::LogDebug("connected immedidately");
      connected();
    }
    else if(WSAGetLastError() == WSAEINPROGRESS)
    {
      // in progress
      llarp::LogDebug("connect in progress");
      WSASetLastError(0);
      return;
    }
    else if(_conn->error)
    {
      // wtf?
      char ebuf[1024];
      int err = WSAGetLastError();
      FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, err, LANG_NEUTRAL,
                    ebuf, 1024, nullptr);
      llarp::LogError("error connecting: ", ebuf);
      _conn->error(_conn);
    }
  }

  int
  tcp_serv::read(byte_t*, size_t)
  {
    int new_fd = ::accept(fd.socket, nullptr, nullptr);
    if(new_fd == -1)
    {
      char ebuf[1024];
      int err = WSAGetLastError();
      FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, err, LANG_NEUTRAL,
                    ebuf, 1024, nullptr);
      llarp::LogError("failed to accept on ", fd.socket, ":", ebuf);
      return -1;
    }
    // build handler
    llarp::tcp_conn* connimpl = new tcp_conn(loop, new_fd);
    if(loop->add_ev(connimpl, true))
    {
      // call callback
      if(tcp->accepted)
        tcp->accepted(tcp, &connimpl->tcp);
      return 0;
    }
    // cleanup error
    delete connimpl;
    return -1;
  }

  struct tun : public ev_io
  {
    llarp_tun_io* t;
    device* tunif;
    tun(llarp_tun_io* tio, llarp_ev_loop* l)
        : ev_io(INVALID_HANDLE_VALUE,
                new LossyWriteQueue_t("win32_tun_write_queue", l, l))
        , t(tio)
        , tunif(tuntap_init())
    {
      this->is_tun = true;
    };

    int
    sendto(const sockaddr* to, const void* data, size_t sz)
    {
      UNREFERENCED_PARAMETER(to);
      UNREFERENCED_PARAMETER(data);
      UNREFERENCED_PARAMETER(sz);
      return -1;
    }

    void
    flush_write()
    {
      if(t->before_write)
      {
        t->before_write(t);
        ev_io::flush_write();
      }
    }

    bool
    tick()
    {
      if(t->tick)
        t->tick(t);
      flush_write();
      return true;
    }

    int
    read(byte_t* buf, size_t sz)
    {
      ssize_t ret = tuntap_read(tunif, buf, sz);
      if(ret > 0 && t->recvpkt)
        // should have pktinfo
        // I have no idea...
        t->recvpkt(t, llarp::InitBuffer(buf, ret));
      return ret;
    }

    bool
    setup()
    {
      if(tuntap_start(tunif, TUNTAP_MODE_TUNNEL, 0) == -1)
      {
        llarp::LogWarn("failed to start interface");
        return false;
      }
      if(tuntap_set_ip(tunif, t->ifaddr, t->ifaddr, t->netmask) == -1)
      {
        llarp::LogWarn("failed to set ip");
        return false;
      }
      if(tuntap_up(tunif) == -1)
      {
        char ebuf[1024];
        int err = GetLastError();
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, err, LANG_NEUTRAL,
                      ebuf, 1024, nullptr);
        llarp::LogWarn("failed to put interface up: ", ebuf);
        return false;
      }

      fd.tun = tunif->tun_fd;
      if(fd.tun == INVALID_HANDLE_VALUE)
        return false;

      // we're already non-blocking
      return true;
    }

    ~tun()
    {
    }
  };

  struct udp_listener : public ev_io
  {
    llarp_udp_io* udp;

    udp_listener(int fd, llarp_udp_io* u) : ev_io(fd), udp(u){};

    ~udp_listener()
    {
    }

    bool
    tick()
    {
      if(udp->tick)
        udp->tick(udp);
      return true;
    }

    int
    read(byte_t* buf, size_t sz)
    {
      llarp_buffer_t b;
      b.base = buf;
      b.cur  = b.base;
      sockaddr_in6 src;
      socklen_t slen = sizeof(sockaddr_in6);
      sockaddr* addr = (sockaddr*)&src;
      ssize_t ret    = ::recvfrom(fd.socket, (char*)b.base, sz, 0, addr, &slen);
      if(ret < 0)
        return -1;
      if(static_cast< size_t >(ret) > sz)
        return -1;
      b.sz = ret;
      udp->recvfrom(udp, addr, b);
      return 0;
    }

    int
    sendto(const sockaddr* to, const void* data, size_t sz)
    {
      socklen_t slen;
      switch(to->sa_family)
      {
        case AF_INET:
          slen = sizeof(struct sockaddr_in);
          break;
        case AF_INET6:
          slen = sizeof(struct sockaddr_in6);
          break;
        default:
          return -1;
      }
      ssize_t sent = ::sendto(fd.socket, (char*)data, sz, 0, to, slen);
      if(sent == -1)
      {
        char ebuf[1024];
        int err = WSAGetLastError();
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, err, LANG_NEUTRAL,
                      ebuf, 1024, nullptr);
        llarp::LogWarn(ebuf);
      }
      return sent;
    }
  };

};  // namespace llarp

struct llarp_win32_loop : public llarp_ev_loop
{
  upoll_t* upollfd;
  HANDLE tun_event_queue;

  llarp_win32_loop() : upollfd(nullptr), tun_event_queue(INVALID_HANDLE_VALUE)
  {
  }

  bool
  tcp_connect(struct llarp_tcp_connecter* tcp, const sockaddr* remoteaddr)
  {
    // create socket
    int fd = usocket(remoteaddr->sa_family, SOCK_STREAM, 0);
    if(fd == -1)
      return false;
    llarp::tcp_conn* conn = new llarp::tcp_conn(this, fd, remoteaddr, tcp);
    add_ev(conn, true);
    conn->connect();
    return true;
  }

  llarp::ev_io*
  bind_tcp(llarp_tcp_acceptor* tcp, const sockaddr* bindaddr)
  {
    int fd = usocket(bindaddr->sa_family, SOCK_STREAM, 0);
    if(fd == -1)
      return nullptr;
    socklen_t sz = sizeof(sockaddr_in);
    if(bindaddr->sa_family == AF_INET6)
    {
      sz = sizeof(sockaddr_in6);
    }
    // keep. inexplicably, windows now has unix domain sockets
    // for now, use the ID numbers directly until this comes out of
    // beta
    else if(bindaddr->sa_family == AF_UNIX)
    {
      sz = 110;  // current size in 10.0.17763, verify each time the beta PSDK
                 // is updated
    }
    if(::bind(fd, bindaddr, sz) == -1)
    {
      uclose(fd);
      return nullptr;
    }
    if(ulisten(fd, 5) == -1)
    {
      uclose(fd);
      return nullptr;
    }
    return new llarp::tcp_serv(this, fd, tcp);
  }

  virtual bool
  udp_listen(llarp_udp_io* l, const sockaddr* src)
  {
    auto ev = create_udp(l, src);
    if(ev)
      l->fd = ev->fd.socket;
    return ev && add_ev(ev, false);
  }

  ~llarp_win32_loop()
  {
    if(upollfd)
      upoll_destroy(upollfd);
    if(tun_event_queue != INVALID_HANDLE_VALUE)
      CloseHandle(tun_event_queue);
  }

  bool
  running() const
  {
    return (upollfd != nullptr) && (tun_event_queue != INVALID_HANDLE_VALUE);
  }

  bool
  init()
  {
    if(!upollfd)
      upollfd = upoll_create(1);
    if(tun_event_queue == INVALID_HANDLE_VALUE)
      tun_event_queue =
          CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 1024);
    return upollfd && (tun_event_queue != INVALID_HANDLE_VALUE);
  }

  int
  tick(int ms)
  {
    upoll_event_t events[1024];
    int result;
    result = upoll_wait(upollfd, events, 1024, ms);
    if(result > 0)
    {
      int idx = 0;
      while(idx < result)
      {
        llarp::ev_io* ev = static_cast< llarp::ev_io* >(events[idx].data.ptr);
        if(ev)
        {
          if(events[idx].events & UPOLLERR)
          {
            ev->error();
          }
          else
          {
            if(events[idx].events & UPOLLIN)
            {
              ev->read(readbuf, sizeof(readbuf));
            }
            if(events[idx].events & UPOLLOUT)
            {
              ev->flush_write();
            }
          }
        }
        ++idx;
      }
    }

    DWORD size         = 0;
    OVERLAPPED* ovl    = nullptr;
    ULONG_PTR listener = 0;
    asio_evt_pkt* pkt;
    while(
        GetQueuedCompletionStatus(tun_event_queue, &size, &listener, &ovl, ms))
    {
      pkt              = (asio_evt_pkt*)ovl;
      llarp::ev_io* ev = reinterpret_cast< llarp::ev_io* >(listener);
      /*if(size != pkt->sz)
        llarp::LogWarn("incomplete async io operation: got ", size,
                       " bytes, expected ", pkt->sz, " bytes");*/
      if(!pkt->write)
        ev->read(readbuf, size);
      else
      {
        ev->flush_write_buffers(pkt->sz);
        printf("write tun\n");
      }
      ++result;
      delete pkt;
    }

    if(result != -1)
      tick_listeners();
    return result;
  }

  int
  run()
  {
    upoll_event_t events[1024];
    int result;
    do
    {
      result = upoll_wait(upollfd, events, 1024, EV_TICK_INTERVAL);
      if(result > 0)
      {
        int idx = 0;
        while(idx < result)
        {
          llarp::ev_io* ev = static_cast< llarp::ev_io* >(events[idx].data.ptr);
          if(ev)
          {
            if(events[idx].events & UPOLLERR)
            {
              ev->error();
            }
            else
            {
              if(events[idx].events & UPOLLIN)
              {
                ev->read(readbuf, sizeof(readbuf));
              }
              if(events[idx].events & UPOLLOUT)
              {
                ev->flush_write();
              }
            }
          }
          ++idx;
        }
      }
      if(result != -1)
        tick_listeners();
    } while(upollfd);
    return result;
  }

  int
  udp_bind(const sockaddr* addr)
  {
    socklen_t slen;
    switch(addr->sa_family)
    {
      case AF_INET:
        slen = sizeof(struct sockaddr_in);
        break;
      case AF_INET6:
        slen = sizeof(struct sockaddr_in6);
        break;
      default:
        return -1;
    }
    int fd = usocket(addr->sa_family, SOCK_DGRAM, 0);
    if(fd == -1)
    {
      perror("usocket()");
      return -1;
    }

    if(addr->sa_family == AF_INET6)
    {
      // enable dual stack explicitly
      int dual = 1;
      if(setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, (char*)&dual, sizeof(dual))
         == -1)
      {
        // failed
        perror("setsockopt()");
        close(fd);
        return -1;
      }
    }
    llarp::Addr a(*addr);
    llarp::LogDebug("bind to ", a);
    if(bind(fd, addr, slen) == -1)
    {
      perror("bind()");
      close(fd);
      return -1;
    }

    return fd;
  }

  bool
  close_ev(llarp::ev_io* ev)
  {
    if(ev->is_tun)
    {
      CancelIo(ev->fd.tun);
      CloseHandle(ev->fd.tun);
      return true;
    }
    return upoll_ctl(upollfd, UPOLL_CTL_DEL, ev->fd.socket, nullptr) != -1;
  }

  llarp::ev_io*
  create_tun(llarp_tun_io* tun)
  {
    llarp::tun* t = new llarp::tun(tun, this);
    if(t->setup())
    {
      return t;
    }
    delete t;
    return nullptr;
  }

  llarp::ev_io*
  create_udp(llarp_udp_io* l, const sockaddr* src)
  {
    int fd = udp_bind(src);
    if(fd == -1)
      return nullptr;
    llarp::ev_io* listener = new llarp::udp_listener(fd, l);
    l->impl                = listener;
    return listener;
  }

  bool
  add_ev(llarp::ev_io* e, bool write)
  {
    if(e->is_tun)
    {
      asio_evt_pkt* pkt = new asio_evt_pkt;
      pkt->write        = false;
      pkt->sz           = sizeof(readbuf);
      CreateIoCompletionPort(e->fd.tun, tun_event_queue, (ULONG_PTR)e, 1024);
      // queue an initial read
      ReadFile(e->fd.tun, readbuf, sizeof(readbuf), nullptr, &pkt->pkt);
      goto add;
    }
    upoll_event_t ev;
    ev.data.ptr = e;
    ev.events   = UPOLLIN | UPOLLERR;
    if(write)
      ev.events |= UPOLLOUT;
    if(upoll_ctl(upollfd, UPOLL_CTL_ADD, e->fd.socket, &ev) == -1)
    {
      delete e;
      return false;
    }
  add:
    handlers.emplace_back(e);
    return true;
  }

  bool
  udp_close(llarp_udp_io* l)
  {
    bool ret = false;
    llarp::udp_listener* listener =
        static_cast< llarp::udp_listener* >(l->impl);
    if(listener)
    {
      close_ev(listener);
      // remove handler
      auto itr = handlers.begin();
      while(itr != handlers.end())
      {
        if(itr->get() == listener)
          itr = handlers.erase(itr);
        else
          ++itr;
      }
      l->impl = nullptr;
      ret     = true;
    }
    return ret;
  }

  void
  stop()
  {
    if(upollfd)
      upoll_destroy(upollfd);
    upollfd = nullptr;
    if(tun_event_queue != INVALID_HANDLE_VALUE)
    {
      CloseHandle(tun_event_queue);
      tun_event_queue = INVALID_HANDLE_VALUE;
    }
  }
};

extern "C" asio_evt_pkt*
getTunEventPkt()
{
  return new asio_evt_pkt;
}

#endif
