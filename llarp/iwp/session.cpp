#include <iwp/session.hpp>
#include <util/memfn.hpp>
#include <messages/link_intro.hpp>
#include <messages/discard.hpp>

namespace llarp
{
  namespace iwp
  {
    static constexpr size_t PacketOverhead = HMACSIZE + TUNNONCESIZE;

    Session::Session(LinkLayer* p, RouterContact rc, AddressInfo ai)
        : m_State{State::Initial}
        , m_Inbound{false}
        , m_Parent{p}
        , m_CreatedAt{p->Now()}
        , m_RemoteAddr{ai}
        , m_ChosenAI{std::move(ai)}
        , m_RemoteRC{std::move(rc)}
    {
      token.Zero();
      GotLIM = util::memFn(&Session::GotOutboundLIM, this);
    }

    Session::Session(LinkLayer* p, Addr from)
        : m_State{State::Initial}
        , m_Inbound{true}
        , m_Parent{p}
        , m_CreatedAt{p->Now()}
        , m_RemoteAddr{from}
    {
      token.Randomize();
      GotLIM = util::memFn(&Session::GotInboundLIM, this);
    }

    void
    Session::Send_LL(const llarp_buffer_t& pkt)
    {
      LogDebug("send ", pkt.sz, " to ", m_RemoteAddr);
      m_Parent->SendTo_LL(m_RemoteAddr, pkt);
      m_LastTX = time_now_ms();
    }

    bool
    Session::GotInboundLIM(const LinkIntroMessage* msg)
    {
      if(msg->rc.enckey != m_RemoteOnionKey)
      {
        LogError("key missmatch");
        return false;
      }
      m_State    = State::Ready;
      GotLIM     = util::memFn(&Session::GotRenegLIM, this);
      m_RemoteRC = msg->rc;
      m_Parent->MapAddr(m_RemoteRC.pubkey, this);
      return m_Parent->SessionEstablished(this);
    }

    bool
    Session::GotOutboundLIM(const LinkIntroMessage* msg)
    {
      if(msg->rc.pubkey != m_RemoteRC.pubkey)
      {
        LogError("ident key missmatch");
        return false;
      }
      m_RemoteRC = msg->rc;
      GotLIM     = util::memFn(&Session::GotRenegLIM, this);
      auto self  = shared_from_this();
      SendOurLIM([self](ILinkSession::DeliveryStatus st) {
        if(st == ILinkSession::DeliveryStatus::eDeliverySuccess)
        {
          self->m_State = State::Ready;
          self->m_Parent->MapAddr(self->m_RemoteRC.pubkey, self.get());
          self->m_Parent->SessionEstablished(self.get());
        }
      });
      return true;
    }

    void
    Session::SendOurLIM(ILinkSession::CompletionHandler h)
    {
      LinkIntroMessage msg;
      msg.rc = m_Parent->GetOurRC();
      msg.N.Randomize();
      msg.P = 60000;
      if(not msg.Sign(m_Parent->Sign))
      {
        LogError("failed to sign our RC for ", m_RemoteAddr);
        return;
      }
      AlignedBuffer< LinkIntroMessage::MaxSize > data;
      llarp_buffer_t buf(data);
      if(not msg.BEncode(&buf))
      {
        LogError("failed to encode LIM for ", m_RemoteAddr);
      }
      buf.sz  = buf.cur - buf.base;
      buf.cur = buf.base;
      if(!SendMessageBuffer(buf, h))
      {
        LogError("failed to send LIM to ", m_RemoteAddr);
      }
      LogDebug("sent LIM to ", m_RemoteAddr);
    }

    void
    Session::EncryptAndSend(const llarp_buffer_t& data)
    {
      std::vector< byte_t > pkt;
      pkt.resize(data.sz + PacketOverhead);
      CryptoManager::instance()->randbytes(pkt.data(), pkt.size());
      llarp_buffer_t pktbuf(pkt);
      pktbuf.base += PacketOverhead;
      pktbuf.sz -= PacketOverhead;
      byte_t* nonce_ptr = pkt.data() + HMACSIZE;

      CryptoManager::instance()->xchacha20_alt(pktbuf, data, m_SessionKey,
                                               nonce_ptr);

      pktbuf.base = nonce_ptr;
      pktbuf.sz   = data.sz + 32;
      CryptoManager::instance()->hmac(pkt.data(), pktbuf, m_SessionKey);

      pktbuf.base = pkt.data();
      pktbuf.sz   = pkt.size();
      Send_LL(pktbuf);
    }

