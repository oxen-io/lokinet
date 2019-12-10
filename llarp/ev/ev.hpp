#ifndef LLARP_EV_HPP
#define LLARP_EV_HPP

#include <net/net_addr.hpp>
#include <ev/ev.h>
#include <util/buffer.hpp>
#include <util/codel.hpp>
#include <util/thread/threading.hpp>

// writev
#ifndef _WIN32
#include <sys/uio.h>
#include <unistd.h>
#endif

#include <algorithm>
#include <deque>
#include <list>
#include <future>
#include <utility>

#ifdef _WIN32
#include <win32/win32_up.h>
#include <win32/win32_upoll.h>
// From the preview SDK, should take a look at that
// periodically in case its definition changes
#define UNIX_PATH_MAX 108

typedef struct sockaddr_un
{
  ADDRESS_FAMILY sun_family;    /* AF_UNIX */
  char sun_path[UNIX_PATH_MAX]; /* pathname */
} SOCKADDR_UN, *PSOCKADDR_UN;
#else

#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) \
    || (__APPLE__ && __MACH__)
#include <sys/event.h>
#endif

#include <sys/un.h>
#endif

struct llarp_ev_pkt_pipe;

#ifndef MAX_WRITE_QUEUE_SIZE
#define MAX_WRITE_QUEUE_SIZE (1024UL)
#endif

#ifndef EV_READ_BUF_SZ
#define EV_READ_BUF_SZ (4 * 1024UL)
#endif
#ifndef EV_WRITE_BUF_SZ
#define EV_WRITE_BUF_SZ (2 * 1024UL)
#endif

/// do io and reset errno after
static ssize_t
IO(std::function< ssize_t(void) > iofunc)
{
  ssize_t ret = iofunc();
#ifndef _WIN32
  errno = 0;
#else
  WSASetLastError(0);
#endif
  return ret;
}

namespace llarp
{
#ifdef _WIN32
  struct win32_ev_io
  {
    struct WriteBuffer
    {
      llarp_time_t timestamp = 0;
      size_t bufsz;
      byte_t buf[EV_WRITE_BUF_SZ] = {0};

      WriteBuffer() = default;

      WriteBuffer(const byte_t* ptr, size_t sz)
      {
        if(sz <= sizeof(buf))
        {
          bufsz = sz;
          memcpy(buf, ptr, bufsz);
        }
        else
          bufsz = 0;
      }

      struct GetTime
      {
        llarp_time_t
        operator()(const WriteBuffer& buf) const
        {
          return buf.timestamp;
        }
      };

      struct GetNow
      {
        llarp_ev_loop_ptr loop;
        GetNow(llarp_ev_loop_ptr l) : loop(l)
        {
        }

        llarp_time_t
        operator()() const
        {
          return llarp_ev_loop_time_now_ms(loop);
        }
      };

      struct PutTime
      {
        llarp_ev_loop_ptr loop;
        PutTime(llarp_ev_loop_ptr l) : loop(l)
        {
        }
        void
        operator()(WriteBuffer& buf)
        {
          buf.timestamp = llarp_ev_loop_time_now_ms(loop);
        }
      };

      struct Compare
      {
        bool
        operator()(const WriteBuffer& left, const WriteBuffer& right) const
        {
          return left.timestamp < right.timestamp;
        }
      };
    };

    using LosslessWriteQueue_t = std::deque< WriteBuffer >;

    intptr_t
        fd;  // Sockets only, fuck UNIX-style reactive IO with a rusty knife

    int flags = 0;
    win32_ev_io(intptr_t f) : fd(f){};

    /// for tcp
    win32_ev_io(intptr_t f, LosslessWriteQueue_t* q)
        : fd(f), m_BlockingWriteQueue(q)
    {
    }

    virtual void
    error()
    {
      char ebuf[1024];
      int err = WSAGetLastError();
      FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, err, LANG_NEUTRAL,
                    ebuf, 1024, nullptr);
      llarp::LogError(ebuf);
    }

    virtual int
    read(byte_t* buf, size_t sz) = 0;

    virtual int
    sendto(const sockaddr* dst, const void* data, size_t sz)
    {
      UNREFERENCED_PARAMETER(dst);
      UNREFERENCED_PARAMETER(data);
      UNREFERENCED_PARAMETER(sz);
      return -1;
    };

