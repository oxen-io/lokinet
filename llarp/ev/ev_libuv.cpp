#include <ev/ev_libuv.hpp>
#include <util/thread/logic.hpp>
#include <util/thread/queue.hpp>

#include <cstring>

namespace libuv
{
#define LoopCall(h, ...) LogicCall(static_cast<Loop*>((h)->loop->data)->m_Logic, __VA_ARGS__)

  struct glue
  {
    virtual ~glue() = default;
    virtual void
    Close() = 0;
  };

  /// tcp connection glue between llarp and libuv
  struct conn_glue : public glue
  {
    using WriteBuffer_t = std::vector<char>;

    struct WriteEvent
    {
      WriteBuffer_t data;
      uv_write_t request;

      WriteEvent() = default;

      explicit WriteEvent(size_t sz, char* ptr)
      {
        request.data = this;
        data.resize(sz);
        std::copy_n(ptr, sz, data.begin());
      }

      uv_buf_t
      Buffer()
      {
        return uv_buf_init(data.data(), data.size());
      }

      uv_write_t*
      Request()
      {
        return &request;
      }
    };

    uv_tcp_t m_Handle;
    uv_connect_t m_Connect;
    uv_check_t m_Ticker;
    llarp_tcp_connecter* const m_TCP;
    llarp_tcp_acceptor* const m_Accept;
    llarp_tcp_conn m_Conn;
    llarp::SockAddr m_Addr;

    conn_glue(uv_loop_t* loop, llarp_tcp_connecter* tcp, const llarp::SockAddr& addr)
        : m_TCP(tcp), m_Accept(nullptr), m_Addr(addr)
    {
      m_Connect.data = this;
      m_Handle.data = this;
      m_TCP->impl = this;
      uv_tcp_init(loop, &m_Handle);
      m_Ticker.data = this;
      uv_check_init(loop, &m_Ticker);
      m_Conn.close = &ExplicitClose;
      m_Conn.write = &ExplicitWrite;
    }

    conn_glue(uv_loop_t* loop, llarp_tcp_acceptor* tcp, const llarp::SockAddr& addr)
        : m_TCP(nullptr), m_Accept(tcp), m_Addr(addr)
    {
      m_Connect.data = nullptr;
      m_Handle.data = this;
      uv_tcp_init(loop, &m_Handle);
      m_Ticker.data = this;
      uv_check_init(loop, &m_Ticker);
      m_Accept->close = &ExplicitCloseAccept;
      m_Conn.write = nullptr;
      m_Conn.closed = nullptr;
      m_Conn.tick = nullptr;
    }

    conn_glue(conn_glue* parent) : m_TCP(nullptr), m_Accept(nullptr)
    {
      m_Connect.data = nullptr;
      m_Conn.close = &ExplicitClose;
      m_Conn.write = &ExplicitWrite;
      m_Handle.data = this;
      uv_tcp_init(parent->m_Handle.loop, &m_Handle);
      m_Ticker.data = this;
      uv_check_init(parent->m_Handle.loop, &m_Ticker);
    }

    static void
    OnOutboundConnect(uv_connect_t* c, int status)
    {
      conn_glue* self = static_cast<conn_glue*>(c->data);
      self->HandleConnectResult(status);
      c->data = nullptr;
    }

    bool
    ConnectAsync()
    {
      return uv_tcp_connect(&m_Connect, &m_Handle, m_Addr, &OnOutboundConnect) != -1;
    }

    static void
    ExplicitClose(llarp_tcp_conn* conn)
    {
      static_cast<conn_glue*>(conn->impl)->Close();
    }
    static void
    ExplicitCloseAccept(llarp_tcp_acceptor* tcp)
    {
      static_cast<conn_glue*>(tcp->impl)->Close();
    }

    static ssize_t
    ExplicitWrite(llarp_tcp_conn* conn, const byte_t* ptr, size_t sz)
    {
      return static_cast<conn_glue*>(conn->impl)->WriteAsync((char*)ptr, sz);
    }

    static void
    OnRead(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf)
    {
      if (nread >= 0)
      {
        auto* conn = static_cast<conn_glue*>(stream->data);
        conn->Read(buf->base, nread);
      }
      else if (nread < 0)
      {
        static_cast<conn_glue*>(stream->data)->Close();
      }
      delete[] buf->base;
    }

