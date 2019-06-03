#include "ev_libuv.hpp"
#include "net/net_addr.hpp"

namespace libuv
{
  /// tcp connection glue between llarp and libuv
  struct conn_glue
  {
    uv_tcp_t m_Handle;
    uv_connect_t m_Connect;
    llarp_tcp_connecter* const m_TCP;
    llarp_tcp_acceptor* const m_Accept;
    llarp_tcp_conn m_Conn;
    llarp::Addr m_Addr;

    conn_glue(uv_loop_t* loop, llarp_tcp_connecter* tcp, const sockaddr* addr)
        : m_TCP(tcp), m_Accept(nullptr), m_Addr(*addr)
    {
      m_Connect.data = this;
      m_Handle.data  = tcp;
      uv_tcp_init(loop, &m_Handle);
    }

    conn_glue(uv_loop_t* loop, llarp_tcp_acceptor* tcp, const sockaddr* addr)
        : m_TCP(nullptr), m_Accept(tcp), m_Addr(*addr)
    {
      m_Handle.data = this;
      uv_tcp_init(loop, &m_Handle);
    }

    conn_glue(conn_glue* parent) : m_TCP(nullptr), m_Accept(nullptr)
    {
      uv_tcp_init(parent->m_Handle.loop, &m_Handle);
    }

    static void
    OnOutboundConnect(uv_connect_t* c, int status)
    {
      conn_glue* self = static_cast< conn_glue* >(c->data);
      self->HandleConnectResult(status);
    }

    bool
    ConnectAsync()
    {
      return uv_tcp_connect(&m_Connect, &m_Handle, m_Addr, &OnOutboundConnect)
          != -1;
    }

    static void
    ExplicitClose(llarp_tcp_conn* conn)
    {
      static_cast< conn_glue* >(conn->impl)->Close();
    }

    static void
    OnRead(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf)
    {
      static_cast< conn_glue* >(stream->data)->Read(buf->base, nread);
      delete[] buf->base;
    }

    static void
    Alloc(uv_handle_t*, size_t suggested_size, uv_buf_t* buf)
    {
      buf->base = new char[suggested_size];
      buf->len  = suggested_size;
    }

    void
    Read(const char* ptr, ssize_t sz)
    {
      const llarp_buffer_t buf(ptr, sz);
      m_Conn.read(&m_Conn, buf);
    }

    void
    HandleConnectResult(int status)
    {
      if(m_TCP && m_TCP->connected)
      {
        if(status == 0)
        {
          m_Conn.impl  = this;
          m_Conn.loop  = m_TCP->loop;
          m_Conn.close = &ExplicitClose;
          m_TCP->connected(m_TCP, &m_Conn);
          uv_read_start(Stream(), &Alloc, &OnRead);
        }
        else if(m_TCP->error)
          m_TCP->error(m_TCP);
      }
    }

    void
    WriteFail()
    {
      m_Conn.close(&m_Conn);
    }

    static void
    OnWritten(uv_write_t* req, int status)
    {
      conn_glue* self = static_cast< conn_glue* >(req->data);
      if(status)
        self->WriteFail();
      delete req;
    }

    uv_stream_t*
    Stream()
    {
      return (uv_stream_t*)&m_Handle;
    }

    bool
    WriteAsync(const void* data, size_t sz)
    {
      uv_write_t* request = new uv_write_t();
      request->data       = this;
      auto buf            = uv_buf_init((char*)data, sz);
      return uv_write(request, Stream(), &buf, 1, &OnWritten) != -1;
    }

    static void
    OnClosed(uv_handle_t* h)
    {
      static_cast< conn_glue* >(h->data)->HandleClosed();
    }

    void
    HandleClosed()
    {
      m_Handle.data = nullptr;
      if(m_Accept && m_Accept->closed)
        m_Accept->closed(m_Accept);
      if(m_Conn.closed)
        m_Conn.closed(&m_Conn);
      delete this;
    }

    void
    Close()
    {
      uv_close((uv_handle_t*)&m_Handle, &OnClosed);
    }

    static void
    OnAccept(uv_stream_t* stream, int status)
    {
      if(status == 0)
      {
        static_cast< conn_glue* >(stream->data)->Accept();
      }
    }

    void
    Accept()
    {
      if(m_Accept && m_Accept->accepted)
      {
        conn_glue* child = new conn_glue(this);
        uv_accept(Stream(), child->Stream());
        m_Accept->accepted(m_Accept, &child->m_Conn);
      }
    }

    bool
    Server()
    {
      return uv_tcp_bind(&m_Handle, m_Addr, 0) == 0
          && uv_listen(Stream(), 5, &OnAccept) == 0;
    }
  };

