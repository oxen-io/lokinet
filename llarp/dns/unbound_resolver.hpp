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

    std::atomic<bool> started;
    std::unique_ptr<std::thread> runner;

    ReplyFunction replyFunc;
    FailFunction failFunc;

    void
    Reset();

   public:
    UnboundResolver(std::shared_ptr<Logic> logic, ReplyFunction replyFunc, FailFunction failFunc);

    static void
    Callback(void* data, int err, ub_result* result);

    // stop resolver thread
    void
    Stop();

    // upstream resolver IP can be IPv4 or IPv6
    bool
    Init();

    bool
    AddUpstreamResolver(const std::string& upstreamResolverIP);

    void
    Lookup(const SockAddr& source, Message msg);
  };

}  // namespace llarp::dns
