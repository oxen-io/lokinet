#include "unbound_resolver.hpp"

#include "server.hpp"
#include <llarp/util/buffer.hpp>
#include <sstream>
#include <llarp/util/str.hpp>

namespace llarp::dns
{
  struct PendingUnboundLookup
  {
    std::weak_ptr<UnboundResolver> resolver;
    Message msg;
    SockAddr resolverAddr;
    SockAddr askerAddr;
    int id;
  };

  void
  UnboundResolver::Stop()
  {
    Reset();
  }

  void
  UnboundResolver::Reset()
  {
    if (runner)
    {
      // cancel all pending lookups
      for (auto id : pending_resolve_jobs)
      {
        if (unboundContext)
          ub_cancel(unboundContext, id);
      }

      pending_resolve_jobs.clear();
      started = false;

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

    // remove from pending jobs
    this_ptr->pending_resolve_jobs.erase(lookup->id);

    if (err != 0)
    {
      Message& msg = lookup->msg;
      msg.AddServFail();
      this_ptr->failFunc(lookup->resolverAddr, lookup->askerAddr, msg);
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

    this_ptr->replyFunc(lookup->resolverAddr, lookup->askerAddr, std::move(pkt));

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
    started = true;
    runner = std::make_unique<std::thread>([&]() {
      while (started)
      {
        if (unboundContext)
          ub_wait(unboundContext);
        std::this_thread::sleep_for(25ms);
      }
    });
    return true;
  }

  bool
  UnboundResolver::AddUpstreamResolver(const SockAddr& upstreamResolver)
  {
    std::stringstream ss;
    ss << upstreamResolver.hostString();

    if (const auto port = upstreamResolver.getPort(); port != 53)
      ss << "@" << port;

    const auto str = ss.str();
    if (ub_ctx_set_fwd(unboundContext, str.c_str()) != 0)
    {
      Reset();
      return false;
    }
    return true;
  }

  void
  UnboundResolver::AddHostsFile(const fs::path& file)
  {
    LogDebug("adding hosts file ", file);
    const auto str = file.u8string();
    if (auto ret = ub_ctx_hosts(unboundContext, str.c_str()))
    {
      throw std::runtime_error{stringify("Failed to add host file ", file, ": ", ub_strerror(ret))};
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
      failFunc(to, from, std::move(msg));
      return;
    }
    const auto& q = msg.questions[0];
    auto* lookup = new PendingUnboundLookup{weak_from_this(), msg, to, from, 0};
    int err = ub_resolve_async(
        unboundContext,
        q.Name().c_str(),
        q.qtype,
        q.qclass,
        (void*)lookup,
        &UnboundResolver::Callback,
        &lookup->id);

    if (err != 0)
    {
      msg.AddServFail();
      failFunc(to, from, std::move(msg));
      return;
    }
    pending_resolve_jobs.emplace(lookup->id);
  }

}  // namespace llarp::dns
