#include "server.hpp"
#include "dns.hpp"
#include <llarp/crypto/crypto.hpp>
#include <array>
#include <utility>
#include <llarp/ev/udp_handle.hpp>

namespace llarp::dns
{
  PacketHandler::PacketHandler(EventLoop_ptr loop, IQueryHandler* h)
      : m_QueryHandler{h}, m_Loop{std::move(loop)}
  {}

  Proxy::Proxy(EventLoop_ptr loop, IQueryHandler* h)
      : PacketHandler{loop, h}, m_Loop(std::move(loop))
  {
    m_Server = m_Loop->make_udp(
        [this](UDPHandle&, SockAddr a, OwnedBuffer buf) { HandlePacket(a, a, buf); });
  }

  void
  PacketHandler::Stop()
  {
    if (m_UnboundResolver)
      m_UnboundResolver->Stop();
  }

  bool
  Proxy::Start(SockAddr addr, std::vector<SockAddr> resolvers, std::vector<fs::path> hostfiles)
  {
    if (not PacketHandler::Start(addr, std::move(resolvers), std::move(hostfiles)))
      return false;
    return m_Server->listen(addr);
  }

  void
  PacketHandler::Restart()
  {
    if (m_UnboundResolver)
    {
      LogInfo("reset libunbound's internal stuff");
      m_UnboundResolver->Init();
    }
  }

  bool
  PacketHandler::Start(SockAddr, std::vector<SockAddr> resolvers, std::vector<fs::path> hostfiles)
  {
    return SetupUnboundResolver(std::move(resolvers), std::move(hostfiles));
  }

  bool
  PacketHandler::SetupUnboundResolver(
      std::vector<SockAddr> resolvers, std::vector<fs::path> hostfiles)
  {
    // if we have no resolvers don't set up unbound
    if (resolvers.empty())
      return true;

    auto failFunc = [self = weak_from_this()](
                        const SockAddr& to, const SockAddr& from, Message msg) {
      if (auto this_ptr = self.lock())
        this_ptr->SendServerMessageBufferTo(to, from, msg.ToBuffer());
    };

    auto replyFunc = [self = weak_from_this()](auto&&... args) {
      if (auto this_ptr = self.lock())
        this_ptr->SendServerMessageBufferTo(std::forward<decltype(args)>(args)...);
    };

    m_UnboundResolver =
        std::make_shared<UnboundResolver>(m_Loop, std::move(replyFunc), std::move(failFunc));
    m_Resolvers.clear();
    if (not m_UnboundResolver->Init())
    {
      llarp::LogError("Failed to initialize upstream DNS resolver.");
      m_UnboundResolver = nullptr;
      return false;
    }
    for (const auto& resolver : resolvers)
    {
      if (not m_UnboundResolver->AddUpstreamResolver(resolver))
      {
        llarp::LogError("Failed to add upstream DNS server: ", resolver);
        m_UnboundResolver = nullptr;
        return false;
      }
      m_Resolvers.emplace(resolver);
    }
    for (const auto& path : hostfiles)
    {
      m_UnboundResolver->AddHostsFile(path);
    }

    return true;
  }

  void
  Proxy::SendServerMessageBufferTo(
      const SockAddr& to, [[maybe_unused]] const SockAddr& from, llarp_buffer_t buf)
  {
    if (!m_Server->send(to, buf))
      llarp::LogError("dns reply failed");
  }

  bool
  PacketHandler::IsUpstreamResolver(const SockAddr& to, [[maybe_unused]] const SockAddr& from) const
  {
    return m_Resolvers.count(to);
  }

  bool
  PacketHandler::ShouldHandlePacket(
      const SockAddr& to, const SockAddr& from, llarp_buffer_t buf) const
  {
    MessageHeader hdr;
    if (not hdr.Decode(&buf))
    {
      return false;
    }

    Message msg{hdr};
    if (not msg.Decode(&buf))
    {
      return false;
    }

    if (m_QueryHandler and m_QueryHandler->ShouldHookDNSMessage(msg))
      return true;
      // If this request is going to an upstream resolver then we want to let it through (i.e. don't
      // handle it), and so want to return false.  If we have something else then we want to
      // intercept it to route it through our caching libunbound server (which then redirects the
      // request to the lokinet-configured upstream, if not cached).
#ifdef ANDROID
    LogError("android dns ", to);
    return IsUpstreamResolver(to, from);
#else
    return !IsUpstreamResolver(to, from);
#endif
  }

  void
  PacketHandler::HandlePacket(const SockAddr& resolver, const SockAddr& from, llarp_buffer_t buf)
  {
    MessageHeader hdr;
    if (not hdr.Decode(&buf))
    {
      llarp::LogWarn("failed to parse dns header from ", from);
      return;
    }

    Message msg(hdr);
    if (not msg.Decode(&buf))
    {
      llarp::LogWarn("failed to parse dns message from ", from);
      return;
    }

    // we don't provide a DoH resolver because it requires verified TLS
    // TLS needs X509/ASN.1-DER and opting into the Root CA Cabal
    // thankfully mozilla added a backdoor that allows ISPs to turn it off
    // so we disable DoH for firefox using mozilla's ISP backdoor
    // see: https://github.com/loki-project/loki-network/issues/832
    for (const auto& q : msg.questions)
    {
      // is this firefox looking for their backdoor record?
      if (q.IsName("use-application-dns.net"))
      {
        // yea it is, let's turn off DoH because god is dead.
        msg.AddNXReply();
        // press F to pay respects
        SendServerMessageBufferTo(from, resolver, msg.ToBuffer());
        return;
      }
    }

    if (m_QueryHandler && m_QueryHandler->ShouldHookDNSMessage(msg))
    {
      auto reply = [self = shared_from_this(), to = from, resolver](dns::Message msg) {
        self->SendServerMessageBufferTo(to, resolver, msg.ToBuffer());
      };
      if (!m_QueryHandler->HandleHookedDNSMessage(std::move(msg), reply))
      {
        llarp::LogWarn("failed to handle hooked dns");
      }
    }
    else if (not m_UnboundResolver)
    {
      // no upstream resolvers
      // let's serv fail it
      msg.AddServFail();
      SendServerMessageBufferTo(from, resolver, msg.ToBuffer());
    }
    else
    {
      m_UnboundResolver->Lookup(resolver, from, std::move(msg));
    }
  }
}  // namespace llarp::dns