  struct udp_glue
  {
    uv_udp_t m_Handle;
    uv_timer_t m_Ticker;
    llarp_udp_io* const m_UDP;
    llarp::Addr m_Addr;
    bool gotpkts;

    udp_glue(uv_loop_t* loop, llarp_udp_io* udp, const sockaddr* src)
        : m_UDP(udp), m_Addr(*src)
    {
      m_Handle.data = this;
      m_Ticker.data = this;
      gotpkts       = false;
      uv_udp_init(loop, &m_Handle);
      uv_timer_init(loop, &m_Ticker);
    }

    static void
    Alloc(uv_handle_t*, size_t suggested_size, uv_buf_t* buf)
    {
      buf->base = new char[suggested_size];
      buf->len  = suggested_size;
    }

    static void
    OnRecv(uv_udp_t* handle, ssize_t nread, const uv_buf_t* buf,
           const struct sockaddr* addr, unsigned)
    {
      if(addr)
        static_cast< udp_glue* >(handle->data)->RecvFrom(nread, buf, addr);
      delete[] buf->base;
    }

    void
    RecvFrom(ssize_t sz, const uv_buf_t* buf, const struct sockaddr* fromaddr)
    {
      if(sz >= 0)
      {
        const size_t pktsz = sz;
        const llarp_buffer_t pkt{(const byte_t*)buf->base, pktsz};
        m_UDP->recvfrom(m_UDP, fromaddr, ManagedBuffer{pkt});
        gotpkts = true;
      }
    }

    static void
    OnTick(uv_timer_t* t)
    {
      static_cast< udp_glue* >(t->data)->Tick();
    }

    void
    Tick()
    {
      if(gotpkts)
      {
        llarp::LogDebug("udp tick");
        if(m_UDP && m_UDP->tick)
          m_UDP->tick(m_UDP);
      }
      gotpkts = false;
      uv_timer_again(&m_Ticker);
    }

    static int
    SendTo(llarp_udp_io* udp, const sockaddr* to, const byte_t* ptr, size_t sz)
    {
      udp_glue* self = static_cast< udp_glue* >(udp->impl);
      uv_buf_t buf   = uv_buf_init((char*)ptr, sz);
      return uv_udp_try_send(&self->m_Handle, &buf, 1, to);
    }

    bool
    Bind()
    {
      auto ret = uv_udp_bind(&m_Handle, m_Addr, 0);
      if(ret)
      {
        llarp::LogError("failed to bind to ", m_Addr, " ", uv_strerror(ret));
        return false;
      }
      if(uv_udp_recv_start(&m_Handle, &Alloc, &OnRecv))
      {
        llarp::LogError("failed to start recving packets via ", m_Addr);
        return false;
      }
      if(uv_timer_start(&m_Ticker, &OnTick, 50, 50))
      {
        llarp::LogError("failed to start ticker");
        return false;
      }
      if(uv_fileno((const uv_handle_t*)&m_Handle, &m_UDP->fd))
        return false;
      m_UDP->sendto = &SendTo;
      return true;
    }

    static void
    OnClosed(uv_handle_t* h)
    {
      udp_glue* glue    = static_cast< udp_glue* >(h->data);
      glue->m_UDP->impl = nullptr;
      delete glue;
    }

    void
    Close()
    {
      uv_timer_stop(&m_Ticker);
      uv_close((uv_handle_t*)&m_Handle, &OnClosed);
    }
  };

  struct tun_glue
  {
    uv_poll_t m_Handle;
    uv_timer_t m_Ticker;
    llarp_tun_io* const m_Tun;
    device* const m_Device;
    byte_t m_Buffer[1500];
    bool readpkt;

    tun_glue(llarp_tun_io* tun) : m_Tun(tun), m_Device(tuntap_init())
    {
      m_Handle.data = this;
      m_Ticker.data = this;
      readpkt       = false;
    }

    ~tun_glue()
    {
      tuntap_destroy(m_Device);
    }

    static void
    OnTick(uv_timer_t* timer)
    {
      static_cast< tun_glue* >(timer->data)->Tick();
    }

    static void
    OnPoll(uv_poll_t* h, int status, int)
    {
      if(status == 0)
      {
        static_cast< tun_glue* >(h->data)->Read();
      }
    }

    void
    Read()
    {
      auto sz = tuntap_read(m_Device, m_Buffer, sizeof(m_Buffer));
      llarp_buffer_t pkt(m_Buffer, sz);
      if(m_Tun && m_Tun->recvpkt)
        m_Tun->recvpkt(m_Tun, pkt);
      readpkt = true;
    }

