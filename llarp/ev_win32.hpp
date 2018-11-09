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

// TODO: convert all socket errno calls to WSAGetLastError(3),
// don't think winsock sets regular errno to this day
namespace llarp
{
  int
  tcp_conn::read(void* buf, size_t sz)
  {
    WSABUF r_buf = {(u_long)sz, (char*)buf};
    DWORD amount = 0;

    WSARecv(std::get< SOCKET >(fd), &r_buf, 1, nullptr, 0, &portfd[0], nullptr);
    GetOverlappedResult((HANDLE)std::get< SOCKET >(fd), &portfd[0], &amount,
                        TRUE);
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

  ssize_t
  tcp_conn::do_write(void* buf, size_t sz)
  {
    WSABUF s_buf = {(u_long)sz, (char*)buf};
    DWORD sent   = 0;

    if(_shouldClose)
      return -1;

    WSASend(std::get< SOCKET >(fd), &s_buf, 1, nullptr, 0, &portfd[1], nullptr);
    GetOverlappedResult((HANDLE)std::get< SOCKET >(fd), &portfd[1], &sent,
                        TRUE);
    return sent;
  }

  void
  tcp_conn::flush_write()
  {
    connected();
    ev_io::flush_write();
  }

  void
  tcp_conn::connect()
  {
    socklen_t slen = sizeof(sockaddr_in);
    if(_addr.ss_family == AF_UNIX)
      slen = 115;
    else if(_addr.ss_family == AF_INET6)
      slen = sizeof(sockaddr_in6);
    int result =
        ::connect(std::get< SOCKET >(fd), (const sockaddr*)&_addr, slen);
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
    SOCKET new_fd = ::accept(std::get< SOCKET >(fd), nullptr, nullptr);
    if(new_fd == INVALID_SOCKET)
    {
      llarp::LogError("failed to accept on ", std::get< SOCKET >(fd), ":",
                      strerror(errno));
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

    udp_listener(SOCKET fd, llarp_udp_io* u) : ev_io(fd), udp(u){};

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

    virtual int
    read(void* buf, size_t sz)
    {
      sockaddr_in6 src;
      socklen_t slen      = sizeof(src);
      sockaddr* addr      = (sockaddr*)&src;
      unsigned long flags = 0;
      WSABUF wbuf         = {(u_long)sz, static_cast< char* >(buf)};
      // WSARecvFrom
      llarp::LogDebug("read ", sz, " bytes into socket");
      int ret = ::WSARecvFrom(std::get< SOCKET >(fd), &wbuf, 1, nullptr, &flags,
                              addr, &slen, &portfd[0], nullptr);
      // 997 is the error code for queued ops
      int s_errno = ::WSAGetLastError();
      if(ret && s_errno != 997)
      {
        llarp::LogWarn("recv socket error ", s_errno);
        return -1;
      }
      udp->recvfrom(udp, addr, buf, sz);
      return 0;
    }

    virtual int
    sendto(const sockaddr* to, const void* data, size_t sz)
    {
      socklen_t slen;
      WSABUF wbuf = {(u_long)sz, (char*)data};
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
      // WSASendTo
      llarp::LogDebug("write ", sz, " bytes into socket");
      ssize_t sent = ::WSASendTo(std::get< SOCKET >(fd), &wbuf, 1, nullptr, 0,
                                 to, slen, &portfd[1], nullptr);
      int s_errno  = ::WSAGetLastError();
      if(sent && s_errno != 997)
      {
        llarp::LogWarn("send socket error ", s_errno);
        return -1;
      }
      return 0;
    }
  };

  struct tun : public ev_io
  {
    llarp_tun_io* t;
    device* tunif;
    OVERLAPPED* tun_async[2];
    tun(llarp_tun_io* tio, llarp_ev_loop* l)
        : ev_io(INVALID_HANDLE_VALUE,
                new LossyWriteQueue_t("win32_tun_write", l))
        , t(tio)
        , tunif(tuntap_init()){};

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
      return WriteFile(std::get< HANDLE >(fd), data, sz, nullptr, tun_async[1]);
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
      llarp::LogDebug("set ifname to ", t->ifname);
      strncpy(tunif->if_name, t->ifname, IFNAMSIZ);

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

      fd           = tunif->tun_fd;
      tun_async[0] = &tunif->ovl[0];
      tun_async[1] = &tunif->ovl[1];
      if(std::get< HANDLE >(fd) == INVALID_HANDLE_VALUE)
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
  HANDLE iocpfd;

  llarp_win32_loop() : iocpfd(INVALID_HANDLE_VALUE)
  {
  }

  bool
  tcp_connect(struct llarp_tcp_connecter* tcp, const sockaddr* remoteaddr)
  {
    // create socket
    DWORD on  = 1;
    SOCKET fd = ::socket(remoteaddr->sa_family, SOCK_STREAM, 0);
    if(fd == INVALID_SOCKET)
      return false;
    // set non blocking
    if(ioctlsocket(fd, FIONBIO, &on) == SOCKET_ERROR)
    {
      ::closesocket(fd);
      return false;
    }
    llarp::tcp_conn* conn = new llarp::tcp_conn(this, fd, remoteaddr, tcp);
    add_ev(conn, true);
    conn->connect();
    return true;
  }

  ~llarp_win32_loop()
  {
    if(iocpfd != INVALID_HANDLE_VALUE)
      ::CloseHandle(iocpfd);
    iocpfd = INVALID_HANDLE_VALUE;
  }

  llarp::ev_io*
  bind_tcp(llarp_tcp_acceptor* tcp, const sockaddr* bindaddr)
  {
    DWORD on  = 1;
    SOCKET fd = ::socket(bindaddr->sa_family, SOCK_STREAM, 0);
    if(fd == INVALID_SOCKET)
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
    if(::bind(fd, bindaddr, sz) == SOCKET_ERROR)
    {
      ::closesocket(fd);
      return nullptr;
    }
    if(::listen(fd, 5) == SOCKET_ERROR)
    {
      ::closesocket(fd);
      return nullptr;
    }
    llarp::ev_io* serv = new llarp::tcp_serv(this, fd, tcp);
    tcp->impl          = serv;

    ioctlsocket(fd, FIONBIO, &on);
    return serv;
  }

  bool
  init()
  {
    if(iocpfd == INVALID_HANDLE_VALUE)
      iocpfd = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);

    if(iocpfd == INVALID_HANDLE_VALUE)
      return false;

    return true;
  }

  // it works! -despair86, 3-Aug-18 @0420
  int
  tick(int ms)
  {
    OVERLAPPED_ENTRY events[1024];
    memset(&events, 0, sizeof(OVERLAPPED_ENTRY) * 1024);
    ULONG numEvents = 0;
    if(::GetQueuedCompletionStatusEx(iocpfd, events, 1024, &numEvents, ms,
                                     false))
    {
      for(ULONG idx = 0; idx < numEvents; ++idx)
      {
        llarp::ev_io* ev =
            reinterpret_cast< llarp::ev_io* >(events[idx].lpCompletionKey);
        if(ev)
        {
          if(ev->write)
            ev->flush_write();
          auto amount =
              std::min(EV_READ_BUF_SZ, events[idx].dwNumberOfBytesTransferred);
          if(events[idx].lpOverlapped)
          {
            memcpy(readbuf, events[idx].lpOverlapped->Pointer, amount);
            ev->read(readbuf, amount);
          }
        }
      }
    }
    tick_listeners();
    if(numEvents)
      return numEvents;
    else
      return -1;
  }

  // ok apparently this isn't being used yet...
  int
  run()
  {
    // The only field we really care about is
    // the listener_id, as it contains the address
    // of the udp_listener instance.
    DWORD iolen = 0;
    // ULONG_PTR is guaranteed to be the same size
    // as an arch-specific pointer value
    ULONG_PTR ev_id      = 0;
    WSAOVERLAPPED* qdata = nullptr;
    int idx              = 0;
    BOOL result =
        ::GetQueuedCompletionStatus(iocpfd, &iolen, &ev_id, &qdata, 10);

    if(result && qdata)
    {
      llarp::udp_listener* ev = reinterpret_cast< llarp::udp_listener* >(ev_id);
      if(ev)
      {
        llarp::LogDebug("size: ", iolen, "\tev_id: ", ev_id,
                        "\tqdata: ", qdata);
        if(iolen <= sizeof(readbuf))
          ev->read(readbuf, iolen);
      }
      ++idx;
    }

    if(!idx)
      return -1;
    else
    {
      result = idx;
      tick_listeners();
    }

    return result;
  }

  SOCKET
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
        return INVALID_SOCKET;
    }
    DWORD on  = 1;
    SOCKET fd = ::socket(addr->sa_family, SOCK_DGRAM, 0);
    if(fd == INVALID_SOCKET)
    {
      perror("WSASocket()");
      return INVALID_SOCKET;
    }

    if(addr->sa_family == AF_INET6)
    {
      // enable dual stack explicitly
      int dual = 1;
      if(setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, (const char*)&dual,
                    sizeof(dual))
         == -1)
      {
        // failed
        perror("setsockopt()");
        closesocket(fd);
        return INVALID_SOCKET;
      }
    }
    llarp::Addr a(*addr);
    llarp::LogDebug("bind to ", a);
    if(bind(fd, addr, slen) == -1)
    {
      perror("bind()");
      closesocket(fd);
      return INVALID_SOCKET;
    }
    llarp::LogDebug("socket fd is ", fd);
    ioctlsocket(fd, FIONBIO, &on);
    return fd;
  }

  bool
  close_ev(llarp::ev_io* ev)
  {
    // On Windows, just close the descriptor to decrease the iocp refcount
    // and stop any pending I/O
    BOOL stopped;
    int close_fd;

    if(std::holds_alternative< SOCKET >(ev->fd))
    {
      stopped =
          ::CancelIo(reinterpret_cast< HANDLE >(std::get< SOCKET >(ev->fd)));
      close_fd = closesocket(std::get< SOCKET >(ev->fd));
    }
    else
    {
      stopped  = ::CancelIo(std::get< HANDLE >(ev->fd));
      close_fd = CloseHandle(std::get< HANDLE >(ev->fd));
      if(close_fd)
        close_fd = 0;  // must be zero
      else
        close_fd = 1;
    }
    return close_fd == 0 && stopped == TRUE;
  }

  llarp::ev_io*
  create_udp(llarp_udp_io* l, const sockaddr* src)
  {
    SOCKET fd = udp_bind(src);
    llarp::LogDebug("new socket fd is ", fd);
    if(fd == INVALID_SOCKET)
      return nullptr;
    llarp::udp_listener* listener = new llarp::udp_listener(fd, l);
    l->impl                       = listener;
    return listener;
  }

  llarp::ev_io*
  create_tun(llarp_tun_io* tun)
  {
    llarp::tun* t = new llarp::tun(tun, this);
    if(t->setup())
      return t;
    delete t;
    return nullptr;
  }

  bool
  add_ev(llarp::ev_io* ev, bool write)
  {
    ev->listener_id = reinterpret_cast< ULONG_PTR >(ev);

    // if the write flag was set earlier,
    // clear it on demand
    if(ev->write && !write)
      ev->write = false;

    if(write)
      ev->write = true;

    // now write a blank packet containing nothing but the address of
    // the event listener
    if(ev->isTCP)
    {
      if(!::CreateIoCompletionPort((HANDLE)std::get< SOCKET >(ev->fd), iocpfd,
                                   ev->listener_id, 0))
      {
        delete ev;
        return false;
      }
      else
        goto start_loop;
    }

    if(std::holds_alternative< SOCKET >(ev->fd))
    {
      if(!::CreateIoCompletionPort((HANDLE)std::get< SOCKET >(ev->fd), iocpfd,
                                   ev->listener_id, 0))
      {
        delete ev;
        return false;
      }
    }
    else
    {
      if(!::CreateIoCompletionPort(std::get< HANDLE >(ev->fd), iocpfd,
                                   ev->listener_id, 0))
      {
        delete ev;
        return false;
      }
    }

  start_loop:
    PostQueuedCompletionStatus(iocpfd, 0, ev->listener_id, nullptr);
    handlers.emplace_back(ev);
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

  bool
  running() const
  {
    return iocpfd != INVALID_HANDLE_VALUE;
  }

  bool
  udp_listen(llarp_udp_io* l, const sockaddr* src)
  {
    auto ev = create_udp(l, src);
    if(ev)
      l->fd = std::get< SOCKET >(ev->fd);
    return ev && add_ev(ev, false);
  }

  void
  stop()
  {
    // Are we leaking any file descriptors?
    // This was part of the reason I had this
    // in the destructor.
    /*if(iocpfd != INVALID_HANDLE_VALUE)
      ::CloseHandle(iocpfd);
    iocpfd = INVALID_HANDLE_VALUE;*/
  }
};

#endif