    /// return false if we want to deregister and remove ourselves
    virtual bool
    tick()
    {
      return true;
    };

    /// used for tun interface and tcp conn
    virtual ssize_t
    do_write(void* data, size_t sz)
    {
      return uwrite(fd, (char*)data, sz);
    }

    bool
    queue_write(const byte_t* buf, size_t sz)
    {
      if(m_BlockingWriteQueue)
      {
        m_BlockingWriteQueue->emplace_back(buf, sz);
        return true;
      }
      else
        return false;
    }

    virtual void
    flush_write()
    {
      flush_write_buffers(0);
    }

    /// called in event loop when fd is ready for writing
    /// requeues anything not written
    /// this assumes fd is set to non blocking
    virtual void
    flush_write_buffers(size_t amount)
    {
      if(m_BlockingWriteQueue)
      {
        if(amount)
        {
          while(amount && m_BlockingWriteQueue->size())
          {
            auto& itr      = m_BlockingWriteQueue->front();
            ssize_t result = do_write(itr.buf, std::min(amount, itr.bufsz));
            if(result == -1)
              return;
            ssize_t dlt = itr.bufsz - result;
            if(dlt > 0)
            {
              // queue remaining to front of queue
              WriteBuffer buff(itr.buf + dlt, itr.bufsz - dlt);
              m_BlockingWriteQueue->pop_front();
              m_BlockingWriteQueue->push_front(buff);
              // TODO: errno?
              return;
            }
            m_BlockingWriteQueue->pop_front();
            amount -= result;
          }
        }
        else
        {
          // write buffers
          while(m_BlockingWriteQueue->size())
          {
            auto& itr      = m_BlockingWriteQueue->front();
            ssize_t result = do_write(itr.buf, itr.bufsz);
            if(result == -1)
              return;
            ssize_t dlt = itr.bufsz - result;
            if(dlt > 0)
            {
              // queue remaining to front of queue
              WriteBuffer buff(itr.buf + dlt, itr.bufsz - dlt);
              m_BlockingWriteQueue->pop_front();
              m_BlockingWriteQueue->push_front(buff);
              // TODO: errno?
              return;
            }
            m_BlockingWriteQueue->pop_front();
            int wsaerr = WSAGetLastError();
            if(wsaerr == WSA_IO_PENDING || wsaerr == WSAEWOULDBLOCK)
            {
              WSASetLastError(0);
              return;
            }
          }
        }
      }
      /// reset errno
      WSASetLastError(0);
    }

    std::unique_ptr< LosslessWriteQueue_t > m_BlockingWriteQueue;

    virtual ~win32_ev_io()
    {
      uclose(fd);
    };
  };
