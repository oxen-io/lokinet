#include "unbound_resolver.hpp"

#include "server.hpp"
#include <llarp/util/buffer.hpp>
#include <sstream>
#include <llarp/util/str.hpp>

#include <unbound.h>

namespace llarp::dns
{
  struct PendingUnboundLookup
  {
    std::weak_ptr<UnboundResolver> resolver;
    Message msg;
    SockAddr resolverAddr;
    SockAddr askerAddr;
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
#ifdef _WIN32
    if (runner.joinable())
    {
      runner.join();
    }
#else
    if (udp)
    {
      udp->close();
    }
    udp.reset();
#endif
    if (unboundContext)
    {
      ub_ctx_delete(unboundContext);
    }
    unboundContext = nullptr;
  }

  UnboundResolver::UnboundResolver(EventLoop_ptr _loop, ReplyFunction reply, FailFunction fail)
      : unboundContext{nullptr}
      , started{false}
      , replyFunc{_loop->make_caller(std::move(reply))}
      , failFunc{_loop->make_caller(std::move(fail))}
  {
#ifndef _WIN32
    loop = _loop->MaybeGetUVWLoop();
#endif
  }

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
      this_ptr->failFunc(lookup->askerAddr, lookup->resolverAddr, msg);
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

    this_ptr->replyFunc(lookup->askerAddr, lookup->resolverAddr, std::move(pkt));

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

    // disable ip6 for upstream dns
    ub_ctx_set_option(unboundContext, "prefer-ip6", "0");
    // enable async
    ub_ctx_async(unboundContext, 1);
#ifdef _WIN32
    runner = std::thread{[&]() {
      while (started)
      {
        if (unboundContext)
          ub_wait(unboundContext);
        std::this_thread::sleep_for(25ms);
      }
      if (unboundContext)
        ub_process(unboundContext);
    }};
#else
    if (auto loop_ptr = loop.lock())
    {
      udp = loop_ptr->resource<uvw::PollHandle>(ub_fd(unboundContext));
      udp->on<uvw::PollEvent>([ptr = weak_from_this()](auto&, auto&) {
        if (auto self = ptr.lock())
        {
          if (self->unboundContext)
          {
            ub_process(self->unboundContext);
          }
        }
      });
      udp->start(uvw::PollHandle::Event::READABLE);
    }
#endif
    started = true;
    return true;
  }

  bool
  UnboundResolver::AddUpstreamResolver(const SockAddr& upstreamResolver)
  {
    std::stringstream ss;
    auto hoststr = upstreamResolver.hostString();
    ss << hoststr;

    if (const auto port = upstreamResolver.getPort(); port != 53)
      ss << "@" << port;

    const auto str = ss.str();
    if (ub_ctx_set_fwd(unboundContext, str.c_str()) != 0)
    {
      Reset();
      return false;
    }

#ifdef __APPLE__
    // On Apple, we configure a localhost resolver to trampoline requests through the tunnel to the
    // actual upstream (because the network extension itself cannot route through the tunnel using
    // normal sockets but instead we "get" to use Apple's interfaces, hurray).
    if (hoststr == "127.0.0.1")
    {
      // Not at all clear why this is needed but without it we get "send failed: Can't assign
      // requested address" when unbound tries to connect to the localhost address using a source
      // address of 0.0.0.0.  Yay apple.
      ub_ctx_set_option(unboundContext, "outgoing-interface:", hoststr.c_str());

      // The trampoline expects just a single source port (and sends everything back to it)
      ub_ctx_set_option(unboundContext, "outgoing-range:", "1");
      ub_ctx_set_option(unboundContext, "outgoing-port-avoid:", "0-65535");
      ub_ctx_set_option(unboundContext, "outgoing-port-permit:", "1253");
    }
#endif

    return true;
  }

  void
  UnboundResolver::AddHostsFile(const fs::path& file)
  {
    LogDebug("adding hosts file ", file);
    const auto str = file.u8string();
    if (auto ret = ub_ctx_hosts(unboundContext, str.c_str()))
    {
      throw std::runtime_error{
          fmt::format("Failed to add host file {}: {}", file, ub_strerror(ret))};
    }
    else
    {
      LogInfo("added hosts file ", file);
    }
  }

  void
  UnboundResolver::Lookup(SockAddr to, SockAddr from, Message msg)
  {
    if (not unboundContext)
    {
      msg.AddServFail();
      failFunc(from, to, std::move(msg));
      return;
    }

    const auto& q = msg.questions[0];
    auto* lookup = new PendingUnboundLookup{weak_from_this(), msg, to, from};
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
      failFunc(from, to, std::move(msg));
      return;
    }
  }

}  // namespace llarp::dns