    static void
    Alloc(uv_handle_t*, size_t suggested_size, uv_buf_t* buf)
    {
      buf->base = new char[suggested_size];
      buf->len = suggested_size;
    }

    void
    Read(const char* ptr, ssize_t sz)
    {
      if (m_Conn.read)
      {
        llarp::LogDebug("tcp read ", sz, " bytes");
        const llarp_buffer_t buf(ptr, sz);
        m_Conn.read(&m_Conn, buf);
      }
    }

    void
    HandleConnectResult(int status)
    {
      if (m_TCP && m_TCP->connected)
      {
        if (status == 0)
        {
          m_Conn.impl = this;
          m_Conn.loop = m_TCP->loop;
          m_Conn.close = &ExplicitClose;
          m_Conn.write = &ExplicitWrite;
          m_TCP->connected(m_TCP, &m_Conn);
          Start();
        }
        else if (m_TCP->error)
        {
          llarp::LogError("failed to connect tcp ", uv_strerror(status));
          m_TCP->error(m_TCP);
        }
      }
    }

    void
    WriteFail()
    {
      if (m_Conn.close)
        m_Conn.close(&m_Conn);
    }

    uv_stream_t*
    Stream()
    {
      return (uv_stream_t*)&m_Handle;
    }

    static void
    OnWritten(uv_write_t* req, int status)
    {
      WriteEvent* ev = static_cast<WriteEvent*>(req->data);
      if (status == 0)
      {
        llarp::LogDebug("wrote ", ev->data.size());
      }
      else
      {
        llarp::LogDebug("write fail");
      }
      delete ev;
    }

    int
    WriteAsync(char* data, size_t sz)
    {
      if (uv_is_closing((const uv_handle_t*)&m_Handle))
        return -1;
      WriteEvent* ev = new WriteEvent(sz, data);
      auto buf = ev->Buffer();
      if (uv_write(ev->Request(), Stream(), &buf, 1, &OnWritten) == 0)
        return sz;
      delete ev;
      return -1;
    }

    static void
    OnClosed(uv_handle_t* h)
    {
      conn_glue* conn = static_cast<conn_glue*>(h->data);
      conn->HandleClosed();
    }

    static void
    FullClose(uv_handle_t* h)
    {
      auto* self = static_cast<conn_glue*>(h->data);
      h->data = nullptr;
      delete self;
      llarp::LogDebug("deleted");
    }

    void
    HandleClosed()
    {
      m_Handle.data = nullptr;
      if (m_Accept)
      {
        if (m_Accept->closed)
          m_Accept->closed(m_Accept);
        m_Accept->impl = nullptr;
      }
      if (m_Conn.closed)
      {
        m_Conn.closed(&m_Conn);
      }
      m_Conn.impl = nullptr;
      llarp::LogDebug("closed");
      uv_close((uv_handle_t*)&m_Ticker, &FullClose);
    }

    static void
    OnShutdown(uv_shutdown_t* shut, int code)
    {
      llarp::LogDebug("shut down ", code);
      auto* self = static_cast<conn_glue*>(shut->data);
      uv_close((uv_handle_t*)&self->m_Handle, &OnClosed);
      delete shut;
    }

    void
    Close() override
    {
      if (uv_is_closing((uv_handle_t*)Stream()))
        return;
      llarp::LogDebug("close tcp connection");
      uv_check_stop(&m_Ticker);
      uv_read_stop(Stream());
      auto* shut = new uv_shutdown_t();
      shut->data = this;
      uv_shutdown(shut, Stream(), &OnShutdown);
    }

    static void
    OnAccept(uv_stream_t* stream, int status)
    {
      if (status == 0)
      {
        conn_glue* conn = static_cast<conn_glue*>(stream->data);
        conn->Accept();
      }
      else
      {
        llarp::LogError("tcp accept failed: ", uv_strerror(status));
      }
    }

    static void
    OnTick(uv_check_t* t)
    {
      conn_glue* conn = static_cast<conn_glue*>(t->data);
      conn->Tick();
    }

    void
    Tick()
    {
      if (m_Accept && m_Accept->tick)
      {
        m_Accept->tick(m_Accept);
      }
      if (m_Conn.tick)
      {
        m_Conn.tick(&m_Conn);
      }
    }

    void
    Start()
    {
      auto result = uv_check_start(&m_Ticker, &OnTick);
      if (result)
        llarp::LogError("failed to start timer ", uv_strerror(result));
      result = uv_read_start(Stream(), &Alloc, &OnRead);
      if (result)
        llarp::LogError("failed to start reader ", uv_strerror(result));
    }