#else
  struct posix_ev_io
  {
    struct WriteBuffer
    {
      llarp_time_t timestamp = 0;
      size_t bufsz;
      byte_t buf[EV_WRITE_BUF_SZ];

      WriteBuffer() = default;

      WriteBuffer(const byte_t* ptr, size_t sz)
      {
        if(sz <= sizeof(buf))
        {
          bufsz = sz;
          memcpy(buf, ptr, bufsz);
        }
        else
          bufsz = 0;
      }

      struct GetTime
      {
        llarp_time_t
        operator()(const WriteBuffer& writebuf) const
        {
          return writebuf.timestamp;
        }
      };

      struct GetNow
      {
        llarp_ev_loop_ptr loop;
        GetNow(llarp_ev_loop_ptr l) : loop(std::move(l))
        {
        }

        llarp_time_t
        operator()() const
        {
          return llarp_ev_loop_time_now_ms(loop);
        }
      };

      struct PutTime
      {
        llarp_ev_loop_ptr loop;
        PutTime(llarp_ev_loop_ptr l) : loop(std::move(l))
        {
        }
        void
        operator()(WriteBuffer& writebuf)
        {
          writebuf.timestamp = llarp_ev_loop_time_now_ms(loop);
        }
      };

      struct Compare
      {
        bool
        operator()(const WriteBuffer& left, const WriteBuffer& right) const
        {
          return left.timestamp < right.timestamp;
        }
      };
    };

    using LossyWriteQueue_t =
        llarp::util::CoDelQueue< WriteBuffer, WriteBuffer::GetTime,
                                 WriteBuffer::PutTime, WriteBuffer::Compare,
                                 WriteBuffer::GetNow, llarp::util::NullMutex,
                                 llarp::util::NullLock, 5, 100, 1024 >;

    using LosslessWriteQueue_t = std::deque< WriteBuffer >;

    int fd;
    int flags = 0;
#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) \
    || (__APPLE__ && __MACH__)
    struct kevent change;
#endif

    posix_ev_io(int f) : fd(f)
    {
    }

    /// for tun
    posix_ev_io(int f, LossyWriteQueue_t* q) : fd(f), m_LossyWriteQueue(q)
    {
    }

    /// for tcp
    posix_ev_io(int f, LosslessWriteQueue_t* q) : fd(f), m_BlockingWriteQueue(q)
    {
    }

    virtual void
    error()
    {
      llarp::LogError(strerror(errno));
    }

    virtual int
    read(byte_t* buf, size_t sz) = 0;

    virtual int
    sendto(__attribute__((unused)) const sockaddr* dst,
           __attribute__((unused)) const void* data,
           __attribute__((unused)) size_t sz)
    {
      return -1;
    }

    /// return false if we want to deregister and remove ourselves
    virtual bool
    tick()
    {
      return true;
    }

    /// used for tun interface and tcp conn
    virtual ssize_t
    do_write(void* data, size_t sz)
    {
      return write(fd, data, sz);
    }

    bool
    queue_write(const byte_t* buf, size_t sz)
    {
      if(m_LossyWriteQueue)
      {
        m_LossyWriteQueue->Emplace(buf, sz);
        return true;
      }
      if(m_BlockingWriteQueue)
      {
        m_BlockingWriteQueue->emplace_back(buf, sz);
        return true;
      }

      return false;
    }

    virtual void
    flush_write()
    {
      flush_write_buffers(0);
    }

    virtual void
    before_flush_write()
    {
    }

    /// called in event loop when fd is ready for writing
    /// requeues anything not written
    /// this assumes fd is set to non blocking
    virtual void
    flush_write_buffers(size_t amount)
    {
      before_flush_write();
      if(m_LossyWriteQueue)
      {
        m_LossyWriteQueue->Process([&](WriteBuffer& buffer) {
          do_write(buffer.buf, buffer.bufsz);
          // if we would block we save the entries for later
          // discard entry
        });
      }
      else if(m_BlockingWriteQueue)
      {
        if(amount)
        {
          while(amount && m_BlockingWriteQueue->size())
          {
            auto& itr      = m_BlockingWriteQueue->front();
            ssize_t result = do_write(itr.buf, std::min(amount, itr.bufsz));
            if(result <= 0)
              return;
            ssize_t dlt = itr.bufsz - result;
            if(dlt > 0)
            {
              // queue remaining to front of queue
              WriteBuffer buff(itr.buf + dlt, itr.bufsz - dlt);
              m_BlockingWriteQueue->pop_front();
              m_BlockingWriteQueue->push_front(buff);
              // TODO: errno?
              return;
            }
            m_BlockingWriteQueue->pop_front();
            amount -= result;
          }
        }
        else
        {
          // write buffers
          while(m_BlockingWriteQueue->size())
          {
            auto& itr      = m_BlockingWriteQueue->front();
            ssize_t result = do_write(itr.buf, itr.bufsz);
            if(result <= 0)
            {
              errno = 0;
              return;
            }
            ssize_t dlt = itr.bufsz - result;
            if(dlt > 0)
            {
              // queue remaining to front of queue
              WriteBuffer buff(itr.buf + dlt, itr.bufsz - dlt);
              m_BlockingWriteQueue->pop_front();
              m_BlockingWriteQueue->push_front(buff);
              // TODO: errno?
              return;
            }
            m_BlockingWriteQueue->pop_front();
            if(errno == EAGAIN || errno == EWOULDBLOCK)
            {
              errno = 0;
              return;
            }
          }
        }
      }
      /// reset errno
      errno = 0;
    }

    std::unique_ptr< LossyWriteQueue_t > m_LossyWriteQueue;
    std::unique_ptr< LosslessWriteQueue_t > m_BlockingWriteQueue;

    virtual ~posix_ev_io()
    {
      close(fd);
    }
  };