    void
    Session::Close()
    {
      if(m_State == State::Closed)
        return;
      const std::vector< byte_t > close_msg = {LLARP_PROTO_VERSION,
                                               Command::eCLOS};
      const llarp_buffer_t buf(close_msg);
      EncryptAndSend(buf);
      if(m_State == State::Ready)
        m_Parent->UnmapAddr(m_RemoteAddr);
      m_State = State::Closed;
      LogInfo("closing connection to ", m_RemoteAddr);
    }

    bool
    Session::SendMessageBuffer(const llarp_buffer_t& buf,
                               ILinkSession::CompletionHandler completed)
    {
      const auto now   = m_Parent->Now();
      const auto msgid = m_TXID++;
      auto& msg =
          m_TXMsgs.emplace(msgid, OutboundMessage{msgid, buf, now, completed})
              .first->second;
      const auto xmit = msg.XMIT();
      const llarp_buffer_t pkt{xmit};
      EncryptAndSend(pkt);
      msg.FlushUnAcked(util::memFn(&Session::EncryptAndSend, this), now);
      LogDebug("send message ", msgid);
      return true;
    }

    void
    Session::Pump()
    {
      const auto now = m_Parent->Now();
      if(m_State == State::Ready || m_State == State::LinkIntro)
      {
        if(ShouldPing())
          SendKeepAlive();
        for(auto& item : m_RXMsgs)
        {
          if(item.second.ShouldSendACKS(now))
          {
            item.second.SendACKS(util::memFn(&Session::EncryptAndSend, this),
                                 now);
          }
        }
        for(auto& item : m_TXMsgs)
        {
          if(item.second.ShouldFlush(now))
          {
            item.second.FlushUnAcked(
                util::memFn(&Session::EncryptAndSend, this), now);
          }
        }
      }
    }

    bool
    Session::GotRenegLIM(const LinkIntroMessage* lim)
    {
      LogDebug("renegotiate session on ", m_RemoteAddr);
      return m_Parent->SessionRenegotiate(lim->rc, m_RemoteRC);
    }

    bool
    Session::RenegotiateSession()
    {
      SendOurLIM();
      return true;
    }

    bool
    Session::ShouldPing() const
    {
      if(m_State == State::Ready)
      {
        static constexpr llarp_time_t PingInterval = 500;
        const auto now                             = m_Parent->Now();
        return now - m_LastTX > PingInterval;
      }
      return false;
    }

    util::StatusObject
    Session::ExtractStatus() const
    {
      return {{"remoteAddr", m_RemoteAddr.ToString()},
              {"remoteRC", m_RemoteRC.ExtractStatus()}};
    }

    bool
    Session::TimedOut(llarp_time_t now) const
    {
      static constexpr llarp_time_t SessionAliveTimeout = 10000;
      if(m_State == State::Ready || m_State == State::LinkIntro)
      {
        return now > m_LastRX && now - m_LastRX > SessionAliveTimeout;
      }
      return now - m_CreatedAt > SessionAliveTimeout;
    }

    void
    Session::Tick(llarp_time_t now)
    {
      // remove pending outbound messsages that timed out
      // inform waiters
      {
        auto itr = m_TXMsgs.begin();
        while(itr != m_TXMsgs.end())
        {
          if(itr->second.IsTimedOut(now))
          {
            itr->second.InformTimeout();
            itr = m_TXMsgs.erase(itr);
          }
          else
            ++itr;
        }
      }
      {
        // remove pending inbound messages that timed out
        auto itr = m_RXMsgs.begin();
        while(itr != m_RXMsgs.end())
        {
          if(itr->second.IsTimedOut(now))
          {
            itr = m_RXMsgs.erase(itr);
          }
          else
            ++itr;
        }
      }
    }