    void
    Accept()
    {
      if (m_Accept && m_Accept->accepted)
      {
        auto* child = new conn_glue(this);
        llarp::LogDebug("accepted new connection");
        child->m_Conn.impl = child;
        child->m_Conn.loop = m_Accept->loop;
        child->m_Conn.close = &ExplicitClose;
        child->m_Conn.write = &ExplicitWrite;
        auto res = uv_accept(Stream(), child->Stream());
        if (res)
        {
          llarp::LogError("failed to accept tcp connection ", uv_strerror(res));
          child->Close();
          return;
        }
        m_Accept->accepted(m_Accept, &child->m_Conn);
        child->Start();
      }
    }

    bool
    Server()
    {
      uv_check_start(&m_Ticker, &OnTick);
      m_Accept->close = &ExplicitCloseAccept;
      return uv_tcp_bind(&m_Handle, m_Addr, 0) == 0 && uv_listen(Stream(), 5, &OnAccept) == 0;
    }
  };

  struct ticker_glue : public glue
  {
    std::function<void(void)> func;

    ticker_glue(uv_loop_t* loop, std::function<void(void)> tick) : func(tick)
    {
      m_Ticker.data = this;
      uv_check_init(loop, &m_Ticker);
    }

    static void
    OnTick(uv_check_t* t)
    {
      ticker_glue* ticker = static_cast<ticker_glue*>(t->data);
      ticker->func();
      Loop* loop = static_cast<Loop*>(t->loop->data);
      loop->FlushLogic();
    }

    bool
    Start()
    {
      return uv_check_start(&m_Ticker, &OnTick) != -1;
    }

    void
    Close() override
    {
      uv_check_stop(&m_Ticker);
      uv_close((uv_handle_t*)&m_Ticker, [](auto h) {
        ticker_glue* self = (ticker_glue*)h->data;
        h->data = nullptr;
        delete self;
      });
    }

    uv_check_t m_Ticker;
  };

  struct udp_glue : public glue
  {
    uv_udp_t m_Handle;
    uv_check_t m_Ticker;
    llarp_udp_io* const m_UDP;
    llarp::SockAddr m_Addr;
    std::vector<char> m_Buffer;

    udp_glue(uv_loop_t* loop, llarp_udp_io* udp, const llarp::SockAddr& src)
        : m_UDP(udp), m_Addr(src)
    {
      m_Handle.data = this;
      m_Ticker.data = this;
      uv_udp_init(loop, &m_Handle);
      uv_check_init(loop, &m_Ticker);
    }

    static void
    Alloc(uv_handle_t* h, size_t suggested_size, uv_buf_t* buf)
    {
      udp_glue* self = static_cast<udp_glue*>(h->data);
      if (self->m_Buffer.empty())
        self->m_Buffer.resize(suggested_size);
      buf->base = self->m_Buffer.data();
      buf->len = self->m_Buffer.size();
    }

    /// callback for libuv
    static void
    OnRecv(uv_udp_t* handle, ssize_t nread, const uv_buf_t* buf, const sockaddr* addr, unsigned)
    {
      udp_glue* glue = static_cast<udp_glue*>(handle->data);
      if (addr)
        glue->RecvFrom(nread, buf, llarp::SockAddr(*addr));
    }

    void
    RecvFrom(ssize_t sz, const uv_buf_t* buf, const llarp::SockAddr& fromaddr)
    {
      if (sz > 0 && m_UDP)
      {
        const size_t pktsz = sz;
        if (m_UDP->recvfrom)
        {
          const llarp_buffer_t pkt((const byte_t*)buf->base, pktsz);
          m_UDP->recvfrom(m_UDP, fromaddr, ManagedBuffer{pkt});
        }
      }
    }

    static void
    OnTick(uv_check_t* t)
    {
      udp_glue* udp = static_cast<udp_glue*>(t->data);
      udp->Tick();
    }

    void
    Tick()
    {
      if (m_UDP && m_UDP->tick)
        m_UDP->tick(m_UDP);
    }

    static int
    SendTo(llarp_udp_io* udp, const llarp::SockAddr& to, const byte_t* ptr, size_t sz)
    {
      auto* self = static_cast<udp_glue*>(udp->impl);
      if (self == nullptr)
        return -1;
      auto buf = uv_buf_init((char*)ptr, sz);
      return uv_udp_try_send(&self->m_Handle, &buf, 1, to);
    }

