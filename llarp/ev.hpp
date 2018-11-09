#ifndef LLARP_EV_HPP
#define LLARP_EV_HPP
#include <llarp/ev.h>
// writev
#ifndef _WIN32
#include <sys/uio.h>
#endif
#include <unistd.h>
#include <llarp/buffer.h>
#include <llarp/codel.hpp>
#include <list>
#include <deque>
#include <algorithm>

#ifdef _WIN32
#include <variant>
#endif

#ifndef MAX_WRITE_QUEUE_SIZE
#define MAX_WRITE_QUEUE_SIZE (1024UL)
#endif

#ifndef EV_READ_BUF_SZ
#define EV_READ_BUF_SZ (4 * 1024UL)
#endif
#ifndef EV_WRITE_BUF_SZ
#define EV_WRITE_BUF_SZ (2 * 1024UL)
#endif

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

      struct PutTime
      {
        llarp_ev_loop* loop;
        PutTime(llarp_ev_loop* l) : loop(l)
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

    typedef llarp::util::CoDelQueue< WriteBuffer, WriteBuffer::GetTime,
                                     WriteBuffer::PutTime, WriteBuffer::Compare,
                                     llarp::util::NullMutex,
                                     llarp::util::NullLock, 5, 100, 128 >
        LossyWriteQueue_t;

    typedef std::deque< WriteBuffer > LosslessWriteQueue_t;

    // on windows, tcp/udp event loops are socket fds
    // and TUN device is a plain old fd
    std::variant< SOCKET, HANDLE > fd;
    ULONG_PTR listener_id = 0;
    bool isTCP            = false;
    bool write            = false;
    WSAOVERLAPPED portfd[2];

    // constructors
    // for udp
    win32_ev_io(SOCKET f) : fd(f)
    {
      memset((void*)&portfd[0], 0, sizeof(WSAOVERLAPPED) * 2);
    };
    // for tun
    win32_ev_io(HANDLE t, LossyWriteQueue_t* q) : fd(t), m_LossyWriteQueue(q)
    {
      memset((void*)&portfd[0], 0, sizeof(WSAOVERLAPPED) * 2);
    }
    // for tcp
    win32_ev_io(SOCKET f, LosslessWriteQueue_t* q)
        : fd(f), m_BlockingWriteQueue(q)
    {
      memset((void*)&portfd[0], 0, sizeof(WSAOVERLAPPED) * 2);
      isTCP = true;
    }

    virtual int
    read(void* buf, size_t sz) = 0;

    virtual int
    sendto(const sockaddr* dst, const void* data, size_t sz)
    {
      (void)(dst);
      (void)(data);
      (void)(sz);
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
      //DWORD w;
      if(std::holds_alternative< HANDLE >(fd))
        WriteFile(std::get< HANDLE >(fd), data, sz, nullptr, &portfd[1]);
      else
        WriteFile((HANDLE)std::get< SOCKET >(fd), data, sz, nullptr,
                  &portfd[1]);
      return sz;
    }

    bool
    queue_write(const byte_t* buf, size_t sz)
    {
      if(m_LossyWriteQueue)
      {
        m_LossyWriteQueue->Emplace(buf, sz);
        return true;
      }
      else if(m_BlockingWriteQueue)
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
      if(m_LossyWriteQueue)
        m_LossyWriteQueue->Process([&](WriteBuffer& buffer) {
          do_write(buffer.buf, buffer.bufsz);
          // if we would block we save the entries for later
          // discard entry
        });
      else if(m_BlockingWriteQueue)
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

    virtual ~win32_ev_io()
    {
      closesocket(std::get< SOCKET >(fd));
    };
  };
#endif

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
        operator()(const WriteBuffer& buf) const
        {
          return buf.timestamp;
        }
      };

      struct PutTime
      {
        llarp_ev_loop* loop;
        PutTime(llarp_ev_loop* l) : loop(l)
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

    typedef llarp::util::CoDelQueue< WriteBuffer, WriteBuffer::GetTime,
                                     WriteBuffer::PutTime, WriteBuffer::Compare,
                                     llarp::util::NullMutex,
                                     llarp::util::NullLock, 5, 100, 128 >
        LossyWriteQueue_t;

    typedef std::deque< WriteBuffer > LosslessWriteQueue_t;

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
    read(void* buf, size_t sz) = 0;

    virtual int
    sendto(__attribute__((unused)) const sockaddr* dst,
           __attribute__((unused)) const void* data,
           __attribute__((unused)) size_t sz)
    {
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
      else if(m_BlockingWriteQueue)
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
      if(m_LossyWriteQueue)
        m_LossyWriteQueue->Process([&](WriteBuffer& buffer) {
          do_write(buffer.buf, buffer.bufsz);
          // if we would block we save the entries for later
          // discard entry
        });
      else if(m_BlockingWriteQueue)
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
    };
  };

