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
    Proxy::Proxy(
        llarp_ev_loop_ptr serverLoop,
        Logic_ptr serverLogic,
        llarp_ev_loop_ptr clientLoop,
        Logic_ptr clientLogic,
        IQueryHandler* h)
        : m_ServerLoop(std::move(serverLoop))
        , m_ClientLoop(std::move(clientLoop))
        , m_ServerLogic(std::move(serverLogic))
        , m_ClientLogic(std::move(clientLogic))
        , m_QueryHandler(h)
    {
      m_Client.user = this;
      m_Server.user = this;
      m_Client.tick = nullptr;
      m_Server.tick = nullptr;
      m_Client.recvfrom = &HandleUDPRecv_client;
      m_Server.recvfrom = &HandleUDPRecv_server;
    }

    void
    Proxy::Stop()
    {}

    bool
    Proxy::Start(const IpAddress& addr, const std::vector<IpAddress>& resolvers)
    {
      if (resolvers.size())
      {
        if (not SetupUnboundResolver(resolvers))
        {
          return false;
        }
      }

      const IpAddress any("0.0.0.0", 0);
      auto self = shared_from_this();
      LogicCall(m_ClientLogic, [=]() {
        llarp_ev_add_udp(self->m_ClientLoop.get(), &self->m_Client, any.createSockAddr());
      });
      LogicCall(m_ServerLogic, [=]() {
        llarp_ev_add_udp(self->m_ServerLoop.get(), &self->m_Server, addr.createSockAddr());
      });
      return true;
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
      auto self = static_cast<Proxy*>(u->user)->shared_from_this();
      // yes we use the server loop here because if the server loop is not the
      // client loop we'll crash again
      LogicCall(
          self->m_ServerLogic, [self, from, msgbuf]() { self->HandlePktServer(from, msgbuf); });
    }

    void
    Proxy::HandleUDPRecv_client(llarp_udp_io* u, const SockAddr& from, ManagedBuffer buf)
    {
      Buffer_t msgbuf = CopyBuffer(buf.underlying);
      auto self = static_cast<Proxy*>(u->user)->shared_from_this();
      LogicCall(
          self->m_ServerLogic, [self, from, msgbuf]() { self->HandlePktClient(from, msgbuf); });
    }

    IpAddress
    Proxy::PickRandomResolver() const
    {
      const size_t sz = m_Resolvers.size();
      if (sz <= 1)
        return m_Resolvers[0];
      auto itr = m_Resolvers.begin();
      std::advance(itr, llarp::randint() % sz);
      return *itr;
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
          this_ptr->HandleUpstreamResponse(to, std::move(buf));
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

      return true;
    }

    void
    Proxy::HandleTick(llarp_udp_io*)
    {}

    void
    Proxy::SendServerMessageBufferTo(const SockAddr& to, const llarp_buffer_t& buf)
    {
      llarp_ev_udp_sendto(&m_Server, to, buf);
    }

    void
    Proxy::SendServerMessageTo(const SockAddr& to, Message msg)
    {
      auto self = shared_from_this();
      LogicCall(m_ServerLogic, [to, msg = std::move(msg), self]() {
        std::array<byte_t, 1500> tmp = {{0}};
        llarp_buffer_t buf(tmp);
        if (msg.Encode(&buf))
        {
          buf.sz = buf.cur - buf.base;
          buf.cur = buf.base;
          self->SendServerMessageBufferTo(to, buf);
        }
        else
          llarp::LogWarn("failed to encode dns message when sending");
      });
    }

    void
    Proxy::HandleUpstreamResponse(SockAddr to, std::vector<byte_t> buf)
    {
      auto self = shared_from_this();
      LogicCall(m_ServerLogic, [to, buffer = std::move(buf), self]() {
        llarp_buffer_t buf(buffer);
        self->SendServerMessageBufferTo(to, buf);
      });
    }

    void
    Proxy::SendClientMessageTo(const SockAddr& to, Message msg)
    {
      auto self = shared_from_this();
      LogicCall(m_ClientLogic, [to, msg, self]() {
        std::array<byte_t, 1500> tmp = {{0}};
        llarp_buffer_t buf(tmp);
        if (msg.Encode(&buf))
        {
          buf.sz = buf.cur - buf.base;
          buf.cur = buf.base;
          llarp_ev_udp_sendto(&self->m_Client, to, buf);
        }
        else
          llarp::LogWarn("failed to encode dns message when sending");
      });
    }

    void
    Proxy::HandlePktClient(const SockAddr& from, Buffer_t buf)
    {
      llarp_buffer_t pkt(buf);
      MessageHeader hdr;
      if (!hdr.Decode(&pkt))
      {
        llarp::LogWarn("failed to parse dns header from ", from);
        return;
      }
      TX tx = {hdr.id, from};
      auto itr = m_Forwarded.find(tx);
      if (itr == m_Forwarded.end())
        return;
      const auto& requester = itr->second;
      auto self = shared_from_this();
      Message msg(hdr);
      if (msg.Decode(&pkt))
      {
        if (m_QueryHandler && m_QueryHandler->ShouldHookDNSMessage(msg))
        {
          msg.hdr_id = itr->first.txid;
          if (!m_QueryHandler->HandleHookedDNSMessage(
                  std::move(msg),
                  std::bind(
                      &Proxy::SendServerMessageTo,
                      self,
                      requester.createSockAddr(),
                      std::placeholders::_1)))
          {
            llarp::LogWarn("failed to handle hooked dns");
          }
          return;
        }
      }
      LogicCall(m_ServerLogic, [=]() {
        // forward reply to requester via server
        const llarp_buffer_t tmpbuf(buf);
        llarp_ev_udp_sendto(&self->m_Server, requester.createSockAddr(), tmpbuf);
      });
      // remove pending
      m_Forwarded.erase(itr);
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

      TX tx = {hdr.id, from};
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

      auto self = shared_from_this();
      if (m_QueryHandler && m_QueryHandler->ShouldHookDNSMessage(msg))
      {
        if (!m_QueryHandler->HandleHookedDNSMessage(
                std::move(msg),
                std::bind(&Proxy::SendServerMessageTo, self, from, std::placeholders::_1)))
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