    bool
    Bind()
    {
      auto ret = uv_udp_bind(&m_Handle, m_Addr, 0);
      if (ret)
      {
        llarp::LogError("failed to bind to ", m_Addr, " ", uv_strerror(ret));
        return false;
      }
      if (uv_udp_recv_start(&m_Handle, &Alloc, &OnRecv))
      {
        llarp::LogError("failed to start recving packets via ", m_Addr);
        return false;
      }
      if (uv_check_start(&m_Ticker, &OnTick))
      {
        llarp::LogError("failed to start ticker");
        return false;
      }
#if defined(_WIN32) || defined(_WIN64)
#else
      if (uv_fileno((const uv_handle_t*)&m_Handle, &m_UDP->fd))
        return false;
#endif
      m_UDP->sendto = &SendTo;
      m_UDP->impl = this;
      return true;
    }

    static void
    OnClosed(uv_handle_t* h)
    {
      auto* glue = static_cast<udp_glue*>(h->data);
      if (glue)
      {
        h->data = nullptr;
        delete glue;
      }
    }

    void
    Close() override
    {
      m_UDP->impl = nullptr;
      uv_check_stop(&m_Ticker);
      uv_close((uv_handle_t*)&m_Handle, &OnClosed);
    }
  };

  struct pipe_glue : public glue
  {
    byte_t m_Buffer[1024 * 8];
    llarp_ev_pkt_pipe* const m_Pipe;
    pipe_glue(uv_loop_t* loop, llarp_ev_pkt_pipe* pipe) : m_Pipe(pipe)
    {
      m_Handle.data = this;
      m_Ticker.data = this;
      uv_poll_init(loop, &m_Handle, m_Pipe->fd);
      uv_check_init(loop, &m_Ticker);
    }

    void
    Tick()
    {
      LoopCall(&m_Handle, std::bind(&llarp_ev_pkt_pipe::tick, m_Pipe));
    }

    static void
    OnRead(uv_poll_t* handle, int status, int)
    {
      if (status)
      {
        return;
      }
      pipe_glue* glue = static_cast<pipe_glue*>(handle->data);
      int r = glue->m_Pipe->read(glue->m_Buffer, sizeof(glue->m_Buffer));
      if (r <= 0)
        return;
      const llarp_buffer_t buf{glue->m_Buffer, static_cast<size_t>(r)};
      glue->m_Pipe->OnRead(buf);
    }

    static void
    OnClosed(uv_handle_t* h)
    {
      auto* self = static_cast<pipe_glue*>(h->data);
      if (self)
      {
        h->data = nullptr;
        delete self;
      }
    }

    void
    Close() override
    {
      uv_check_stop(&m_Ticker);
      uv_close((uv_handle_t*)&m_Handle, &OnClosed);
    }

    static void
    OnTick(uv_check_t* h)
    {
      pipe_glue* pipe = static_cast<pipe_glue*>(h->data);
      LoopCall(h, std::bind(&pipe_glue::Tick, pipe));
    }

    bool
    Start()
    {
      if (uv_poll_start(&m_Handle, UV_READABLE, &OnRead))
        return false;
      if (uv_check_start(&m_Ticker, &OnTick))
        return false;
      return true;
    }

    uv_poll_t m_Handle;
    uv_check_t m_Ticker;
  };
#if defined(_WIN32) || defined(_WIN64)
#else
  struct tun_glue : public glue
  {
    uv_poll_t m_Handle;
    uv_check_t m_Ticker;
    llarp_tun_io* const m_Tun;
    device* const m_Device;
    byte_t m_Buffer[1500];
    bool readpkt;

    tun_glue(llarp_tun_io* tun) : m_Tun(tun), m_Device(tuntap_init())
    {
      m_Handle.data = this;
      m_Ticker.data = this;
      readpkt = false;
    }

    ~tun_glue() override
    {
      tuntap_destroy(m_Device);
    }

    static void
    OnTick(uv_check_t* timer)
    {
      tun_glue* tun = static_cast<tun_glue*>(timer->data);
      tun->Tick();
    }

    static void
    OnPoll(uv_poll_t* h, int, int events)
    {
      if (events & UV_READABLE)
      {
        static_cast<tun_glue*>(h->data)->Read();
      }
    }