    using Introduction = AlignedBuffer< 64 >;

    void
    Session::GenerateAndSendIntro()
    {
      Introduction intro;

      TunnelNonce N;
      N.Randomize();
      if(not CryptoManager::instance()->transport_dh_client(
             m_SessionKey, m_ChosenAI.pubkey,
             m_Parent->RouterEncryptionSecret(), N))
      {
        LogError("failed to transport_dh_client on outbound session to ",
                 m_RemoteAddr);
        return;
      }
      const auto pk = m_Parent->RouterEncryptionSecret().toPublic();
      std::copy_n(pk.begin(), pk.size(), intro.begin());
      std::copy(N.begin(), N.end(), intro.begin() + 32);
      LogDebug("pk=", pk.ToHex(), " N=", N.ToHex(),
               " remote-pk=", m_ChosenAI.pubkey.ToHex());
      std::vector< byte_t > req;
      req.resize(intro.size() + (randint() % 64));
      CryptoManager::instance()->randbytes(req.data(), req.size());
      std::copy_n(intro.begin(), intro.size(), req.begin());
      const llarp_buffer_t buf(req);
      Send_LL(buf);
      m_State = State::Introduction;
      LogDebug("sent intro to ", m_RemoteAddr);
    }

    void
    Session::HandleCreateSessionRequest(const llarp_buffer_t& buf)
    {
      std::vector< byte_t > result;
      if(not DecryptMessage(buf, result))
      {
        LogError("failed to decrypt session request from ", m_RemoteAddr);
        return;
      }
      if(result.size() < token.size())
      {
        LogError("bad session request size, ", result.size(), " < ",
                 token.size(), " from ", m_RemoteAddr);
        return;
      }
      if(not std::equal(result.begin(), result.begin() + token.size(),
                        token.begin()))
      {
        LogError("token missmatch from ", m_RemoteAddr);
        return;
      }
      m_LastRX = m_Parent->Now();
      m_State  = State::LinkIntro;
      SendOurLIM();
    }

    void
    Session::HandleGotIntro(const llarp_buffer_t& buf)
    {
      if(buf.sz < Introduction::SIZE)
      {
        LogWarn("intro too small from ", m_RemoteAddr);
        return;
      }
      TunnelNonce N;
      std::copy_n(buf.base, PubKey::SIZE, m_RemoteOnionKey.begin());
      std::copy_n(buf.base + PubKey::SIZE, TunnelNonce::SIZE, N.begin());
      const PubKey pk = m_Parent->TransportSecretKey().toPublic();
      LogDebug("got intro: remote-pk=", m_RemoteOnionKey.ToHex(),
               " N=", N.ToHex(), " local-pk=", pk.ToHex(), " sz=", buf.sz);
      if(not CryptoManager::instance()->transport_dh_server(
             m_SessionKey, m_RemoteOnionKey, m_Parent->TransportSecretKey(), N))
      {
        LogError("failed to transport_dh_server on inbound intro from ",
                 m_RemoteAddr);
        return;
      }
      std::vector< byte_t > reply;
      reply.resize(token.size() + (randint() % 32));
      CryptoManager::instance()->randbytes(reply.data(), reply.size());
      std::copy_n(token.begin(), token.size(), reply.begin());
      const llarp_buffer_t pkt{reply};
      m_LastRX = m_Parent->Now();
      EncryptAndSend(pkt);
      LogDebug("sent intro ack to ", m_RemoteAddr);
      m_State = State::Introduction;
    }

    void
    Session::HandleGotIntroAck(const llarp_buffer_t& buf)
    {
      std::vector< byte_t > reply;
      if(not DecryptMessage(buf, reply))
      {
        LogError("intro ack decrypt failed from ", m_RemoteAddr);
        return;
      }
      if(reply.size() < token.size())
      {
        LogError("bad intro ack size ", reply.size(), " < ", token.size(),
                 " from ", m_RemoteAddr);
        return;
      }
      m_LastRX = m_Parent->Now();
      std::copy_n(reply.begin(), token.size(), token.begin());
      const llarp_buffer_t pkt{token};
      EncryptAndSend(pkt);
      LogDebug("sent session request to ", m_RemoteAddr);
      m_State = State::LinkIntro;
    }

