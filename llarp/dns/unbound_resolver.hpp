#pragma once

#include <unbound.h>
#include <mutex>
#include <atomic>
#include <memory>
#include <queue>

#include <ev/ev.hpp>
#include <util/thread/logic.hpp>

#include <dns/message.hpp>

#ifdef _WIN32
#include <thread>
#endif

namespace llarp::dns
{
  using ReplyFunction = std::function<void(SockAddr source, std::vector<byte_t> buf)>;
  using FailFunction = std::function<void(SockAddr source, Message msg)>;

  class UnboundResolver : public std::enable_shared_from_this<UnboundResolver>
  {
   private:
    ub_ctx* unboundContext;
#ifdef _WIN32
    /// windows needs to run as blocking in another thread to work at all becuase LOL windows
    std::unique_ptr<std::thread> runnerThread;
#endif

    std::atomic<bool> started;

    llarp_ev_loop_ptr eventLoop;
    ReplyFunction replyFunc;
    FailFunction failFunc;

    void
    Reset();

    void
    DeregisterPollFD();
    void
    RegisterPollFD();

   public:
    UnboundResolver(llarp_ev_loop_ptr eventLoop, ReplyFunction replyFunc, FailFunction failFunc);

    static void
    Callback(void* data, int err, ub_result* result);

    // upstream resolver IP can be IPv4 or IPv6
    bool
    Init();

    bool
    AddUpstreamResolver(const std::string& upstreamResolverIP);

    void
    Lookup(const SockAddr& source, Message msg);
  };

}  // namespace llarp::dns
