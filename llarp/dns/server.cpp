#include <dns/server.hpp>
#include <dns/dns.hpp>
#include <crypto/crypto.hpp>
#include <util/thread/logic.hpp>
#include <array>
#include <utility>

namespace llarp
{
  namespace dns
  {
    Proxy::Proxy(llarp_ev_loop_ptr serverLoop, Logic_ptr serverLogic, IQueryHandler* h)
        : m_ServerLoop(std::move(serverLoop))
        , m_ServerLogic(std::move(serverLogic))
        , m_QueryHandler(h)
    {
      m_Server.user = this;
      m_Server.tick = nullptr;
      m_Server.recvfrom = &HandleUDPRecv_server;
    }

    void
    Proxy::Stop()
    {
      if (m_UnboundResolver)
        m_UnboundResolver->Stop();
    }

    bool
    Proxy::Start(const IpAddress& addr, const std::vector<IpAddress>& resolvers)
    {
      if (resolvers.size())
      {
        if (not SetupUnboundResolver(resolvers))
        {
          llarp::LogError("Failed to add upstream resolvers during DNS server setup.");
          return false;
        }
      }

      return (llarp_ev_add_udp(m_ServerLoop.get(), &m_Server, addr.createSockAddr()) == 0);
    }

    static Proxy::Buffer_t
    CopyBuffer(const llarp_buffer_t& buf)
    {
      std::vector<byte_t> msgbuf(buf.sz);
      std::copy_n(buf.base, buf.sz, msgbuf.data());
      return msgbuf;
    }

    void
    Proxy::HandleUDPRecv_server(llarp_udp_io* u, const SockAddr& from, ManagedBuffer buf)
    {
      Buffer_t msgbuf = CopyBuffer(buf.underlying);
      auto self = static_cast<Proxy*>(u->user);
      self->HandlePktServer(from, msgbuf);
    }

    bool
    Proxy::SetupUnboundResolver(const std::vector<IpAddress>& resolvers)
    {
      auto failFunc = [self = weak_from_this()](SockAddr to, Message msg) {
        auto this_ptr = self.lock();
        if (this_ptr)
        {
          this_ptr->SendServerMessageTo(to, std::move(msg));
        }
      };

      auto replyFunc = [self = weak_from_this()](SockAddr to, std::vector<byte_t> buf) {
        auto this_ptr = self.lock();
        if (this_ptr)
        {
          this_ptr->SendServerMessageBufferTo(to, std::move(buf));
        }
      };

      m_UnboundResolver = std::make_shared<UnboundResolver>(
          m_ServerLoop, std::move(replyFunc), std::move(failFunc));
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
      m_UnboundResolver->Start();
      return true;
    }

    void
    Proxy::HandleTick(llarp_udp_io*)
    {}

    void
    Proxy::SendServerMessageBufferTo(SockAddr to, std::vector<byte_t> buf)
    {
      if (llarp_ev_udp_sendto(&m_Server, to, buf) < 0)
        llarp::LogError("dns reply failed");
    }

    void
    Proxy::SendServerMessageTo(const SockAddr& to, Message msg)
    {
      std::vector<byte_t> tmp;
      tmp.resize(1500);
      llarp_buffer_t buf{tmp};
      if (msg.Encode(&buf))
      {
        tmp.resize(buf.cur - buf.base);
        SendServerMessageBufferTo(to, std::move(tmp));
      }
      else
        llarp::LogWarn("failed to encode dns message when sending");
    }

    void
    Proxy::HandlePktServer(const SockAddr& from, Buffer_t buf)
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
          SendServerMessageTo(from, std::move(msg));
          return;
        }
      }
      if (m_QueryHandler && m_QueryHandler->ShouldHookDNSMessage(msg))
      {
        if (!m_QueryHandler->HandleHookedDNSMessage(
                std::move(msg),
                std::bind(
                    &Proxy::SendServerMessageTo, shared_from_this(), from, std::placeholders::_1)))
        {
          llarp::LogWarn("failed to handle hooked dns");
        }
      }
      else if (not m_UnboundResolver)
      {
        // no upstream resolvers
        // let's serv fail it
        msg.AddServFail();

        SendServerMessageTo(from, std::move(msg));
      }
      else
      {
        m_UnboundResolver->Lookup(from, std::move(msg));
      }
    }

  }  // namespace dns
}  // namespace llarp
