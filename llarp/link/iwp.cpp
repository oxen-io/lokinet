#include <link/iwp.hpp>
#include <link/iwp_internal.hpp>
#include <router/abstractrouter.hpp>

namespace llarp
{
  namespace iwp
  {
    std::array< byte_t, 6 > OuterMessage::obtain_flow_id_magic =
      std::array< byte_t, 6 >{{'n', 'e', 't', 'i', 'd', '?'}};
    
    std::array< byte_t, 6 > OuterMessage::give_flow_id_magic =
      std::array< byte_t, 6 >{{'n', 'e', 't', 'i', 'd', '!'}};

    OuterMessage::OuterMessage()
    {
      Clear();
    }

    OuterMessage::~OuterMessage()
    {
    }

    void
    OuterMessage::Clear()
    {
      command = 0;
      flow.Zero();
      netid.Zero();
      reject.fill(0);
      N.Zero();
      X.Zero();
      Xsize = 0;
      Zsig.Zero();
      Zhash.Zero();
      pubkey.Zero();
      magic.fill(0);
      uinteger = 0;
      A.reset();
    }

    void
    OuterMessage::CreateReject(const char* msg, llarp_time_t now,
                               const PubKey& pk)
    {
      Clear();
      std::copy_n(msg, std::min(strlen(msg), reject.size()), reject.begin());
      uinteger = now;
      pubkey   = pk;
    }

    bool
    OuterMessage::Encode(llarp_buffer_t* buf) const
    {
      if(buf->size_left() < 2)
        return false;
      *buf->cur = command;
      buf->cur++;
      *buf->cur = '=';
      buf->cur++;
      switch(command)
      {
        case eOCMD_ObtainFlowID:

        case eOCMD_GiveFlowID:
          if(!buf->write(reject.begin(), reject.end()))
            return false;
          if(!buf->write(give_flow_id_magic.begin(), give_flow_id_magic.end()))
            return false;
          if(!buf->write(flow.begin(), flow.end()))
            return false;
          if(!buf->write(pubkey.begin(), pubkey.end()))
            return false;
          return buf->write(Zsig.begin(), Zsig.end());
        default:
          return false;
      }
    }

    bool
    OuterMessage::Decode(llarp_buffer_t* buf)
    {
      static constexpr size_t header_size = 2;

      if(buf->size_left() < header_size)
        return false;
      command = *buf->cur;
      ++buf->cur;
      if(*buf->cur != '=')
        return false;
      ++buf->cur;
      switch(command)
      {
        case eOCMD_ObtainFlowID:
          if(!buf->read_into(magic.begin(), magic.end()))
            return false;
          if(!buf->read_into(netid.begin(), netid.end()))
            return false;
          if(!buf->read_uint64(uinteger))
            return false;
          if(!buf->read_into(pubkey.begin(), pubkey.end()))
            return false;
          if(buf->size_left() <= Zsig.size())
            return false;
          Xsize = buf->size_left() - Zsig.size();
          if(!buf->read_into(X.begin(), X.begin() + Xsize))
            return false;
          return buf->read_into(Zsig.begin(), Zsig.end());
        case eOCMD_GiveFlowID:
          if(!buf->read_into(magic.begin(), magic.end()))
            return false;
          if(!buf->read_into(flow.begin(), flow.end()))
            return false;
          if(!buf->read_into(pubkey.begin(), pubkey.end()))
            return false;
          buf->cur += buf->size_left() - Zsig.size();
          return buf->read_into(Zsig.begin(), Zsig.end());
        case eOCMD_Reject:
          if(!buf->read_into(reject.begin(), reject.end()))
            return false;
          if(!buf->read_uint64(uinteger))
            return false;
          if(!buf->read_into(pubkey.begin(), pubkey.end()))
            return false;
          buf->cur += buf->size_left() - Zsig.size();
          return buf->read_into(Zsig.begin(), Zsig.end());
        case eOCMD_SessionNegotiate:
          if(!buf->read_into(flow.begin(), flow.end()))
            return false;
          if(!buf->read_into(pubkey.begin(), pubkey.end()))
            return false;
          if(!buf->read_uint64(uinteger))
            return false;
          if(buf->size_left() == Zsig.size() + 32)
          {
            A.reset(new AlignedBuffer< 32 >());
            if(!buf->read_into(A->begin(), A->end()))
              return false;
          }
          return buf->read_into(Zsig.begin(), Zsig.end());
        case eOCMD_TransmitData:
          if(!buf->read_into(flow.begin(), flow.end()))
            return false;
          if(!buf->read_into(N.begin(), N.end()))
            return false;
          if(buf->size_left() <= Zhash.size())
            return false;
          Xsize = buf->size_left() - Zhash.size();
          if(!buf->read_into(X.begin(), X.begin() + Xsize))
            return false;
          return buf->read_into(Zhash.begin(), Zhash.end());
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
      m_FlowCookie.Randomize();
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
          if(!crypto->verify(m_OuterMsg.pubkey, sigbuf, m_OuterMsg.Zsig))
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
      if(!crypto->shorthash(h, buf))
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
