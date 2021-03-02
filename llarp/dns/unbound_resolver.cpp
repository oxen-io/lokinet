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
    SockAddr replyFrom;
  };

  void
  UnboundResolver::Stop()
  {
    Reset();
  }

  void
  UnboundResolver::Reset()
  {
    started = false;
    if (runner)
    {
      runner->join();
      runner.reset();
    }
    if (unboundContext)
    {
      ub_ctx_delete(unboundContext);
    }
    unboundContext = nullptr;
  }

  UnboundResolver::UnboundResolver(EventLoop_ptr loop, ReplyFunction reply, FailFunction fail)
      : unboundContext(nullptr)
      , started(false)
      , replyFunc(loop->make_caller(std::move(reply)))
      , failFunc(loop->make_caller(std::move(fail)))
  {}

  // static callback
  void
  UnboundResolver::Callback(void* data, int err, ub_result* result)
  {
    std::unique_ptr<PendingUnboundLookup> lookup{static_cast<PendingUnboundLookup*>(data)};

    auto this_ptr = lookup->resolver.lock();
    if (not this_ptr)
      return;  // resolver is gone, so we don't reply.

    if (err != 0)
    {
      Message& msg = lookup->msg;
      msg.AddServFail();
      this_ptr->failFunc(lookup->replyFrom, lookup->source, msg);
      ub_resolve_free(result);
      return;
    }
    OwnedBuffer pkt{(size_t)result->answer_len};
    std::memcpy(pkt.buf.get(), result->answer_packet, pkt.sz);
    llarp_buffer_t buf(pkt);

    MessageHeader hdr;
    hdr.Decode(&buf);
    hdr.id = lookup->msg.hdr_id;

    buf.cur = buf.base;
    hdr.Encode(&buf);

    this_ptr->replyFunc(lookup->replyFrom, lookup->source, std::move(pkt));

    ub_resolve_free(result);
  }

  bool
  UnboundResolver::Init()
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

    ub_ctx_async(unboundContext, 1);
    runner = std::make_unique<std::thread>([&]() {
      while (started)
      {
        if (unboundContext)
          ub_wait(unboundContext);
        std::this_thread::sleep_for(25ms);
      }
    });
    started = true;
    return true;
  }

  bool
  UnboundResolver::AddUpstreamResolver(const std::string& upstreamResolverIP)
  {
    if (ub_ctx_set_fwd(unboundContext, upstreamResolverIP.c_str()) != 0)
    {
      Reset();
      return false;
    }
    return true;
  }

  void
  UnboundResolver::Lookup(SockAddr to, SockAddr from, Message msg)
  {
    if (not unboundContext)
    {
      msg.AddServFail();
      failFunc(to, from, std::move(msg));
      return;
    }

    const auto& q = msg.questions[0];
    auto* lookup = new PendingUnboundLookup{weak_from_this(), msg, from, to};
    int err = ub_resolve_async(
        unboundContext,
        q.Name().c_str(),
        q.qtype,
        q.qclass,
        (void*)lookup,
        &UnboundResolver::Callback,
        nullptr);

    if (err != 0)
    {
      msg.AddServFail();
      failFunc(to, from, std::move(msg));
      return;
    }
  }

}  // namespace llarp::dns