    bool
    Session::DecryptMessage(const llarp_buffer_t& buf,
                            std::vector< byte_t >& result)
    {
      if(buf.sz <= PacketOverhead)
      {
        LogError("packet too small ", buf.sz);
        return false;
      }
      ShortHash H;
      llarp_buffer_t curbuf(buf.base, buf.sz);
      curbuf.base += ShortHash::SIZE;
      curbuf.sz -= ShortHash::SIZE;
      if(not CryptoManager::instance()->hmac(H.data(), curbuf, m_SessionKey))
      {
        LogError("failed to caclulate keyed hash for ", m_RemoteAddr);
        return false;
      }
      const ShortHash expected{buf.base};
      if(H != expected)
      {
        LogError("keyed hash missmatch ", H, " != ", expected, " from ",
                 m_RemoteAddr, " state=", int(m_State), " size=", buf.sz);
        return false;
      }
      const byte_t* nonce_ptr = curbuf.base;
      curbuf.base += 32;
      curbuf.sz -= 32;
      result.resize(buf.sz - PacketOverhead);
      const llarp_buffer_t outbuf(result);
      LogDebug("decrypt: ", result.size(), " bytes from ", m_RemoteAddr);
      return CryptoManager::instance()->xchacha20_alt(outbuf, curbuf,
                                                      m_SessionKey, nonce_ptr);
    }

    void
    Session::Start()
    {
      if(m_Inbound)
        return;
      GenerateAndSendIntro();
    }

    void
    Session::HandleSessionData(const llarp_buffer_t& buf)
    {
      std::vector< byte_t > result;
      if(not DecryptMessage(buf, result))
      {
        LogError("failed to decrypt session data from ", m_RemoteAddr);
        return;
      }
      if(result.size() == token.size())
      {
        /// we got a token so we return it
        const llarp_buffer_t pktbuf(token);
        EncryptAndSend(pktbuf);
        return;
      }
      if(result[0] != LLARP_PROTO_VERSION)
      {
        LogError("protocol version missmatch ", int(result[0]),
                 " != ", LLARP_PROTO_VERSION);
        return;
      }
      LogDebug("command ", int(result[1]), " from ", m_RemoteAddr);
      switch(result[1])
      {
        case Command::eXMIT:
          HandleXMIT(std::move(result));
          return;
        case Command::eDATA:
          HandleDATA(std::move(result));
          return;
        case Command::eACKS:
          HandleACKS(std::move(result));
          return;
        case Command::ePING:
          HandlePING(std::move(result));
          return;
        case Command::eNACK:
          HandleNACK(std::move(result));
          return;
        case Command::eCLOS:
          HandleCLOS(std::move(result));
          return;
      }
      LogError("invalid command ", int(result[1]));
    }

    void
    Session::HandleNACK(std::vector< byte_t > data)
    {
      if(data.size() < 10)
      {
        LogError("short nack from ", m_RemoteAddr);
        return;
      }
      uint64_t txid = bufbe64toh(data.data() + 2);
      LogDebug("got nack on ", txid, " from ", m_RemoteAddr);
      auto itr = m_TXMsgs.find(txid);
      if(itr != m_TXMsgs.end())
      {
        auto xmit = itr->second.XMIT();
        const llarp_buffer_t pkt(xmit);
        EncryptAndSend(pkt);
      }
      m_LastRX = m_Parent->Now();
    }

    void
    Session::HandleXMIT(std::vector< byte_t > data)
    {
      if(data.size() < 44)
      {
        LogError("short XMIT from ", m_RemoteAddr, " ", data.size(), " < 44");
        return;
      }
      uint16_t sz   = bufbe16toh(data.data() + 2);
      uint64_t rxid = bufbe64toh(data.data() + 4);
      ShortHash h{data.data() + 12};
      LogDebug("rxid=", rxid, " sz=", sz, " h=", h.ToHex());
      auto itr = m_RXMsgs.find(rxid);
      if(itr == m_RXMsgs.end())
        m_RXMsgs.emplace(
            rxid, InboundMessage{rxid, sz, std::move(h), m_Parent->Now()});
      else
        LogDebug("got duplicate xmit on ", rxid, " from ", m_RemoteAddr);
      m_LastRX = m_Parent->Now();
    }

