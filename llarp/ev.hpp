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

#ifdef _WIN32
#include <variant>
#endif

#ifndef MAX_WRITE_QUEUE_SIZE
#define MAX_WRITE_QUEUE_SIZE 1024
#endif

#ifndef EV_READ_BUF_SZ
#define EV_READ_BUF_SZ (4 * 1024)
#endif
#ifndef EV_WRITE_BUF_SZ
#define EV_WRITE_BUF_SZ (2 * 1024)
#endif

namespace llarp
{
  struct ev_io
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
        llarp_time_t operator()(const WriteBuffer & buf) const
        {
          return buf.timestamp;
        }
      };

      struct PutTime
      {
        llarp_ev_loop * loop;
        PutTime(llarp_ev_loop * l ) : loop(l) {}
        void operator()(WriteBuffer & buf)
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

#ifndef _WIN32
    int fd;
    int flags = 0;
    ev_io(int f) : fd(f)
    {
    }

    /// for tun
    ev_io(int f, LossyWriteQueue_t* q) : fd(f), m_LossyWriteQueue(q)
    {
    }

    /// for tcp
    ev_io(int f, LosslessWriteQueue_t* q) : fd(f), m_BlockingWriteQueue(q)
    {
    }
#else
    // on windows, tcp/udp event loops are socket fds
    // and TUN device is a plain old fd
    std::variant< SOCKET, HANDLE > fd;

    // These....shouldn't be here, but because of the distinction,
    // coupled with the async events api, we have to add our file
    // descriptors to the event queue at object construction,
    // unlike UNIX where these can be separated
    ULONG_PTR listener_id = 0;
    bool isTCP            = false;
    bool write            = false;
    WSAOVERLAPPED portfd[2];

    // for udp?
    ev_io(SOCKET f) : fd(f)
    {
      memset((void*)&portfd[0], 0, sizeof(WSAOVERLAPPED) * 2);
    };
    // for tun
    ev_io(HANDLE t, LossyWriteQueue_t* q) : fd(t), m_LossyWriteQueue(q)
    {
    }
    // for tcp
    ev_io(SOCKET f, LosslessWriteQueue_t* q) : fd(f), m_BlockingWriteQueue(q)
    {
      memset((void*)&portfd[0], 0, sizeof(WSAOVERLAPPED) * 2);
      isTCP = true;
    }
#endif
    virtual int
    read(void* buf, size_t sz) = 0;

    virtual int
    sendto(const sockaddr* dst, const void* data, size_t sz)
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
#ifndef _WIN32
      return write(fd, data, sz);
#else
      DWORD w;
      WriteFile(std::get< HANDLE >(fd), data, sz, nullptr, &portfd[1]);
      GetOverlappedResult(std::get< HANDLE >(fd), &portfd[1], &w, TRUE);
      return w;
#endif
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

    /// called in event loop when fd is ready for writing
    /// requeues anything not written
    /// this assumes fd is set to non blocking
    virtual void
    flush_write()
    {
      if(m_LossyWriteQueue)
        m_LossyWriteQueue->Process([&](WriteBuffer& buffer) {
          do_write(buffer.buf, buffer.bufsz);
          // if we would block we save the entries for later
          // discard entry
        });
      else if(m_BlockingWriteQueue)
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
      /// reset errno
      errno = 0;
#if _WIN32
      SetLastError(0);
#endif
    }

    std::unique_ptr< LossyWriteQueue_t > m_LossyWriteQueue;
    std::unique_ptr< LosslessWriteQueue_t > m_BlockingWriteQueue;
    virtual ~ev_io()
    {
#ifndef _WIN32
      ::close(fd);
#else
      closesocket(std::get< SOCKET >(fd));
#endif
    };
  };

  struct tcp_conn : public ev_io
  {
    bool _shouldClose = false;
    llarp_tcp_conn* tcp;
    tcp_conn(int fd, llarp_tcp_conn* conn)
        : ev_io(fd, new LosslessWriteQueue_t{}), tcp(conn)
    {
    }

    virtual ~tcp_conn()
    {
      delete tcp;
    }

    virtual ssize_t
    do_write(void* buf, size_t sz)
    {
      if(_shouldClose)
        return -1;
#ifdef __linux__
      return ::send(fd, buf, sz, MSG_NOSIGNAL);  // ignore sigpipe
#else
      // TODO: make async
      return ::send(std::get< SOCKET >(fd), (char*)buf, sz, 0);
#endif
    }

    int
    read(void* buf, size_t sz)
    {
      if(_shouldClose)
        return -1;
#ifndef _WIN32
      ssize_t amount = ::read(fd, buf, sz);
#else
      // TODO: make async
      ssize_t amount = ::recv(std::get< SOCKET >(fd), (char*)buf, sz, 0);
#endif
      if(amount > 0)
      {
        if(tcp->read)
          tcp->read(tcp, buf, amount);
      }
      else
      {
        // error
        _shouldClose = true;
        return -1;
      }
      return 0;
    }

    bool
    tick();

    int
    sendto(const sockaddr*, const void*, size_t)
    {
      return -1;
    }
  };

  struct tcp_serv : public ev_io
  {
    llarp_ev_loop* loop;
    llarp_tcp_acceptor* tcp;
    tcp_serv(llarp_ev_loop* l, int fd, llarp_tcp_acceptor* t)
        : ev_io(fd), loop(l), tcp(t)
    {
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

struct llarp_ev_loop
{
  byte_t readbuf[EV_READ_BUF_SZ];
  llarp_time_t _now = 0;
  virtual bool
  init() = 0;
  virtual int
  run() = 0;

  virtual int
  tick(int ms) = 0;

  virtual void
  stop() = 0;

  bool
  udp_listen(llarp_udp_io* l, const sockaddr* src)
  {
    auto ev = create_udp(l, src);
    if(ev)
    {
#ifdef _WIN32
      l->fd = std::get< SOCKET >(ev->fd);
#else
      l->fd          = ev->fd;
#endif
    }
    return ev && add_ev(ev, false);
  }

  virtual llarp::ev_io*
  create_udp(llarp_udp_io* l, const sockaddr* src) = 0;

  virtual bool
  udp_close(llarp_udp_io* l) = 0;
  /// deregister event listener
  virtual bool
  close_ev(llarp::ev_io* ev) = 0;

  virtual llarp::ev_io*
  create_tun(llarp_tun_io* tun) = 0;

  llarp::ev_io*
  bind_tcp(llarp_tcp_acceptor* tcp, const sockaddr* addr);

  /// register event listener
  virtual bool
  add_ev(llarp::ev_io* ev, bool write = false) = 0;

  virtual bool
  running() const = 0;

  virtual ~llarp_ev_loop(){};

  std::list< std::unique_ptr< llarp::ev_io > > handlers;

  void
  tick_listeners()
  {
    auto itr = handlers.cbegin();
    while(itr != handlers.cend())
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