    void
    Read()
    {
      auto sz = tuntap_read(m_Device, m_Buffer, sizeof(m_Buffer));
      if (sz > 0)
      {
        llarp::LogDebug("tun read ", sz);
        const llarp_buffer_t pkt(m_Buffer, sz);
        if (m_Tun && m_Tun->recvpkt)
          m_Tun->recvpkt(m_Tun, pkt);
      }
    }

    void
    Tick()
    {
      if (m_Tun->before_write)
        m_Tun->before_write(m_Tun);
      if (m_Tun->tick)
        m_Tun->tick(m_Tun);
    }

    static void
    OnClosed(uv_handle_t* h)
    {
      auto* self = static_cast<tun_glue*>(h->data);
      if (self)
      {
        h->data = nullptr;
        delete self;
      }
    }

    void
    Close() override
    {
      if (m_Tun->impl == nullptr)
        return;
      m_Tun->impl = nullptr;
      uv_check_stop(&m_Ticker);
      uv_close((uv_handle_t*)&m_Ticker, [](uv_handle_t* h) {
        tun_glue* glue = static_cast<tun_glue*>(h->data);
        uv_close((uv_handle_t*)&glue->m_Handle, &OnClosed);
      });
    }

    bool
    Write(const byte_t* pkt, size_t sz)
    {
      return tuntap_write(m_Device, (void*)pkt, sz) != -1;
    }

    static bool
    WritePkt(llarp_tun_io* tun, const byte_t* pkt, size_t sz)
    {
      tun_glue* glue = static_cast<tun_glue*>(tun->impl);
      return glue && glue->Write(pkt, sz);
    }

    bool
    Init(uv_loop_t* loop)
    {
      memcpy(m_Device->if_name, m_Tun->ifname, sizeof(m_Device->if_name));
      if (tuntap_start(m_Device, TUNTAP_MODE_TUNNEL, 0) == -1)
      {
        llarp::LogError("failed to start up ", m_Tun->ifname);
        return false;
      }
      if (tuntap_set_ip(m_Device, m_Tun->ifaddr, m_Tun->ifaddr, m_Tun->netmask) == -1)
      {
        llarp::LogError("failed to set address on ", m_Tun->ifname);
        return false;
      }
      if (tuntap_up(m_Device) == -1)
      {
        llarp::LogError("failed to put up ", m_Tun->ifname);
        return false;
      }
      if (m_Device->tun_fd == -1)
      {
        llarp::LogError("tun interface ", m_Tun->ifname, " has invalid fd: ", m_Device->tun_fd);
        return false;
      }

      tuntap_set_nonblocking(m_Device, 1);

      if (uv_poll_init(loop, &m_Handle, m_Device->tun_fd) == -1)
      {
        llarp::LogError("failed to start polling on ", m_Tun->ifname);
        return false;
      }
      if (uv_poll_start(&m_Handle, UV_READABLE, &OnPoll))
      {
        llarp::LogError("failed to start polling on ", m_Tun->ifname);
        return false;
      }
      if (uv_check_init(loop, &m_Ticker) != 0 || uv_check_start(&m_Ticker, &OnTick) != 0)
      {
        llarp::LogError("failed to set up tun interface timer for ", m_Tun->ifname);
        return false;
      }
      m_Tun->writepkt = &WritePkt;
      m_Tun->impl = this;
      return true;
    }
  };
#endif

  void
  Loop::FlushLogic()
  {
    while (not m_LogicCalls.empty())
    {
      auto f = m_LogicCalls.popFront();
      f();
    }
  }

  static void
  OnAsyncWake(uv_async_t* async_handle)
  {
    Loop* loop = static_cast<Loop*>(async_handle->data);
    loop->update_time();
    loop->process_timer_queue();
    loop->process_cancel_queue();
    loop->FlushLogic();
    auto& log = llarp::LogContext::Instance();
    if (log.logStream)
      log.logStream->Tick(loop->time_now());
  }

  Loop::Loop(size_t queue_size)
      : llarp_ev_loop(), m_LogicCalls(queue_size), m_timerQueue(20), m_timerCancelQueue(20)
  {
  }

