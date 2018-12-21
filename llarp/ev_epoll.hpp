#ifndef EV_EPOLL_HPP
#define EV_EPOLL_HPP
#include <fcntl.h>
#include <buffer.h>
#include <net.h>
#include <signal.h>
#include <sys/epoll.h>
#include <sys/un.h>
#include <tuntap.h>
#include <unistd.h>
#include <cstdio>
#include <buffer.hpp>
#include <ev.hpp>
#include <net.hpp>
#include <logger.hpp>
#include <mem.hpp>
#include <cassert>

#ifdef ANDROID
/** TODO: correct this value */
#define SOCK_NONBLOCK (0)
#endif

namespace llarp
{
  int
  tcp_conn::read(byte_t* buf, size_t sz)
  {
    if(_shouldClose)
      return -1;

    ssize_t amount = ::read(fd, buf, sz);

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
    // pretty much every UNIX system still extant, _including_ solaris
    // (on both sides of the fork) can ignore SIGPIPE....except
    // the other vendored systems... -rick
    return ::send(fd, buf, sz, MSG_NOSIGNAL);  // ignore sigpipe
  }

  void
  tcp_conn::connect()
  {
    socklen_t slen = sizeof(sockaddr_in);
    if(_addr.ss_family == AF_UNIX)
      slen = sizeof(sockaddr_un);
    else if(_addr.ss_family == AF_INET6)
      slen = sizeof(sockaddr_in6);
    int result = ::connect(fd, (const sockaddr*)&_addr, slen);
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
  tcp_serv::read(byte_t*, size_t)
  {
    int new_fd = ::accept(fd, nullptr, nullptr);
    if(new_fd == -1)
    {
      llarp::LogError("failed to accept on ", fd, ":", strerror(errno));
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
    read(byte_t* buf, size_t sz)
    {
      llarp_buffer_t b;
      b.base = buf;
      b.cur  = b.base;
      sockaddr_in6 src;
      socklen_t slen = sizeof(sockaddr_in6);
      sockaddr* addr = (sockaddr*)&src;
      ssize_t ret    = ::recvfrom(fd, b.base, sz, 0, addr, &slen);
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
      ssize_t sent = ::sendto(fd, data, sz, SOCK_NONBLOCK, to, slen);
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
        : ev_io(-1, new LossyWriteQueue_t("tun_write_queue", l, l))
        , t(tio)
        , tunif(tuntap_init())

              {

              };

    int
    sendto(__attribute__((unused)) const sockaddr* to,
           __attribute__((unused)) const void* data,
           __attribute__((unused)) size_t sz)
    {
      return -1;
    }

    bool
    tick()
    {
      if(t->tick)
        t->tick(t);
      flush_write();
      return true;
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

    int
    read(byte_t* buf, size_t sz)
    {
      ssize_t ret = tuntap_read(tunif, buf, sz);
      if(ret > 0 && t->recvpkt)
      {
        // does not have pktinfo
        t->recvpkt(t, llarp::InitBuffer(buf, ret));
      }
      return ret;
    }

    static int
    wait_for_fd_promise(struct device* dev)
    {
      llarp::tun* t = static_cast< llarp::tun* >(dev->user);
      if(t->t->get_fd_promise)
      {
        struct llarp_fd_promise* promise = t->t->get_fd_promise(t->t);
        if(promise)
          return llarp_fd_promise_wait_for_value(promise);
      }
      return -1;
    }

    bool
    setup()
    {
      // for android
      if(t->get_fd_promise)
      {
        tunif->obtain_fd = &wait_for_fd_promise;
        tunif->user      = this;
      }
      llarp::LogDebug("set ifname to ", t->ifname);
      strncpy(tunif->if_name, t->ifname, sizeof(tunif->if_name));
      if(tuntap_start(tunif, TUNTAP_MODE_TUNNEL, 0) == -1)
      {
        llarp::LogWarn("failed to start interface");
        return false;
      }
      if(t->get_fd_promise == nullptr)
      {
        if(tuntap_up(tunif) == -1)
        {
          llarp::LogWarn("failed to put interface up: ", strerror(errno));
          return false;
        }
        if(tuntap_set_ip(tunif, t->ifaddr, t->ifaddr, t->netmask) == -1)
        {
          llarp::LogWarn("failed to set ip");
          return false;
        }
      }
      fd = tunif->tun_fd;
      if(fd == -1)
        return false;
      // set non blocking
      int flags = fcntl(fd, F_GETFL, 0);
      if(flags == -1)
        return false;
      return fcntl(fd, F_SETFL, flags | O_NONBLOCK) != -1;
    }

    ~tun()
    {
      if(tunif)
        tuntap_destroy(tunif);
    }
  };
};  // namespace llarp

struct llarp_epoll_loop : public llarp_ev_loop
{
  int epollfd;
  llarp_epoll_loop() : epollfd(-1)
  {
  }

  bool
  tcp_connect(struct llarp_tcp_connecter* tcp, const sockaddr* remoteaddr)
  {
    // create socket
    int fd = ::socket(remoteaddr->sa_family, SOCK_STREAM, 0);
    if(fd == -1)
      return false;
    // set non blocking
    int flags = fcntl(fd, F_GETFL, 0);
    if(flags == -1)
    {
      ::close(fd);
      return false;
    }
    if(fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
    {
      ::close(fd);
      return false;
    }
    llarp::tcp_conn* conn = new llarp::tcp_conn(this, fd, remoteaddr, tcp);
    add_ev(conn, true);
    conn->connect();
    return true;
  }

  llarp::ev_io*
  bind_tcp(llarp_tcp_acceptor* tcp, const sockaddr* bindaddr)
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
    return new llarp::tcp_serv(this, fd, tcp);
  }

  virtual bool
  udp_listen(llarp_udp_io* l, const sockaddr* src)
  {
    auto ev = create_udp(l, src);
    if(ev)
      l->fd = ev->fd;
    return ev && add_ev(ev, false);
  }

  ~llarp_epoll_loop()
  {
    if(epollfd != -1)
      close(epollfd);
  }

  bool
  running() const
  {
    return epollfd != -1;
  }

  bool
  init()
  {
    if(epollfd == -1)
      epollfd = epoll_create(1);
    return false;
  }

  int
  tick(int ms)
  {
    epoll_event events[1024];
    int result;
    result = epoll_wait(epollfd, events, 1024, ms);
    if(result > 0)
    {
      int idx = 0;
      while(idx < result)
      {
        llarp::ev_io* ev = static_cast< llarp::ev_io* >(events[idx].data.ptr);
        if(ev)
        {
          llarp::LogDebug(idx, " of ", result,
                          " events=", std::to_string(events[idx].events));
          if(events[idx].events & EPOLLERR)
          {
            ev->error();
          }
          else
          {
            if(events[idx].events & EPOLLIN)
            {
              ev->read(readbuf, sizeof(readbuf));
            }
            if(events[idx].events & EPOLLOUT)
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
    epoll_event events[1024];
    int result;
    do
    {
      result = epoll_wait(epollfd, events, 1024, EV_TICK_INTERVAL);
      if(result > 0)
      {
        int idx = 0;
        while(idx < result)
        {
          llarp::ev_io* ev = static_cast< llarp::ev_io* >(events[idx].data.ptr);
          if(ev)
          {
            if(events[idx].events & EPOLLERR)
            {
              ev->error();
            }
            else
            {
              if(events[idx].events & EPOLLIN)
              {
                ev->read(readbuf, sizeof(readbuf));
              }
              if(events[idx].events & EPOLLOUT)
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
    } while(epollfd != -1);
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
    int fd = socket(addr->sa_family, SOCK_DGRAM, 0);
    if(fd == -1)
    {
      perror("socket()");
      return -1;
    }

    if(addr->sa_family == AF_INET6)
    {
      // enable dual stack explicitly
      int dual = 1;
      if(setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &dual, sizeof(dual)) == -1)
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
    return epoll_ctl(epollfd, EPOLL_CTL_DEL, ev->fd, nullptr) != -1;
  }

  llarp::ev_io*
  create_tun(llarp_tun_io* tun)
  {
    llarp::tun* t = new llarp::tun(tun, this);
    if(tun->get_fd_promise)
    {
    }
    else if(t->setup())
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
    epoll_event ev;
    ev.data.ptr = e;
    ev.events   = EPOLLIN | EPOLLERR;
    if(write)
      ev.events |= EPOLLOUT;
    if(epoll_ctl(epollfd, EPOLL_CTL_ADD, e->fd, &ev) == -1)
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
    // close all handlers before closing the epoll fd
    auto itr = handlers.begin();
    while(itr != handlers.end())
    {
      close_ev(itr->get());
      itr = handlers.erase(itr);
    }

    if(epollfd != -1)
      close(epollfd);
    epollfd = -1;
  }
};

#endif
