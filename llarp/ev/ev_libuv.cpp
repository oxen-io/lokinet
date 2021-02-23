#include <ev/ev_libuv.hpp>
#include <ev/vpn.hpp>
#include <util/thread/logic.hpp>
#include <util/thread/queue.hpp>

#include <cstring>

namespace libuv
{
#define LoopCall(h, ...)    \
  {                         \
    auto __f = __VA_ARGS__; \
    __f();                  \
  }

  struct glue
  {
    virtual ~glue() = default;
    virtual void
    Close() = 0;
  };

  class UVWakeup final : public llarp::EventLoopWakeup, public glue
  {
    uv_async_t m_Impl;
    const int m_Idx;
    static void
    OnWake(uv_async_t* self)
    {
      static_cast<UVWakeup*>(self->data)->callback();
    }

   public:
    UVWakeup(uv_loop_t* loop, std::function<void()> hook, int idx)
        : llarp::EventLoopWakeup{hook}, m_Idx{idx}
    {
      uv_async_init(loop, &m_Impl, OnWake);
      m_Impl.data = this;
    }

    ~UVWakeup() = default;

    void
    Close() override
    {
      uv_close((uv_handle_t*)&m_Impl, [](uv_handle_t* h) {
        auto loop = static_cast<libuv::Loop*>(h->loop->data);
        loop->delete_waker(static_cast<UVWakeup*>(h->data)->m_Idx);
      });
    }

    void
    End() override
    {
      Close();
    }

    void
    Wakeup() override
    {
      uv_async_send(&m_Impl);
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
      llarp::LogTrace("ticker_glue::OnTick() start");
      ticker_glue* ticker = static_cast<ticker_glue*>(t->data);
      ticker->func();
      // Loop* loop = static_cast<Loop*>(t->loop->data);
      // loop->FlushLogic();
      llarp::LogTrace("ticker_glue::OnTick() end");
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
      llarp::LogTrace("udp_glue::OnTick() start");
      udp_glue* udp = static_cast<udp_glue*>(t->data);
      udp->Tick();
      llarp::LogTrace("udp_glue::OnTick() end");
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
      const auto buf = uv_buf_init((char*)ptr, sz);
      return uv_udp_try_send(
          &self->m_Handle, &buf, 1, (const sockaddr*)static_cast<const sockaddr_in*>(to));
    }

    bool
    Bind()
    {
      auto ret =
          uv_udp_bind(&m_Handle, (const sockaddr*)static_cast<const sockaddr_in*>(m_Addr), 0);
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

  struct tun_glue : public glue
  {
    uv_poll_t m_Handle;
    uv_check_t m_Ticker;
    std::shared_ptr<llarp::vpn::NetworkInterface> m_NetIf;
    std::function<void(llarp::net::IPPacket)> m_Handler;

    tun_glue(
        std::shared_ptr<llarp::vpn::NetworkInterface> netif,
        std::function<void(llarp::net::IPPacket)> handler)
        : m_NetIf{std::move(netif)}, m_Handler{std::move(handler)}
    {
      m_Handle.data = this;
      m_Ticker.data = this;
    }

    static void
    OnTick(uv_check_t* h)
    {
      auto self = static_cast<tun_glue*>(h->data);
      while (self->m_NetIf->HasNextPacket())
        self->Read();
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
      auto pkt = m_NetIf->ReadNextPacket();
      LogDebug("got packet ", pkt.sz);
      if (m_Handler)
        m_Handler(std::move(pkt));
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
      uv_check_stop(&m_Ticker);
#ifndef _WIN32
      uv_close((uv_handle_t*)&m_Handle, &OnClosed);
#endif
    }

    bool
    Init(uv_loop_t* loop)
    {
      if (uv_check_init(loop, &m_Ticker) == -1)
      {
        return false;
      }
      if (uv_check_start(&m_Ticker, &OnTick) == -1)
      {
        return false;
      }
#ifndef _WIN32
      if (uv_poll_init(loop, &m_Handle, m_NetIf->PollFD()) == -1)
      {
        llarp::LogError("failed to initialize polling on ", m_NetIf->IfName());
        return false;
      }
      if (uv_poll_start(&m_Handle, UV_READABLE, &OnPoll))
      {
        llarp::LogError("failed to start polling on ", m_NetIf->IfName());
        return false;
      }
#endif
      return true;
    }
  };

  void
  Loop::FlushLogic()
  {
    llarp::LogTrace("Loop::FlushLogic() start");
    while (not m_LogicCalls.empty())
    {
      auto f = m_LogicCalls.popFront();
      f();
    }
    llarp::LogTrace("Loop::FlushLogic() end");
  }

  static void
  OnAsyncWake(uv_async_t* async_handle)
  {
    llarp::LogTrace("OnAsyncWake, ticking event loop.");
    Loop* loop = static_cast<Loop*>(async_handle->data);
    loop->update_time();
    loop->process_timer_queue();
    loop->process_cancel_queue();
    loop->FlushLogic();
    loop->PumpLL();
    auto& log = llarp::LogContext::Instance();
    if (log.logStream)
      log.logStream->Tick(loop->time_now());
  }

  constexpr size_t TimerQueueSize = 20;

  Loop::Loop(size_t queue_size)
      : llarp::EventLoop{}
      , PumpLL{[]() {}}
      , m_LogicCalls{queue_size}
      , m_timerQueue{TimerQueueSize}
      , m_timerCancelQueue{TimerQueueSize}
  {}

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
    llarp::EventLoop::update_time();
    uv_update_time(&m_Impl);
  }

  bool
  Loop::running() const
  {
    return m_Run.load();
  }

  static void
  OnTickTimeout(uv_timer_t* timer)
  {
    uv_stop(timer->loop);
  }

  int
  Loop::run()
  {
    llarp::LogTrace("Loop::run()");
    m_EventLoopThreadID = std::this_thread::get_id();
    return uv_run(&m_Impl, UV_RUN_DEFAULT);
  }

  void
  Loop::set_pump_function(std::function<void(void)> pump)
  {
    PumpLL = std::move(pump);
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
    llarp::LogTrace("Loop::call_after_delay()");
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
    llarp::LogError("Loop::udp_listen failed to bind");
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
  Loop::add_network_interface(
      std::shared_ptr<llarp::vpn::NetworkInterface> netif,
      std::function<void(llarp::net::IPPacket)> handler)
  {
    auto* glue = new tun_glue(netif, handler);
    // call to Init gives ownership of glue to event loop
    if (glue->Init(&m_Impl))
      return true;
    delete glue;
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

    if (inEventLoop and m_LogicCalls.full())
    {
      FlushLogic();
    }
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

  llarp::EventLoopWakeup*
  Loop::make_event_loop_waker(std::function<void()> callback)
  {
    auto wake_idx = m_NumWakers++;
    auto wake = new UVWakeup{&m_Impl, callback, wake_idx};
    m_Wakers[wake_idx] = wake;
    return wake;
  }

  void
  Loop::delete_waker(int idx)
  {
    delete m_Wakers[idx];
    m_Wakers.erase(idx);
  }

}  // namespace libuv
