#include "libuv.hpp"
#include <cstdint>
#include <memory>
#include <thread>
#include <type_traits>
#include <cstring>

#include <llarp/util/exceptions.hpp>
#include <llarp/util/thread/queue.hpp>
#include <llarp/vpn/platform.hpp>

#include <uvw.hpp>
#include <llarp/constants/platform.hpp>
#include "llarp/ev/ev.hpp"
#include "llarp/net/net_int.hpp"
#include "llarp/net/sock_addr.hpp"
#include "llarp/util/buffer.hpp"
#include "uvw/udp.h"
#include "uvw/util.h"

namespace llarp::uv
{
  std::shared_ptr<uvw::Loop>
  Loop::MaybeGetUVWLoop()
  {
    return m_Impl;
  }

  class UVWakeup final : public EventLoopWakeup
  {
    std::shared_ptr<uvw::AsyncHandle> async;
    std::atomic_flag _triggered = ATOMIC_FLAG_INIT;

   public:
    UVWakeup(uvw::Loop& loop, std::function<void()> callback)
        : async{loop.resource<uvw::AsyncHandle>()}
    {
      async->on<uvw::AsyncEvent>([this, f = std::move(callback)](auto&, auto&) {
        _triggered.clear();
        f();
      });
    }

    bool
    Trigger() override
    {
      bool ret = _triggered.test_and_set();
      async->send();
      return ret;
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
    UDPHandle(uvw::Loop& loop, ReceiveFunc rf, std::optional<SockAddr> laddr);

    bool
    send(const SockAddr& dest, const llarp_buffer_t& buf) override;

    SockAddr
    LocalAddr() const override
    {
      if (not maybe_laddr)
      {
        auto addr = handle->sock<uvw::IPv4>();
        if (addr.ip.empty())
          addr = handle->sock<uvw::IPv6>();

        maybe_laddr = SockAddr{addr.ip, huint16_t{static_cast<uint16_t>(addr.port)}};
      }
      return *maybe_laddr;
    }

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
    mutable std::optional<SockAddr> maybe_laddr;
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
  }

  Loop::Loop(size_t queue_size) : llarp::EventLoop{}, m_LogicCalls{queue_size}
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

  std::shared_ptr<llarp::UDPHandle>
  Loop::make_udp(UDPReceiveFunc on_recv, const std::optional<SockAddr>& laddr)
  {
    return std::static_pointer_cast<llarp::UDPHandle>(
        std::make_shared<llarp::uv::UDPHandle>(*m_Impl, std::move(on_recv), laddr));
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
    auto reader = [netif, handler = std::move(handler), this](const auto&, auto&) {
      for (auto pkt = netif->ReadNextPacket(); true; pkt = netif->ReadNextPacket())
      {
        if (pkt.empty())
          return;
        if (handler)
          handler(std::move(pkt));
        // on protactor style platforms we want to emulate the reactor style pattern so we wakeup
        // the event loop when we get packets.
        if constexpr (platform::has_proactor_io)
          wakeup();
      }
    };

    if constexpr (llarp::platform::has_reactor_io)
    {
      auto handle = m_Impl->resource<uvw::PollHandle>(netif->PollFD());
      if (not handle)
        return false;
      handle->on<uvw::PollEvent>(std::move(reader));
      handle->start(uvw::PollHandle::Event::READABLE);
    }
    else
    {
      auto handle = m_Impl->resource<uvw::PrepareHandle>();
      if (not handle)
        return false;
      handle->on<uvw::PrepareEvent>(std::move(reader));
      handle->start();
    }
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
  }

  llarp::uv::UDPHandle::UDPHandle(uvw::Loop& loop, ReceiveFunc rf, std::optional<SockAddr> laddr)
      : llarp::UDPHandle{std::move(rf)}, handle{loop.resource<uvw::UDPHandle>()}
  {
    std::optional<std::string> maybe_addr_str;
    if (laddr)
      maybe_addr_str = laddr->ToString();

    auto err = handle->on<uvw::ErrorEvent>(
        [addr_str = maybe_addr_str.value_or("all interfaces")](auto& event, auto&) {
          throw llarp::util::bind_socket_error{
              fmt::format("failed to bind udp socket on {}: {}", addr_str, event.what())};
        });
    if (laddr)
      handle->bind(*static_cast<const sockaddr*>(*laddr));
    handle->on<uvw::UDPDataEvent>([this](auto& ev, auto&) {
      on_recv(
          *this,
          SockAddr{ev.sender.ip, huint16_t{static_cast<uint16_t>(ev.sender.port)}},
          OwnedBuffer{std::move(ev.data), ev.length});
    });
    handle->recv();
    handle->erase(err);
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
    if (m_EventLoopThreadID)
      return *m_EventLoopThreadID == std::this_thread::get_id();
    // assume we are in it because we haven't started up yet
    return true;
  }

}  // namespace llarp::uv
