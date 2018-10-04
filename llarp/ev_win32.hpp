#ifndef EV_WIN32_HPP
#define EV_WIN32_HPP
#include <llarp/buffer.h>
#include <llarp/net.h>
#include <windows.h>
#include <cstdio>
#include <llarp/net.hpp>
#include "ev.hpp"
#include "logger.hpp"

namespace llarp
{
  struct udp_listener : public ev_io
  {
    llarp_udp_io* udp;

    // we receive queued data in the OVERLAPPED data field,
    // much like the pipefds in the UNIX kqueue and loonix
    // epoll handles
    // 0 is the read port, 1 is the write port
    WSAOVERLAPPED portfds[2] = {0};
    size_t iosz;

    udp_listener(SOCKET fd, llarp_udp_io* u) : ev_io(fd), udp(u){};

    ~udp_listener()
    {
    }

    int
    getData(void* buf, size_t sz, size_t ret)
    {
      iosz = ret;
      return read(buf, sz);
    }

    virtual int
    read(void* buf, size_t sz)
    {
      sockaddr_in6 src;
      socklen_t slen      = sizeof(src);
      sockaddr* addr      = (sockaddr*)&src;
      unsigned long flags = 0;
      WSABUF wbuf         = {sz, static_cast< char* >(buf)};
      // WSARecvFrom
      int ret = ::WSARecvFrom(std::get< SOCKET >(fd), &wbuf, 1, nullptr, &flags,
                              addr, &slen, &portfds[0], nullptr);
      // 997 is the error code for queued ops
      int s_errno = ::WSAGetLastError();
      if(ret && s_errno != 997)
      {
        llarp::LogWarn("recv socket error ", s_errno);
        return -1;
      }
      // get the _real_ payload size from tick()
      udp->recvfrom(udp, addr, buf, iosz);
      return 0;
    }

    virtual int
    sendto(const sockaddr* to, const void* data, size_t sz)
    {
      socklen_t slen;
      WSABUF wbuf = {sz, (char*)data};
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
      ssize_t sent = ::WSASendTo(std::get< SOCKET >(fd), &wbuf, 1, nullptr, 0,
                                 to, slen, &portfds[1], nullptr);
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
    tun(llarp_tun_io* tio)
        : ev_io(INVALID_HANDLE_VALUE)
        , t(tio)
        , tunif(tuntap_init())

              {

              };

    int
    sendto(const sockaddr* to, const void* data, size_t sz)
    {
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

    int
    read(void* buf, size_t sz)
    {
      ssize_t ret = tuntap_read(tunif, buf, sz);
      if(ret > 4 && t->recvpkt)
        // should have pktinfo
        t->recvpkt(t, ((byte_t*)buf) + 4, ret - 4);
      return ret;
    }

    bool
    setup()
    {
      llarp::LogDebug("set ifname to ", t->ifname);
      strncpy(tunif->if_name, t->ifname, sizeof(tunif->if_name));

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

      fd = tunif->tun_fd;
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

  ~llarp_win32_loop()
  {
    if(iocpfd != INVALID_HANDLE_VALUE)
      ::CloseHandle(iocpfd);
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
    // The only field we really care about is
    // the listener_id, as it contains the address
    // of the udp_listener instance.
    DWORD iolen = 0;
    // ULONG_PTR is guaranteed to be the same size
    // as an arch-specific pointer value
    ULONG_PTR ev_id      = 0;
    WSAOVERLAPPED* qdata = nullptr;
    int result           = 0;
    int idx              = 0;

    do
    {
      if(ev_id && qdata && iolen)
      {
        llarp::udp_listener* ev =
            reinterpret_cast< llarp::udp_listener* >(ev_id);
        if(ev && !ev->fd.valueless_by_exception())
        {
          ev->getData(readbuf, sizeof(readbuf), iolen);
        }
      }
      ++idx;
    } while(::GetQueuedCompletionStatus(iocpfd, &iolen, &ev_id, &qdata, ms));

    if(!idx)
      return -1;
    else
    {
      tick_listeners();
      result = idx;
    }

    return result;
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
    int result           = 0;
    int idx              = 0;

    // unlike epoll and kqueue, we only need to run so long as the
    // system call returns TRUE
    do
    {
      if(ev_id && qdata && iolen)
      {
        llarp::udp_listener* ev =
            reinterpret_cast< llarp::udp_listener* >(ev_id);
        if(ev && !ev->fd.valueless_by_exception())
        {
          ev->getData(readbuf, sizeof(readbuf), iolen);
        }
      }
      ++idx;
    } while(::GetQueuedCompletionStatus(iocpfd, &iolen, &ev_id, &qdata, 10));

    if(!idx)
      return -1;
    else
    {
      tick_listeners();
      result = idx;
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
    SOCKET fd = ::WSASocket(addr->sa_family, SOCK_DGRAM, 0, nullptr, 0,
                            WSA_FLAG_OVERLAPPED);
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
    llarp::LogInfo("socket fd is ", fd);
    return fd;
  }

  bool
  close_ev(llarp::ev_io* ev)
  {
    // On Windows, just close the socket to decrease the iocp refcount
    // and stop any pending I/O
    BOOL stopped =
        ::CancelIo(reinterpret_cast< HANDLE >(std::get< SOCKET >(ev->fd)));
    return closesocket(std::get< SOCKET >(ev->fd)) == 0 && stopped == TRUE;
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
    udp_listeners.push_back(l);
    return listener;
  }

  llarp::ev_io*
  create_tun(llarp_tun_io* tun)
  {
    llarp::tun* t = new llarp::tun(tun);
    if(t->setup())
      return t;
    delete t;
    return nullptr;
  }

  bool
  add_ev(llarp::ev_io* ev, bool write)
  {
    ev->listener_id = reinterpret_cast< ULONG_PTR >(ev);
    switch(ev->fd.index())
    {
      case 0:
        if(!::CreateIoCompletionPort((HANDLE)std::get< 0 >(ev->fd), iocpfd,
                                     ev->listener_id, 0))
        {
          delete ev;
          return false;
        }
        break;
      case 1:
        if(!::CreateIoCompletionPort(std::get< 1 >(ev->fd), iocpfd,
                                     ev->listener_id, 0))
        {
          delete ev;
          return false;
        }
        break;
      default:
        return false;
    }
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
      ret     = close_ev(listener);
      l->impl = nullptr;
      delete listener;
      udp_listeners.remove(l);
    }
    return ret;
  }

  bool
  running() const
  {
    return iocpfd != INVALID_HANDLE_VALUE;
  }

  void
  stop()
  {
    // still does nothing
  }
};

#endif
