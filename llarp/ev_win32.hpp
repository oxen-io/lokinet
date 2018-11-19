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
  tcp_conn::read(void* buf, size_t sz)
  {
    if(_shouldClose)
      return -1;

    ssize_t amount = uread(fd.socket, (char*)buf, sz);

    if(amount > 0)
    {
      if(tcp.read)
        tcp.read(&tcp, buf, amount);
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
    else if(errno == EINPROGRESS)
    {
      // in progress
      llarp::LogDebug("connect in progress");
      errno = 0;
      return;
    }
    else if(_conn->error)
    {
      // wtf?
      llarp::LogError("error connecting ", strerror(errno));
      _conn->error(_conn);
    }
  }

  int
  tcp_serv::read(void*, size_t)
  {
    int new_fd = ::accept(fd.socket, nullptr, nullptr);
    if(new_fd == -1)
    {
      llarp::LogError("failed to accept on ", fd.socket, ":", strerror(errno));
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
    read(void* buf, size_t sz)
    {
      if(this->is_tun)
      {
        llarp::tun* t = (llarp::tun*)this;
        ssize_t ret   = tuntap_read(t->tunif, buf, sz);
        goto next;
      }
      sockaddr_in6 src;
      socklen_t slen = sizeof(sockaddr_in6);
      sockaddr* addr = (sockaddr*)&src;
      ssize_t ret    = ::recvfrom(fd.socket, (char*)buf, sz, 0, addr, &slen);
    next:
      if(ret < 0)
        return -1;
      if(static_cast< size_t >(ret) > sz)
        return -1;
      udp->recvfrom(udp, addr, buf, ret);
      return 0;
    }

    int
    sendto(const sockaddr* to, const void* data, size_t sz)
    {
      if(this->is_tun)
      {
      }
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
        llarp::LogWarn(strerror(errno));
      }
      return sent;
    }
  };

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
      (void)(to);
      (void)(data);
      (void)(sz);
      return -1;
    }

    void
    flush_write()
    {
      if(t->before_write)
      {
        t->before_write(t);
      }
      ev_io::flush_write();
    }

    bool
    tick()
    {
      if(t->tick)
        t->tick(t);
      flush_write();
      return true;
    }

    ssize_t
    do_write(void* data, size_t sz)
    {
      return tuntap_write(tunif, data, sz);
    }

    int
    read(void* buf, size_t sz)
    {
      ssize_t ret = tuntap_read(tunif, buf, sz);
      if(ret > 0 && t->recvpkt)
        // should have pktinfo
        // I have no idea...
        t->recvpkt(t, (byte_t*)buf, ret);
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
        llarp::LogWarn("failed to put interface up: ", strerror(errno));
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

};  // namespace llarp

struct llarp_win32_loop : public llarp_ev_loop
{
  upoll_t* upollfd;
  llarp_win32_loop() : upollfd(nullptr)
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
  }

  bool
  running() const
  {
    return upollfd != nullptr;
  }

  bool
  init()
  {
    if(!upollfd)
      upollfd = upoll_create(1);
    return false;
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
	// if tun, add to vector without adding to
	// the epollfd - epollfds on windows only take
	// real sockets
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
  }
};

#endif