// finally create aliases by platform
#ifdef _WIN32
  using ev_io = win32_ev_io;
#define sizeof(sockaddr_un) 115
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

    /// inbound
    tcp_conn(llarp_ev_loop* loop, int fd)
        : ev_io(fd, new LosslessWriteQueue_t{}), _conn(nullptr)
    {
      tcp.impl   = this;
      tcp.loop   = loop;
      tcp.closed = nullptr;
      tcp.user   = nullptr;
      tcp.read   = nullptr;
      tcp.tick   = nullptr;
    }

    /// outbound
    tcp_conn(llarp_ev_loop* loop, int fd, const sockaddr* addr,
             llarp_tcp_connecter* conn)
        : ev_io(fd, new LosslessWriteQueue_t{}), _conn(conn)
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
    }

    virtual ~tcp_conn()
    {
    }

    /// start connecting
    void
    connect();

    /// calls connected hooks
    void
    connected()
    {
      // we are connected yeh boi
      if(_conn)
      {
        if(_conn->connected && !_calledConnected)
          _conn->connected(_conn, &tcp);
      }
      _calledConnected = true;
    }

    void
    flush_write();

    void
    flush_write_buffers(size_t a)
    {
      connected();
      ev_io::flush_write_buffers(a);
    }

    void
    error()
    {
      if(_conn)
      {
        llarp::LogError("tcp_conn error: ", strerror(errno));
        if(_conn->error)
          _conn->error(_conn);
      }
    }

    virtual ssize_t
    do_write(void* buf, size_t sz);

    virtual int
    read(void* buf, size_t sz);

    bool
    tick();
  };

  struct tcp_serv : public ev_io
  {
    llarp_ev_loop* loop;
    llarp_tcp_acceptor* tcp;
    tcp_serv(llarp_ev_loop* l, int fd, llarp_tcp_acceptor* t)
        : ev_io(fd), loop(l), tcp(t)
    {
      tcp->impl = this;
    }

    bool
    tick()
    {
      if(tcp->tick)
        tcp->tick(tcp);
      return true;
    }

    /// actually does accept() :^)
    virtual int
    read(void*, size_t);
  };

};  // namespace llarp

// this (nearly!) abstract base class
// is overriden for each platform
struct llarp_ev_loop
{
  byte_t readbuf[EV_READ_BUF_SZ] = {0};
  llarp_time_t _now              = 0;
  virtual bool
  init() = 0;
  virtual int
  run() = 0;

  virtual int
  tick(int ms) = 0;

  virtual void
  stop() = 0;

  virtual bool
  udp_listen(llarp_udp_io* l, const sockaddr* src) = 0;

  virtual llarp::ev_io*
  create_udp(llarp_udp_io* l, const sockaddr* src) = 0;

  virtual bool
  udp_close(llarp_udp_io* l) = 0;
  /// deregister event listener
  virtual bool
  close_ev(llarp::ev_io* ev) = 0;

  virtual llarp::ev_io*
  create_tun(llarp_tun_io* tun) = 0;

  virtual llarp::ev_io*
  bind_tcp(llarp_tcp_acceptor* tcp, const sockaddr* addr) = 0;

  /// return false on socket error (non blocking)
  virtual bool
  tcp_connect(llarp_tcp_connecter* tcp, const sockaddr* addr) = 0;

  /// register event listener
  virtual bool
  add_ev(llarp::ev_io* ev, bool write) = 0;

  virtual bool
  running() const = 0;

  virtual ~llarp_ev_loop(){};

  std::list< std::unique_ptr< llarp::ev_io > > handlers;

  void
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
};

#endif
