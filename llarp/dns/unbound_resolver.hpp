#pragma once

#include <unbound.h>
#include <mutex>
#include <atomic>
#include <memory>
#include <queue>

#include <ev/ev.hpp>
#include <util/thread/logic.hpp>

#include <dns/message.hpp>

namespace llarp::dns
{
  using ReplyFunction = std::function<void(const SockAddr& source, const Message& msg)>;

  class UnboundResolver : public std::enable_shared_from_this<UnboundResolver>
  {

  private:
    ub_ctx* unboundContext;

    bool started;

    llarp_ev_loop_ptr eventLoop;
    ReplyFunction replyFunc;

    void Reset();

    void DeregisterPollFD();
    void RegisterPollFD();

  public:


    UnboundResolver(
        llarp_ev_loop_ptr eventLoop,
        ReplyFunction replyFunc);

    static void Callback(void* data, int err, ub_result* result);

    // upstream resolver IP can be IPv4 or IPv6
    bool Init();

    bool AddUpstreamResolver(const std::string& upstreamResolverIP);

    void Lookup(const SockAddr& source, Message& msg);
  };

} // namespace llarp::dns

