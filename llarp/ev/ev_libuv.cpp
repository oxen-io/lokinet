#include <ev/ev_libuv.hpp>
#include <ev/vpn.hpp>
#include <memory>
#include <thread>
#include <type_traits>
#include <util/thread/queue.hpp>

#include <cstring>
#include "ev/ev.hpp"

#include <uvw.hpp>

namespace llarp::uv
{
  class UVWakeup final : public EventLoopWakeup
  {
    std::shared_ptr<uvw::AsyncHandle> async;

   public:
    UVWakeup(uvw::Loop& loop, std::function<void()> callback)
        : async{loop.resource<uvw::AsyncHandle>()}
    {
      async->on<uvw::AsyncEvent>([f = std::move(callback)](auto&, auto&) { f(); });
    }

    void
    Trigger() override
    {
      async->send();
    }

    ~UVWakeup() override
    {
      async->close();
    }
  };

  class UVRepeater final : public EventLoopRepeater
  {
    std::shared_ptr<uvw::TimerHandle> timer;

   public:
    UVRepeater(uvw::Loop& loop) : timer{loop.resource<uvw::TimerHandle>()}
    {}

    void
    start(llarp_time_t every, std::function<void()> task) override
    {
      timer->start(every, every);
      timer->on<uvw::TimerEvent>([task = std::move(task)](auto&, auto&) { task(); });
    }

    ~UVRepeater() override
    {
      timer->stop();
    }
  };

  struct UDPHandle final : llarp::UDPHandle
  {
    UDPHandle(uvw::Loop& loop, ReceiveFunc rf);

    bool
    listen(const SockAddr& addr) override;

    bool
    send(const SockAddr& dest, const llarp_buffer_t& buf) override;

    std::optional<int>
    file_descriptor() override
    {
#ifndef _WIN32
      if (int fd = handle->fd(); fd >= 0)
        return fd;
#endif
      return std::nullopt;
    }

    void
    close() override;

    ~UDPHandle() override;

   private:
    std::shared_ptr<uvw::UDPHandle> handle;

    void
    reset_handle(uvw::Loop& loop);
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

  void
  Loop::tick_event_loop()
  {
    llarp::LogTrace("ticking event loop.");
    FlushLogic();
    PumpLL();
    auto& log = llarp::LogContext::Instance();
    if (log.logStream)
      log.logStream->Tick(time_now());
  }

  Loop::Loop(size_t queue_size) : llarp::EventLoop{}, PumpLL{[] {}}, m_LogicCalls{queue_size}
  {
    if (!(m_Impl = uvw::Loop::create()))
      throw std::runtime_error{"Failed to construct libuv loop"};

#ifdef LOKINET_DEBUG
    last_time = 0;
    loop_run_count = 0;
#endif

#ifndef _WIN32
    signal(SIGPIPE, SIG_IGN);
#endif

    m_Run.store(true);
    m_nextID.store(0);
    if (!(m_WakeUp = m_Impl->resource<uvw::AsyncHandle>()))
      throw std::runtime_error{"Failed to create libuv async"};
    m_WakeUp->on<uvw::AsyncEvent>([this](const auto&, auto&) { tick_event_loop(); });
  }

  bool
  Loop::running() const
  {
    return m_Run.load();
  }

  void
  Loop::run()
  {
    llarp::LogTrace("Loop::run_loop()");
    m_EventLoopThreadID = std::this_thread::get_id();
    m_Impl->run();
    m_Impl->close();
    m_Impl.reset();
    llarp::LogInfo("we have stopped");
  }

  void
  Loop::wakeup()
  {
    m_WakeUp->send();
  }

  void
  Loop::set_pump_function(std::function<void(void)> pump)
  {
    PumpLL = std::move(pump);
  }

  std::shared_ptr<llarp::UDPHandle>
  Loop::make_udp(UDPReceiveFunc on_recv)
  {
    return std::static_pointer_cast<llarp::UDPHandle>(
        std::make_shared<llarp::uv::UDPHandle>(*m_Impl, std::move(on_recv)));
  }

  static void
  setup_oneshot_timer(uvw::Loop& loop, llarp_time_t delay, std::function<void()> callback)
  {
    auto timer = loop.resource<uvw::TimerHandle>();
    timer->on<uvw::TimerEvent>([f = std::move(callback)](const auto&, auto& timer) {
      f();
      timer.stop();
      timer.close();
    });
    timer->start(delay, 0ms);
  }

  void
  Loop::call_later(llarp_time_t delay_ms, std::function<void(void)> callback)
  {
    llarp::LogTrace("Loop::call_after_delay()");
#ifdef TESTNET_SPEED
    delay_ms *= TESTNET_SPEED;
#endif

    if (inEventLoop())
      setup_oneshot_timer(*m_Impl, delay_ms, std::move(callback));
    else
    {
      call_soon([this, f = std::move(callback), target_time = time_now() + delay_ms] {
        // Recalculate delay because it may have taken some time to get ourselves into the logic
        // thread
        auto updated_delay = target_time - time_now();
        if (updated_delay <= 0ms)
          f();  // Timer already expired!
        else
          setup_oneshot_timer(*m_Impl, updated_delay, std::move(f));
      });
    }
  }

