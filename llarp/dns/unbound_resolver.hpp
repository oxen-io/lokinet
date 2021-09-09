#pragma once

#include <mutex>
#include <atomic>
#include <memory>
#include <queue>

#include <llarp/ev/ev.hpp>
#include <llarp/util/fs.hpp>

#include "message.hpp"

#ifdef _WIN32
#include <thread>
#else
#include <uvw.hpp>
#endif

extern "C"
{
  struct ub_ctx;
  struct ub_result;
}

namespace llarp::dns
{
  using ReplyFunction =
      std::function<void(const SockAddr& reply_to, const SockAddr& from_resolver, OwnedBuffer buf)>;
  using FailFunction =
      std::function<void(const SockAddr& reply_to, const SockAddr& from_resolver, Message msg)>;

  class UnboundResolver : public std::enable_shared_from_this<UnboundResolver>
  {
   private:
    ub_ctx* unboundContext;

    std::atomic<bool> started;

#ifdef _WIN32
    std::thread runner;
#else
    std::weak_ptr<uvw::Loop> loop;
    std::shared_ptr<uvw::PollHandle> udp;
#endif

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
