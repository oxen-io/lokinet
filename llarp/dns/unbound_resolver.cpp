#include <dns/unbound_resolver.hpp>

#include <dns/server.hpp>
#include <util/buffer.hpp>

namespace llarp::dns
{
  struct PendingUnboundLookup
  {
    std::weak_ptr<UnboundResolver> resolver;
    Message msg;
    SockAddr source;
  };

  void UnboundResolver::Reset()
  {
    started = false;
    if (unboundContext)
    {
      DeregisterPollFD();
      ub_ctx_delete(unboundContext);
    }
    unboundContext = nullptr;
  }

  void UnboundResolver::DeregisterPollFD()
  {
    eventLoop->deregister_poll_fd_readable(ub_fd(unboundContext));
  }

  void UnboundResolver::RegisterPollFD()
  {
    eventLoop->register_poll_fd_readable(ub_fd(unboundContext), [=](){ ub_process(unboundContext); });
  }

  UnboundResolver::UnboundResolver(
      llarp_ev_loop_ptr eventLoop,
      ReplyFunction replyFunc)
    : unboundContext(nullptr), started(false), eventLoop(eventLoop), replyFunc(replyFunc)
  {
  }

  // static callback
  void UnboundResolver::Callback(void* data, int err, ub_result* result)
  {
    std::unique_ptr<PendingUnboundLookup> lookup{static_cast<PendingUnboundLookup*>(data)};

    auto this_ptr = lookup->resolver.lock();
    if (not this_ptr) return; // resolver is gone, so we don't reply.

    if (err != 0)
    {
      Message& msg = lookup->msg;
      msg.AddServFail();
      this_ptr->replyFunc(lookup->source, msg);
      ub_resolve_free(result);
      return;
    }

    llarp_buffer_t buf;
    buf.base = buf.cur = static_cast<byte_t*>(result->answer_packet);
    buf.sz = result->answer_len;

    MessageHeader hdr;
    hdr.Decode(&buf);
    hdr.id = lookup->msg.hdr_id;

    Message msg(hdr);
    msg.Decode(&buf);

    this_ptr->replyFunc(lookup->source, msg);

    ub_resolve_free(result);
  }

  bool UnboundResolver::Init()
  {
    if (started)
    {
      Reset();
    }

    unboundContext = ub_ctx_create();

    if (not unboundContext)
    {
      return false;
    }

    RegisterPollFD();

    return true;
  }

  bool UnboundResolver::AddUpstreamResolver(const std::string& upstreamResolverIP)
  {
    if (ub_ctx_set_fwd(unboundContext, upstreamResolverIP.c_str()) != 0)
    {
      Reset();
      return false;
    }
    return true;
  }

  void UnboundResolver::Lookup(const SockAddr& source, Message& msg)
  {
    if (not unboundContext)
    {
      msg.AddServFail();
      replyFunc(source, msg);
      return;
    }

    started = true;

    const auto& q = msg.questions[0];
    auto* lookup = new PendingUnboundLookup{weak_from_this(), msg, source};
    int err = ub_resolve_async(unboundContext, q.Name().c_str(), q.qtype, q.qclass, (void*)lookup, &UnboundResolver::Callback, nullptr);

    if (err != 0)
    {
      msg.AddServFail();
      replyFunc(source, msg);
      return;
    }
  }

} // namespace llarp::dns