  bool
  Loop::init()
  {
    if (uv_loop_init(&m_Impl) == -1)
      return false;

#ifdef LOKINET_DEBUG
    last_time = 0;
    loop_run_count = 0;
#endif

    m_Impl.data = this;
#if defined(_WIN32) || defined(_WIN64)
#else
    uv_loop_configure(&m_Impl, UV_LOOP_BLOCK_SIGNAL, SIGPIPE);
#endif
    m_TickTimer = new uv_timer_t;
    m_TickTimer->data = this;
    if (uv_timer_init(&m_Impl, m_TickTimer) == -1)
      return false;
    m_Run.store(true);
    m_nextID.store(0);
    m_WakeUp.data = this;
    uv_async_init(&m_Impl, &m_WakeUp, &OnAsyncWake);
    return true;
  }

  void
  Loop::update_time()
  {
    llarp_ev_loop::update_time();
    uv_update_time(&m_Impl);
  }

  bool
  Loop::running() const
  {
    return m_Run.load();
  }

  bool
  Loop::tcp_connect(llarp_tcp_connecter* tcp, const llarp::SockAddr& addr)
  {
    auto* impl = new conn_glue(&m_Impl, tcp, addr);
    tcp->impl = impl;
    if (impl->ConnectAsync())
      return true;
    delete impl;
    tcp->impl = nullptr;
    return false;
  }

  static void
  OnTickTimeout(uv_timer_t* timer)
  {
    uv_stop(timer->loop);
  }

  int
  Loop::run()
  {
    m_EventLoopThreadID = std::this_thread::get_id();
    return uv_run(&m_Impl, UV_RUN_DEFAULT);
  }

  int
  Loop::tick(int ms)
  {
    if (m_Run)
    {
#ifdef TESTNET_SPEED
      ms *= TESTNET_SPEED;
#endif
      uv_timer_start(m_TickTimer, &OnTickTimeout, ms, 0);
      uv_run(&m_Impl, UV_RUN_ONCE);
    }
    return 0;
  }

  struct TimerData
  {
    Loop* loop;
    uint64_t job_id;
  };

  void
  CloseUVTimer(uv_timer_t* timer)
  {
    // have to delete timer handle this way because libuv.
    uv_timer_stop(timer);
    uv_close((uv_handle_t*)timer, [](uv_handle_t* handle) { delete (uv_timer_t*)handle; });
  }

  static void
  OnUVTimer(uv_timer_t* timer)
  {
    TimerData* timer_data = static_cast<TimerData*>(timer->data);
    Loop* loop = timer_data->loop;
    loop->do_timer_job(timer_data->job_id);

    delete timer_data;
    CloseUVTimer(timer);
  }

  uint32_t
  Loop::call_after_delay(llarp_time_t delay_ms, std::function<void(void)> callback)
  {
#ifdef TESTNET_SPEED
    delay_ms *= TESTNET_SPEED;
#endif
    PendingTimer timer;
    timer.delay_ms = delay_ms;
    timer.callback = callback;
    timer.job_id = m_nextID++;
    uint64_t job_id = timer.job_id;

    m_timerQueue.pushBack(std::move(timer));
    uv_async_send(&m_WakeUp);

    return job_id;
  }

  void
  Loop::cancel_delayed_call(uint32_t job_id)
  {
    m_timerCancelQueue.pushBack(job_id);
    uv_async_send(&m_WakeUp);
  }

  void
  Loop::process_timer_queue()
  {
    while (not m_timerQueue.empty())
    {
      PendingTimer job = m_timerQueue.popFront();
      uint64_t job_id = job.job_id;
      m_pendingCalls.emplace(job_id, std::move(job.callback));

      TimerData* timer_data = new TimerData;
      timer_data->loop = this;
      timer_data->job_id = job_id;

      uv_timer_t* newTimer = new uv_timer_t;
      newTimer->data = (void*)timer_data;

      uv_timer_init(&m_Impl, newTimer);
      uv_timer_start(newTimer, &OnUVTimer, job.delay_ms.count(), 0);
    }
  }

  void
  Loop::process_cancel_queue()
  {
    while (not m_timerCancelQueue.empty())
    {
      uint64_t job_id = m_timerCancelQueue.popFront();
      m_pendingCalls.erase(job_id);
    }
  }

  void
  Loop::do_timer_job(uint64_t job_id)
  {
    auto itr = m_pendingCalls.find(job_id);
    if (itr != m_pendingCalls.end())
    {
      if (itr->second)
        itr->second();
      m_pendingCalls.erase(itr->first);
    }
  }

  void
  Loop::stop()
  {
    if (m_Run)
    {
      llarp::LogInfo("stopping event loop");
      CloseAll();
      uv_stop(&m_Impl);
    }
    m_Run.store(false);
  }

