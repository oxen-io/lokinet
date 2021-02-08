#include <dns/server.hpp>
#include <dns/dns.hpp>
#include <crypto/crypto.hpp>
#include <util/thread/logic.hpp>
#include <array>
#include <utility>

namespace llarp::dns
{
  static std::vector<byte_t>
  MessageToBuffer(Message msg)
  {
    std::array<byte_t, 1500> tmp = {{0}};
    llarp_buffer_t buf(tmp);
    if (not msg.Encode(&buf))
      throw std::runtime_error("cannot encode dns message");

    buf.sz = buf.cur - buf.base;
    buf.cur = buf.base;
    std::vector<byte_t> pkt;
    pkt.resize(buf.sz);
    std::copy_n(tmp.data(), pkt.size(), pkt.data());
    return pkt;
  }
  PacketHandler::PacketHandler(Logic_ptr logic, IQueryHandler* h)
      : m_QueryHandler{h}, m_Logic{logic}
  {}

  Proxy::Proxy(llarp_ev_loop_ptr loop, Logic_ptr logic, IQueryHandler* h)
      : PacketHandler{logic, h}, m_Loop(std::move(loop))
  {
    m_Server.user = this;
    m_Server.tick = nullptr;
    m_Server.recvfrom = &HandleUDP;
  }

  void
  PacketHandler::Stop()
  {
    if (m_UnboundResolver)
      m_UnboundResolver->Stop();
  }

  bool
  Proxy::Start(SockAddr addr, std::vector<IpAddress> resolvers)
  {
    if (not PacketHandler::Start(addr, std::move(resolvers)))
      return false;
    return (llarp_ev_add_udp(m_Loop, &m_Server, addr) == 0);
  }

  static Proxy::Buffer_t
  CopyBuffer(const llarp_buffer_t& buf)
  {
    std::vector<byte_t> msgbuf(buf.sz);
    std::copy_n(buf.base, buf.sz, msgbuf.data());
    return msgbuf;
  }

  void
  Proxy::HandleUDP(llarp_udp_io* u, const SockAddr& from, ManagedBuffer buf)
  {
    Buffer_t msgbuf = CopyBuffer(buf.underlying);
    auto self = static_cast<Proxy*>(u->user);
    self->HandlePacket(from, std::move(msgbuf));
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
  PacketHandler::Start(SockAddr, std::vector<IpAddress> resolvers)
  {
    return SetupUnboundResolver(std::move(resolvers));
  }

  bool
  PacketHandler::SetupUnboundResolver(std::vector<IpAddress> resolvers)
  {
    auto failFunc = [self = weak_from_this()](SockAddr to, Message msg) {
      auto this_ptr = self.lock();
      if (this_ptr)
      {
        this_ptr->SendServerMessageBufferTo(to, MessageToBuffer(std::move(msg)));
      }
    };

    auto replyFunc = [self = weak_from_this()](SockAddr to, std::vector<byte_t> buf) {
      auto this_ptr = self.lock();
      if (this_ptr)
      {
        this_ptr->HandleUpstreamResponse(to, std::move(buf));
      }
    };

    m_UnboundResolver =
        std::make_shared<UnboundResolver>(m_Logic, std::move(replyFunc), std::move(failFunc));
    if (not m_UnboundResolver->Init())
    {
      llarp::LogError("Failed to initialize upstream DNS resolver.");
      m_UnboundResolver = nullptr;
      return false;
    }
    for (const auto& resolver : resolvers)
    {
      if (not m_UnboundResolver->AddUpstreamResolver(resolver.toHost()))
      {
        llarp::LogError("Failed to add upstream DNS server: ", resolver.toHost());
        m_UnboundResolver = nullptr;
        return false;
      }
    }

    return true;
  }

  void
  Proxy::SendServerMessageBufferTo(SockAddr to, Buffer_t buf)
  {
    if (llarp_ev_udp_sendto(&m_Server, to, buf) < 0)
      llarp::LogError("dns reply failed");
  }

  void
  PacketHandler::HandleUpstreamResponse(SockAddr to, std::vector<byte_t> buf)
  {
    LogicCall(m_Logic, [to, buffer = std::move(buf), self = shared_from_this()]() {
      self->SendServerMessageBufferTo(to, std::move(buffer));
    });
  }

  void
  PacketHandler::HandlePacket(SockAddr from, Buffer_t buf)
  {
    MessageHeader hdr;
    llarp_buffer_t pkt(buf);
    if (!hdr.Decode(&pkt))
    {
      llarp::LogWarn("failed to parse dns header from ", from);
      return;
    }

    Message msg(hdr);
    if (!msg.Decode(&pkt))
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
        SendServerMessageBufferTo(from, MessageToBuffer(std::move(msg)));
        return;
      }
    }

    if (m_QueryHandler && m_QueryHandler->ShouldHookDNSMessage(msg))
    {
      auto reply = [self = shared_from_this(), to = from](dns::Message msg) {
        self->SendServerMessageBufferTo(to, MessageToBuffer(std::move(msg)));
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
      SendServerMessageBufferTo(from, MessageToBuffer(std::move(msg)));
    }
    else
    {
      m_UnboundResolver->Lookup(from, std::move(msg));
    }
  }
}  // namespace llarp::dns