#endif

// finally create aliases by platform
#ifdef _WIN32
  using ev_io = win32_ev_io;
#else
  using ev_io = posix_ev_io;
#endif

  // wew, managed to get away with using
  // 'int fd' across all platforms
  // since we're operating entirely
  // on sockets
  struct tcp_conn : public ev_io
  {
    sockaddr_storage _addr;
    bool _shouldClose     = false;
    bool _calledConnected = false;
    llarp_tcp_conn tcp;
    // null if inbound otherwise outbound
    llarp_tcp_connecter* _conn;

    static void
    DoClose(llarp_tcp_conn* conn)
    {
      static_cast< tcp_conn* >(conn->impl)->_shouldClose = true;
    }

    /// inbound
    tcp_conn(llarp_ev_loop* loop, int _fd)
        : ev_io(_fd, new LosslessWriteQueue_t{}), _conn(nullptr)
    {
      tcp.impl   = this;
      tcp.loop   = loop;
      tcp.closed = nullptr;
      tcp.user   = nullptr;
      tcp.read   = nullptr;
      tcp.tick   = nullptr;
      tcp.close  = &DoClose;
    }

    /// outbound
    tcp_conn(llarp_ev_loop* loop, int _fd, const sockaddr* addr,
             llarp_tcp_connecter* conn)
        : ev_io(_fd, new LosslessWriteQueue_t{}), _conn(conn)
    {
      socklen_t slen = sizeof(sockaddr_in);
      if(addr->sa_family == AF_INET6)
        slen = sizeof(sockaddr_in6);
      else if(addr->sa_family == AF_UNIX)
        slen = sizeof(sockaddr_un);
      memcpy(&_addr, addr, slen);
      tcp.impl   = this;
      tcp.loop   = loop;
      tcp.closed = nullptr;
      tcp.user   = nullptr;
      tcp.read   = nullptr;
      tcp.tick   = nullptr;
      tcp.close  = &DoClose;
    }

    ~tcp_conn() override = default;

    /// start connecting
    void
    connect();

    /// calls connected hooks
    void
    connected()
    {
      sockaddr_storage st;
      socklen_t sl;
      if(getpeername(fd, (sockaddr*)&st, &sl) == 0)
      {
        // we are connected yeh boi
        if(_conn)
        {
          if(_conn->connected && !_calledConnected)
            _conn->connected(_conn, &tcp);
        }
        _calledConnected = true;
      }
      else
      {
        error();
      }
    }

    void
    flush_write() override;

    void
    flush_write_buffers(size_t a) override
    {
      connected();
      ev_io::flush_write_buffers(a);
    }

    void
    error() override
    {
      _shouldClose = true;
      if(_conn)
      {
#ifndef _WIN32
        llarp::LogError("tcp_conn error: ", strerror(errno));
#else
        char ebuf[1024];
        int err = WSAGetLastError();
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, err, LANG_NEUTRAL,
                      ebuf, 1024, nullptr);
        llarp::LogError("tcp_conn error: ", ebuf);
#endif
        if(_conn->error)
          _conn->error(_conn);
      }
      errno = 0;
    }

    ssize_t
    do_write(void* buf, size_t sz) override;

    int
    read(byte_t* buf, size_t sz) override;

    bool
    tick() override;
  };

  struct tcp_serv : public ev_io
  {
    llarp_ev_loop* loop;
    llarp_tcp_acceptor* tcp;
    tcp_serv(llarp_ev_loop* l, int _fd, llarp_tcp_acceptor* t)
        : ev_io(_fd), loop(l), tcp(t)
    {
      tcp->impl = this;
    }

    bool
    tick() override
    {
      if(tcp->tick)
        tcp->tick(tcp);
      return true;
    }

    /// actually does accept() :^)
    int
    read(byte_t*, size_t) override;
  };

}  // namespace llarp

#ifdef _WIN32
struct llarp_fd_promise
{
  void
  Set(std::pair< int, int >)
  {
  }

  int
  Get()
  {
    return -1;
  }
};
#else
struct llarp_fd_promise
{
  using promise_val_t = std::pair< int, int >;
  llarp_fd_promise(std::promise< promise_val_t >* p) : _impl(p)
  {
  }
  std::promise< promise_val_t >* _impl;