  void
  Loop::CloseAll()
  {
    llarp::LogInfo("Closing all handles");
    uv_walk(
        &m_Impl,
        [](uv_handle_t* h, void*) {
          if (uv_is_closing(h))
            return;
          if (h->data && uv_is_active(h) && h->type != UV_TIMER && h->type != UV_POLL)
          {
            auto glue = reinterpret_cast<libuv::glue*>(h->data);
            if (glue)
              glue->Close();
          }
        },
        nullptr);
  }

  void
  Loop::stopped()
  {
    llarp::LogInfo("we have stopped");
  }

  bool
  Loop::udp_listen(llarp_udp_io* udp, const llarp::SockAddr& src)
  {
    auto* impl = new udp_glue(&m_Impl, udp, src);
    udp->impl = impl;
    if (impl->Bind())
    {
      return true;
    }
    delete impl;
    return false;
  }

  bool
  Loop::add_ticker(std::function<void(void)> func)
  {
    auto* ticker = new ticker_glue(&m_Impl, func);
    if (ticker->Start())
    {
      return true;
    }
    delete ticker;
    return false;
  }

  bool
  Loop::udp_close(llarp_udp_io* udp)
  {
    if (udp == nullptr)
      return false;
    auto* glue = static_cast<udp_glue*>(udp->impl);
    if (glue == nullptr)
      return false;
    glue->Close();
    return true;
  }

  bool
  Loop::tun_listen(llarp_tun_io* tun)
  {
#if defined(_WIN32) || defined(_WIN64)
    (void)tun;
    return false;
#else
    auto* glue = new tun_glue(tun);
    tun->impl = glue;
    if (glue->Init(&m_Impl))
    {
      return true;
    }
    delete glue;
    return false;
#endif
  }

  bool
  Loop::tcp_listen(llarp_tcp_acceptor* tcp, const llarp::SockAddr& addr)
  {
    auto* glue = new conn_glue(&m_Impl, tcp, addr);
    tcp->impl = glue;
    if (glue->Server())
      return true;
    tcp->impl = nullptr;
    delete glue;
    return false;
  }

  bool
  Loop::add_pipe(llarp_ev_pkt_pipe* p)
  {
    auto* glue = new pipe_glue(&m_Impl, p);
    if (glue->Start())
      return true;
    delete glue;
    return false;
  }

  void
  Loop::call_soon(std::function<void(void)> f)
  {
    if (not m_EventLoopThreadID.has_value())
    {
      m_LogicCalls.tryPushBack(f);
      uv_async_send(&m_WakeUp);
      return;
    }
    const auto inEventLoop = *m_EventLoopThreadID == std::this_thread::get_id();

    while (m_LogicCalls.full() and inEventLoop)
    {
      FlushLogic();
    }
    if (inEventLoop)
    {
      if (m_LogicCalls.tryPushBack(f) != llarp::thread::QueueReturn::Success)
      {
        f();
      }
    }
    else
      m_LogicCalls.pushBack(f);
    uv_async_send(&m_WakeUp);
  }

  void
  OnUVPollFDReadable(uv_poll_t* handle, int status, [[maybe_unused]] int events)
  {
    if (status < 0)
      return;  // probably fd was closed

    auto func = static_cast<libuv::Loop::Callback*>(handle->data);

    (*func)();
  }

  void
  Loop::register_poll_fd_readable(int fd, Callback callback)
  {
    if (m_Polls.count(fd))
    {
      llarp::LogError(
          "Attempting to create event loop poll on fd ",
          fd,
          ", but an event loop poll for that fd already exists.");
      return;
    }

    // new a copy as the one passed in here will go out of scope
    auto function_ptr = new Callback(callback);

    auto& new_poll = m_Polls[fd];

    uv_poll_init(&m_Impl, &new_poll, fd);
    new_poll.data = (void*)function_ptr;
    uv_poll_start(&new_poll, UV_READABLE, &OnUVPollFDReadable);
  }

  void
  Loop::deregister_poll_fd_readable(int fd)
  {
    auto itr = m_Polls.find(fd);

    if (itr != m_Polls.end())
    {
      uv_poll_stop(&(itr->second));
      auto func = static_cast<Callback*>(itr->second.data);
      delete func;
      m_Polls.erase(itr);
    }
  }

}  // namespace libuv