    void
    Tick()
    {
      if(readpkt)
      {
        if(m_Tun->tick)
          m_Tun->tick(m_Tun);
        if(m_Tun->before_write)
          m_Tun->before_write(m_Tun);
      }
      readpkt = false;
      uv_timer_again(&m_Ticker);
    }

    static void
    OnClosed(uv_handle_t* h)
    {
      tun_glue* self = static_cast< tun_glue* >(h->data);
      delete self;
    }

    void
    Close()
    {
      uv_close((uv_handle_t*)&m_Handle, &OnClosed);
    }

    bool
    Init(uv_loop_t* loop)
    {
      strncpy(m_Device->if_name, m_Tun->ifname, sizeof(m_Device->if_name));
      if(tuntap_start(m_Device, TUNTAP_MODE_TUNNEL, 0) == -1)
      {
        llarp::LogError("failed to start up ", m_Tun->ifname);
        return false;
      }
      if(tuntap_up(m_Device) == -1)
      {
        llarp::LogError("failed to put up ", m_Tun->ifname);
        return false;
      }
      if(m_Device->tun_fd == -1)
      {
        llarp::LogError("tun interface ", m_Tun->ifname,
                        " has invalid fd: ", m_Device->tun_fd);
        return false;
      }
      if(uv_poll_init(loop, &m_Handle, m_Device->tun_fd) == -1)
      {
        llarp::LogError("failed to start polling on ", m_Tun->ifname);
        return false;
      }
      if(uv_poll_start(&m_Handle, UV_READABLE, &OnPoll))
      {
        llarp::LogError("failed to start polling on ", m_Tun->ifname);
        return false;
      }
      if(uv_timer_init(loop, &m_Ticker) != 0
         || uv_timer_start(&m_Ticker, &OnTick, 50, 50) != 0)
      {
        llarp::LogError("failed to set up tun interface timer for ",
                        m_Tun->ifname);
        return false;
      }
      return true;
    }
  };

  bool
  Loop::init()
  {
    m_Impl.reset(uv_loop_new());
    if(uv_loop_init(m_Impl.get()) == -1)
      return false;
    m_TickTimer.data = this;
    m_Run.store(true);
    return uv_timer_init(m_Impl.get(), &m_TickTimer) != -1;
  }

  void
  Loop::update_time()
  {
    uv_update_time(m_Impl.get());
  }

  bool
  Loop::running() const
  {
    return m_Run.load();
  }

  llarp_time_t
  Loop::time_now() const
  {
    return llarp::time_now_ms();
  }

  bool
  Loop::tcp_connect(llarp_tcp_connecter* tcp, const sockaddr* addr)
  {
    conn_glue* impl = new conn_glue(m_Impl.get(), tcp, addr);
    tcp->impl       = impl;
    if(impl->ConnectAsync())
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
  Loop::tick(int ms)
  {
    uv_timer_start(&m_TickTimer, &OnTickTimeout, ms, 0);
    uv_run(m_Impl.get(), UV_RUN_ONCE);
    return 0;
  }

  void
  Loop::stop()
  {
    llarp::LogInfo("stopping event loop");
    m_Run.store(false);
  }

  bool
  Loop::udp_listen(llarp_udp_io* udp, const sockaddr* src)
  {
    udp_glue* impl = new udp_glue(m_Impl.get(), udp, src);
    udp->impl      = impl;
    if(impl->Bind())
    {
      m_CloseFuncs.emplace_back(std::bind(&udp_glue::Close, impl));
      return true;
    }
    return false;
  }

  bool
  Loop::udp_close(llarp_udp_io* udp)
  {
    if(udp == nullptr)
      return false;
    udp_glue* glue = static_cast< udp_glue* >(udp->impl);
    if(glue == nullptr)
      return false;
    glue->Close();
    return true;
  }

  bool
  Loop::tun_listen(llarp_tun_io* tun)
  {
    tun_glue* glue = new tun_glue(tun);
    tun->impl      = glue;
    if(glue->Init(m_Impl.get()))
    {
      m_CloseFuncs.emplace_back(std::bind(&tun_glue ::Close, glue));
      return true;
    }
    delete glue;
    return false;
  }

  bool
  Loop::tcp_listen(llarp_tcp_acceptor* tcp, const sockaddr* addr)
  {
    conn_glue* glue = new conn_glue(m_Impl.get(), tcp, addr);
    tcp->impl       = glue;
    if(glue->Server())
      return true;
    tcp->impl = nullptr;
    delete glue;
    return false;
  }

}  // namespace libuv

llarp_ev_loop_ptr
llarp_make_uv_loop()
{
  auto loop = std::make_shared< libuv::Loop >();
  if(loop->init())
    return loop;
  return nullptr;
}