  void
  Set(promise_val_t fds)
  {
    _impl->set_value(fds);
  }

  promise_val_t
  Get()
  {
    auto future = _impl->get_future();
    future.wait();
    return future.get();
  }
};
#endif

// this (nearly!) abstract base class
// is overriden for each platform
struct llarp_ev_loop
{
  byte_t readbuf[EV_READ_BUF_SZ] = {0};

  virtual bool
  init() = 0;

  virtual int
  run() = 0;

  virtual bool
  running() const = 0;

  virtual void
  update_time()
  {
  }

  virtual llarp_time_t
  time_now() const
  {
    return llarp::time_now_ms();
  }

  virtual void
  stopped(){};

  /// return false on socket error (non blocking)
  virtual bool
  tcp_connect(llarp_tcp_connecter* tcp, const sockaddr* addr) = 0;

  virtual int
  tick(int ms) = 0;

  virtual bool
  add_ticker(std::function< void(void) > ticker) = 0;

  virtual void
  stop() = 0;

  virtual bool
  udp_listen(llarp_udp_io* l, const sockaddr* src) = 0;

  virtual bool
  udp_close(llarp_udp_io* l) = 0;
  /// deregister event listener
  virtual bool
  close_ev(llarp::ev_io* ev) = 0;

  virtual bool
  tun_listen(llarp_tun_io* tun)
  {
    auto dev  = create_tun(tun);
    tun->impl = dev;
    if(dev)
    {
      return add_ev(dev, false);
    }
    return false;
  }

  virtual llarp::ev_io*
  create_tun(llarp_tun_io* tun) = 0;

  virtual llarp::ev_io*
  bind_tcp(llarp_tcp_acceptor* tcp, const sockaddr* addr) = 0;

  virtual bool
  add_pipe(llarp_ev_pkt_pipe*)
  {
    return false;
  }

  /// give this event loop a logic thread for calling
  virtual void set_logic(std::shared_ptr< llarp::Logic >) = 0;

  /// register event listener
  virtual bool
  add_ev(llarp::ev_io* ev, bool write) = 0;

  virtual bool
  tcp_listen(llarp_tcp_acceptor* tcp, const sockaddr* addr)
  {
    auto conn = bind_tcp(tcp, addr);
    return conn && add_ev(conn, true);
  }

  virtual ~llarp_ev_loop() = default;

  std::list< std::unique_ptr< llarp::ev_io > > handlers;

  virtual void
  tick_listeners()
  {
    auto itr = handlers.begin();
    while(itr != handlers.end())
    {
      if((*itr)->tick())
        ++itr;
      else
      {
        close_ev(itr->get());
        itr = handlers.erase(itr);
      }
    }
  }

  virtual void
  call_soon(std::function< void(void) > f) = 0;
};

struct PacketBuffer
{
  PacketBuffer(PacketBuffer&& other)
  {
    _ptr       = other._ptr;
    _sz        = other._sz;
    other._ptr = nullptr;
    other._sz  = 0;
  }

  PacketBuffer(const PacketBuffer&) = delete;

  PacketBuffer&
  operator=(const PacketBuffer&) = delete;

  PacketBuffer() : PacketBuffer(nullptr, 0){};
  explicit PacketBuffer(size_t sz) : _sz{sz}
  {
    _ptr = new char[sz];
  }
  PacketBuffer(char* buf, size_t sz)
  {
    _ptr = buf;
    _sz  = sz;
  }
  ~PacketBuffer()
  {
    if(_ptr)
      delete[] _ptr;
  }
  byte_t*
  data()
  {
    return (byte_t*)_ptr;
  }
  size_t
  size()
  {
    return _sz;
  }
  byte_t& operator[](size_t sz)
  {
    return data()[sz];
  }
  void
  reserve(size_t sz)
  {
    if(_ptr)
      delete[] _ptr;
    _ptr = new char[sz];
    _sz  = sz;
  }

 private:
  char* _ptr = nullptr;
  size_t _sz = 0;
};

struct PacketEvent
{
  llarp::Addr remote;
  PacketBuffer pkt;
};

struct llarp_pkt_list : public std::vector< PacketEvent >
{
};

#endif
