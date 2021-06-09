#pragma once

#include <unbound.h>
#include <mutex>
#include <atomic>
#include <memory>
#include <queue>

#include <llarp/ev/ev.hpp>
#include <llarp/util/fs.hpp>

#include "message.hpp"

#ifdef _WIN32
#include <thread>
#endif

namespace llarp::dns
{
  using ReplyFunction =
      std::function<void(const SockAddr& resolver, const SockAddr& source, OwnedBuffer buf)>;
  using FailFunction =
      std::function<void(const SockAddr& resolver, const SockAddr& source, Message msg)>;

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
    UnboundResolver(EventLoop_ptr loop, ReplyFunction replyFunc, FailFunction failFunc);

    static void
    Callback(void* data, int err, ub_result* result);

    // stop resolver thread
    void
    Stop();

    // upstream resolver IP can be IPv4 or IPv6
    bool
    Init();

    bool
    AddUpstreamResolver(const SockAddr& upstreamResolverIP);

    void
    AddHostsFile(const fs::path& file);

    void
    Lookup(SockAddr to, SockAddr from, Message msg);
  };

}  // namespace llarp::dns