    void
    Session::HandleDATA(std::vector< byte_t > data)
    {
      if(data.size() < FragmentSize + 12)
      {
        LogError("short DATA from ", m_RemoteAddr, " ", data.size());
        return;
      }
      m_LastRX      = m_Parent->Now();
      uint16_t sz   = bufbe16toh(data.data() + 2);
      uint64_t rxid = bufbe64toh(data.data() + 4);
      auto itr      = m_RXMsgs.find(rxid);
      if(itr == m_RXMsgs.end())
      {
        LogDebug("no rxid=", rxid, " for ", m_RemoteAddr);
        std::vector< byte_t > nack = {
            LLARP_PROTO_VERSION, Command::eNACK, 0, 0, 0, 0, 0, 0, 0, 0};
        htobe64buf(nack.data() + 2, rxid);
        const llarp_buffer_t nackbuf(nack);
        EncryptAndSend(nackbuf);
        return;
      }
      itr->second.HandleData(sz, data.data() + 12, m_Parent->Now());

      if(itr->second.IsCompleted())
      {
        itr->second.SendACKS(util::memFn(&Session::EncryptAndSend, this),
                             m_Parent->Now());
        if(itr->second.Verify())
        {
          auto msg = std::move(itr->second);
          const llarp_buffer_t buf(msg.m_Data.data(), msg.m_Size);
          m_Parent->HandleMessage(this, buf);
        }
        else
        {
          LogError("hash missmatch for message ", itr->first);
        }
        m_RXMsgs.erase(rxid);
      }
    }

    void
    Session::HandleACKS(std::vector< byte_t > data)
    {
      if(data.size() < 11)
      {
        LogError("short ACKS from ", m_RemoteAddr, " ", data.size(), " < 11");
        return;
      }
      m_LastRX      = m_Parent->Now();
      uint64_t txid = bufbe64toh(data.data() + 2);
      auto itr      = m_TXMsgs.find(txid);
      if(itr == m_TXMsgs.end())
      {
        LogDebug("no txid=", txid, " for ", m_RemoteAddr);
        return;
      }
      itr->second.Ack(data[10]);

      if(itr->second.IsTransmitted())
      {
        LogDebug("sent message ", itr->first);
        itr->second.Completed();
        itr = m_TXMsgs.erase(itr);
      }
    }

    void Session::HandleCLOS(std::vector< byte_t >)
    {
      LogInfo("remote closed by ", m_RemoteAddr);
      Close();
    }

    void Session::HandlePING(std::vector< byte_t >)
    {
      m_LastRX = m_Parent->Now();
    }

    bool
    Session::SendKeepAlive()
    {
      if(m_State == State::Ready)
      {
        std::vector< byte_t > ping{LLARP_PROTO_VERSION, Command::ePING};
        const llarp_buffer_t buf(ping);
        EncryptAndSend(buf);
        return true;
      }
      return false;
    }

    bool
    Session::IsEstablished() const
    {
      return m_State == State::Ready;
    }

    void
    Session::Recv_LL(const llarp_buffer_t& buf)
    {
      switch(m_State)
      {
        case State::Initial:
          if(m_Inbound)
          {
            // initial data
            // enter introduction phase
            HandleGotIntro(buf);
          }
          else
          {
            // this case should never happen
            ::abort();
          }
          break;
        case State::Introduction:
          if(m_Inbound)
          {
            // we are replying to an intro ack
            HandleCreateSessionRequest(buf);
          }
          else
          {
            // we got an intro ack
            // send a session request
            HandleGotIntroAck(buf);
          }
          break;
        case State::LinkIntro:
        default:
          HandleSessionData(buf);
          break;
      }
    }
  }  // namespace iwp
}  // namespace llarp