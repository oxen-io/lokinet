#include <iwp/linklayer.hpp>

namespace llarp
{
  namespace iwp
  {
    LinkLayer::LinkLayer(const SecretKey& enckey, GetRCFunc getrc,
                         LinkMessageHandler h, SessionEstablishedHandler est,
                         SessionRenegotiateHandler reneg, SignBufferFunc sign,
                         TimeoutHandler t, SessionClosedHandler closed)
        : ILinkLayer(enckey, getrc, h, sign, est, reneg, t, closed)
    {
      m_FlowCookie.Randomize();
    }

    LinkLayer::~LinkLayer() = default;

    void
    LinkLayer::Pump()
    {
      ILinkLayer::Pump();
    }

    const char*
    LinkLayer::Name() const
    {
      return "iwp";
    }

    bool
    LinkLayer::KeyGen(SecretKey& k)
    {
      k.Zero();
      CryptoManager::instance()->encryption_keygen(k);
      return !k.IsZero();
    }

    uint16_t
    LinkLayer::Rank() const
    {
      return 2;
    }

    bool
    LinkLayer::Start(std::shared_ptr< Logic > l)
    {
      if(!ILinkLayer::Start(l))
        return false;
      return false;
    }

    void
    LinkLayer::RecvFrom(const Addr& from, const void* pkt, size_t sz)
    {
      m_OuterMsg.Clear();
      llarp_buffer_t sigbuf(pkt, sz);
      llarp_buffer_t decodebuf(pkt, sz);
      if(!m_OuterMsg.Decode(&decodebuf))
      {
        LogError("failed to decode outer message");
        return;
      }
      NetID ourNetID;
      switch(m_OuterMsg.command)
      {
        case eOCMD_ObtainFlowID:
          sigbuf.sz -= m_OuterMsg.Zsig.size();
          if(!CryptoManager::instance()->verify(m_OuterMsg.pubkey, sigbuf,
                                                m_OuterMsg.Zsig))
          {
            LogError("failed to verify signature on '",
                     (char)m_OuterMsg.command, "' message from ", from);
            return;
          }
          if(!ShouldSendFlowID(from))
          {
            SendReject(from, "no flo 4u :^)");
            return;
          }
          if(m_OuterMsg.netid == ourNetID)
          {
            if(GenFlowIDFor(m_OuterMsg.pubkey, from, m_OuterMsg.flow))
              SendFlowID(from, m_OuterMsg.flow);
            else
              SendReject(from, "genflow fail");
          }
          else
            SendReject(from, "bad netid");
      }
    }

    std::shared_ptr< ILinkSession >
    LinkLayer::NewOutboundSession(const RouterContact& rc,
                                  const AddressInfo& ai)
    {
      (void)rc;
      (void)ai;
      // TODO: implement me
      return {};
    }

    void
    LinkLayer::SendFlowID(const Addr& to, const FlowID_t& flow)
    {
      // TODO: implement me
      (void)to;
      (void)flow;
    }

    bool
    LinkLayer::VerifyFlowID(const PubKey& pk, const Addr& from,
                            const FlowID_t& flow) const
    {
      FlowID_t expected;
      if(!GenFlowIDFor(pk, from, expected))
        return false;
      return expected == flow;
    }

    bool
    LinkLayer::GenFlowIDFor(const PubKey& pk, const Addr& from,
                            FlowID_t& flow) const
    {
      std::array< byte_t, 128 > tmp = {{0}};
      if(inet_ntop(AF_INET6, from.addr6(), (char*)tmp.data(), tmp.size())
         == nullptr)
        return false;
      std::copy_n(pk.begin(), pk.size(), tmp.begin() + 64);
      std::copy_n(m_FlowCookie.begin(), m_FlowCookie.size(),
                  tmp.begin() + 64 + pk.size());
      llarp_buffer_t buf(tmp);
      ShortHash h;
      if(!CryptoManager::instance()->shorthash(h, buf))
        return false;
      std::copy_n(h.begin(), flow.size(), flow.begin());
      return true;
    }

    bool
    LinkLayer::ShouldSendFlowID(const Addr& to) const
    {
      (void)to;
      // TODO: implement me
      return false;
    }

    void
    LinkLayer::SendReject(const Addr& to, const char* msg)
    {
      if(strlen(msg) > 14)
      {
        throw std::logic_error("reject message too big");
      }
      std::array< byte_t, 120 > pkt;
      auto now  = Now();
      PubKey pk = GetOurRC().pubkey;
      OuterMessage m;
      m.CreateReject(msg, now, pk);
      llarp_buffer_t encodebuf(pkt);
      if(!m.Encode(&encodebuf))
      {
        LogError("failed to encode reject message to ", to);
        return;
      }
      llarp_buffer_t signbuf(pkt.data(), pkt.size() - m.Zsig.size());
      if(!Sign(m.Zsig, signbuf))
      {
        LogError("failed to sign reject messsage to ", to);
        return;
      }
      std::copy_n(m.Zsig.begin(), m.Zsig.size(),
                  pkt.begin() + (pkt.size() - m.Zsig.size()));
      llarp_buffer_t pktbuf(pkt);
      SendTo_LL(to, pktbuf);
    }
  }  // namespace iwp

}  // namespace llarp