  void
  Loop::stop()
  {
    if (m_Run)
    {
      if (not inEventLoop())
        return call_soon([this] { stop(); });

      llarp::LogInfo("stopping event loop");
      m_Impl->walk([](auto&& handle) {
        if constexpr (!std::is_pointer_v<std::remove_reference_t<decltype(handle)>>)
          handle.close();
      });
      llarp::LogDebug("Closed all handles, stopping the loop");
      m_Impl->stop();

      m_Run.store(false);
    }
  }

  bool
  Loop::add_ticker(std::function<void(void)> func)
  {
    auto check = m_Impl->resource<uvw::CheckHandle>();
    check->on<uvw::CheckEvent>([f = std::move(func)](auto&, auto&) { f(); });
    check->start();
    return true;
  }

  bool
  Loop::add_network_interface(
      std::shared_ptr<llarp::vpn::NetworkInterface> netif,
      std::function<void(llarp::net::IPPacket)> handler)
  {
#ifndef _WIN32
    using event_t = uvw::PollEvent;
    auto handle = m_Impl->resource<uvw::PollHandle>(netif->PollFD());
#else
    using event_t = uvw::CheckEvent;
    auto handle = m_Impl->resource<uvw::CheckHandle>();
#endif
    if (!handle)
      return false;

    handle->on<event_t>([netif = std::move(netif), handler = std::move(handler)](
                            const event_t&, [[maybe_unused]] auto& handle) {
      for (auto pkt = netif->ReadNextPacket(); pkt.sz > 0; pkt = netif->ReadNextPacket())
      {
        LogDebug("got packet ", pkt.sz);
        if (handler)
          handler(std::move(pkt));
      }
    });

#ifndef _WIN32
    handle->start(uvw::PollHandle::Event::READABLE);
#else
    handle->start();
#endif

    return true;
  }

  void
  Loop::call_soon(std::function<void(void)> f)
  {
    if (not m_EventLoopThreadID.has_value())
    {
      m_LogicCalls.tryPushBack(f);
      m_WakeUp->send();
      return;
    }

    if (inEventLoop() and m_LogicCalls.full())
    {
      FlushLogic();
    }
    m_LogicCalls.pushBack(f);
    m_WakeUp->send();
  }

  // Sets `handle` to a new uvw UDP handle, first initiating a close and then disowning the handle
  // if already set, allocating the resource, and setting the receive event on it.
  void
  UDPHandle::reset_handle(uvw::Loop& loop)
  {
    if (handle)
      handle->close();
    handle = loop.resource<uvw::UDPHandle>();
    handle->on<uvw::UDPDataEvent>([this](auto& event, auto& /*handle*/) {
      on_recv(
          *this,
          SockAddr{event.sender.ip, static_cast<uint16_t>(event.sender.port)},
          OwnedBuffer{std::move(event.data), event.length});
    });
  }

  llarp::uv::UDPHandle::UDPHandle(uvw::Loop& loop, ReceiveFunc rf) : llarp::UDPHandle{std::move(rf)}
  {
    reset_handle(loop);
  }

  bool
  UDPHandle::listen(const SockAddr& addr)
  {
    if (handle->active())
      reset_handle(handle->loop());

    bool good = true;
    auto err = handle->on<uvw::ErrorEvent>([&](auto& event, auto&) {
      llarp::LogError("failed to bind and start receiving on ", addr, ": ", event.what());
      good = false;
    });
    handle->bind(*static_cast<const sockaddr*>(addr));
    if (good)
      handle->recv();
    handle->erase(err);
    return good;
  }

  bool
  UDPHandle::send(const SockAddr& to, const llarp_buffer_t& buf)
  {
    return handle->trySend(
               *static_cast<const sockaddr*>(to),
               const_cast<char*>(reinterpret_cast<const char*>(buf.base)),
               buf.sz)
        >= 0;
  }

  void
  UDPHandle::close()
  {
    handle->close();
    handle.reset();
  }

  UDPHandle::~UDPHandle()
  {
    close();
  }

  std::shared_ptr<llarp::EventLoopWakeup>
  Loop::make_waker(std::function<void()> callback)
  {
    return std::static_pointer_cast<llarp::EventLoopWakeup>(
        std::make_shared<UVWakeup>(*m_Impl, std::move(callback)));
  }

  std::shared_ptr<EventLoopRepeater>
  Loop::make_repeater()
  {
    return std::static_pointer_cast<EventLoopRepeater>(std::make_shared<UVRepeater>(*m_Impl));
  }

  bool
  Loop::inEventLoop() const
  {
    return m_EventLoopThreadID and *m_EventLoopThreadID == std::this_thread::get_id();
  }

}  // namespace llarp::uv
