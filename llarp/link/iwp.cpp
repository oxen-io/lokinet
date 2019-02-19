#include <link/iwp.hpp>
#include <link/iwp_internal.hpp>
#include <router/abstractrouter.hpp>

namespace llarp
{
  namespace iwp
  {
    void
    OuterMessage::Clear()
    {
      command = 0;
      flow.Zero();
      netid.Zero();
      nextFlowID.Zero();
      rejectReason.clear();
      N.Zero();
      X.Zero();
      Xsize = 0;
      Z.Zero();
    }

    bool
    OuterMessage::Decode(llarp_buffer_t* buf)
    {
      if(buf->size_left() < 34)
        return false;
      command = *buf->cur;
      ++buf->cur;
      if(*buf->cur != '=')
        return false;
      std::copy_n(flow.begin(), 32, buf->cur);
      buf->cur += 32;
      switch(command)
      {
        case eOCMD_ObtainFlowID:
          if(buf->size_left() < 40)
            return false;
          buf->cur += 32;
          std::copy_n(netid.begin(), 8, buf->cur);
          return true;
        case eOCMD_GiveFlowID:
          if(buf->size_left() < 32)
            return false;
          std::copy_n(nextFlowID.begin(), 32, buf->cur);
          return true;
        case eOCMD_Reject:
          rejectReason = std::string(buf->cur, buf->base + buf->sz);
          return true;
        case eOCMD_SessionNegotiate:
          // explicit fallthrough
        case eOCMD_TransmitData:
          if(buf->size_left() <= 56)
            return false;
          std::copy_n(N.begin(), N.size(), buf->cur);
          buf->cur += N.size();
          Xsize = buf->size_left() - Z.size();
          if(Xsize > X.size())
            return false;
          std::copy_n(X.begin(), Xsize, buf->cur);
          buf->cur += Xsize;
          std::copy_n(Z.begin(), Z.size(), buf->cur);
          return true;

        default:
          return false;
      }
    }

    LinkLayer::LinkLayer(Crypto* c, const SecretKey& enckey, GetRCFunc getrc,
                         LinkMessageHandler h, SessionEstablishedHandler est,
                         SessionRenegotiateHandler reneg, SignBufferFunc sign,
                         TimeoutHandler t, SessionClosedHandler closed)
        : ILinkLayer(enckey, getrc, h, sign, est, reneg, t, closed), crypto(c)
    {
    }

    LinkLayer::~LinkLayer()
    {
    }

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
      crypto->encryption_keygen(k);
      return !k.IsZero();
    }

    uint16_t
    LinkLayer::Rank() const
    {
      return 2;
    }

    bool
    LinkLayer::Start(Logic* l)
    {
      if(!ILinkLayer::Start(l))
        return false;
      /// TODO: change me to true when done
      return false;
    }

    void
    LinkLayer::RecvFrom(const Addr& from, const void* pkt, size_t sz)
    {
      m_OuterMsg.Clear();
      llarp_buffer_t buf(pkt, sz);
      if(!m_OuterMsg.Decode(&buf))
      {
        LogError("failed to decode outer message");
        return;
      }
      NetID ourNetID;
      switch(m_OuterMsg.command)
      {
        case eOCMD_ObtainFlowID:
          if(!ShouldSendFlowID(from))
            return;  // drop

          if(m_OuterMsg.netid == ourNetID)
            SendFlowID(from, m_OuterMsg.flow);
          else
            SendReject(from, m_OuterMsg.flow, "bad net id");
      }
    }

    ILinkSession*
    LinkLayer::NewOutboundSession(const RouterContact& rc,
                                  const AddressInfo& ai)
    {
      (void)rc;
      (void)ai;
      // TODO: implement me
      return nullptr;
    }

    void
    LinkLayer::SendFlowID(const Addr& to, const FlowID_t& flow)
    {
      // TODO: implement me
      (void)to;
      (void)flow;
    }

    bool
    LinkLayer::ShouldSendFlowID(const Addr& to) const
    {
      (void)to;
      // TODO: implement me
      return false;
    }

    void
    LinkLayer::SendReject(const Addr& to, const FlowID_t& flow, const char* msg)
    {
      // TODO: implement me
      (void)to;
      (void)flow;
      (void)msg;
    }

    std::unique_ptr< ILinkLayer >
    NewServerFromRouter(AbstractRouter* r)
    {
      using namespace std::placeholders;
      return NewServer(
          r->crypto(), r->encryption(), std::bind(&AbstractRouter::rc, r),
          std::bind(&AbstractRouter::HandleRecvLinkMessageBuffer, r, _1, _2),
          std::bind(&AbstractRouter::OnSessionEstablished, r, _1),
          std::bind(&AbstractRouter::CheckRenegotiateValid, r, _1, _2),
          std::bind(&AbstractRouter::Sign, r, _1, _2),
          std::bind(&AbstractRouter::OnConnectTimeout, r, _1),
          std::bind(&AbstractRouter::SessionClosed, r, _1));
    }

    std::unique_ptr< ILinkLayer >
    NewServer(Crypto* c, const SecretKey& enckey, GetRCFunc getrc,
              LinkMessageHandler h, SessionEstablishedHandler est,
              SessionRenegotiateHandler reneg, SignBufferFunc sign,
              TimeoutHandler t, SessionClosedHandler closed)
    {
      (void)c;
      (void)enckey;
      (void)getrc;
      (void)h;
      (void)est;
      (void)reneg;
      (void)sign;
      (void)t;
      (void)closed;
      // TODO: implement me
      return nullptr;
    }
  }  // namespace iwp
}  // namespace llarp
