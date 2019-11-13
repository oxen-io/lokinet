#include <iwp/session.hpp>

#include <messages/link_intro.hpp>
#include <messages/discard.hpp>
#include <util/meta/memfn.hpp>

namespace llarp
{
  namespace iwp
  {
    ILinkSession::Packet_t
    Session::CreatePacket(Command cmd, size_t plainsize, size_t minpad,
                          size_t variance)
    {
      const size_t pad =
          minpad > 0 ? minpad + (variance > 0 ? randint() % variance : 0) : 0;
      const size_t pktsz = PacketOverhead + plainsize + pad + CommandOverhead;
      auto pkt = m_Parent->ObtainPacket(m_EncryptSeqno++, nullptr, pktsz);
      if(pkt == nullptr)
        return nullptr;
      // randomize pad
      if(pad)
      {
        CryptoManager::instance()->randbytes(
            pkt->Data() + PacketOverhead + CommandOverhead + plainsize, pad);
      }
      // randomize nounce
      CryptoManager::instance()->randbytes(pkt->Data() + HMACSIZE,
                                           TUNNONCESIZE);
      pkt->Data()[PacketOverhead]     = LLARP_PROTO_VERSION;
      pkt->Data()[PacketOverhead + 1] = cmd;
      return pkt;
    }

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
      CryptoManager::instance()->shorthash(m_SessionKey,
                                           llarp_buffer_t(rc.pubkey));
    }

    Session::Session(LinkLayer* p, Addr from)
        : m_State{State::Initial}
        , m_Inbound{true}
        , m_Parent{p}
        , m_CreatedAt{p->Now()}
        , m_RemoteAddr{from}
    {
      token.Randomize();
      GotLIM          = util::memFn(&Session::GotInboundLIM, this);
      const PubKey pk = m_Parent->GetOurRC().pubkey;
      CryptoManager::instance()->shorthash(m_SessionKey, llarp_buffer_t(pk));
    }

    Session::~Session()
    {
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
      if(msg->rc.pubkey != m_ExpectedIdent)
      {
        LogError("ident key missmatch from ", m_RemoteAddr, " ", msg->rc.pubkey,
                 " != ", m_ExpectedIdent);
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
      ILinkSession::Message_t data(LinkIntroMessage::MaxSize + PacketOverhead);
      llarp_buffer_t buf(data);
      if(not msg.BEncode(&buf))
      {
        LogError("failed to encode LIM for ", m_RemoteAddr);
      }
      if(!SendMessageBuffer(std::move(data), h))
      {
        LogError("failed to send LIM to ", m_RemoteAddr);
      }
      LogDebug("sent LIM to ", m_RemoteAddr);
    }

    void
    Session::EncryptAndSend(ILinkSession::Packet_t data)
    {
      if(data == nullptr)
        return;
      if(not IsEstablished())
      {
        DoEncryptThenSend(data);
        m_Parent->FreeBuffer(data);
        return;
      }
      m_EncryptQueue.emplace(data);
    }

    void
    Session::DoEncryptThenSend(ILinkSession::Packet_t pkt)
    {
      assert(pkt->Size() > PacketOverhead);
      llarp_buffer_t pktbuf(pkt->Data(), pkt->Size());
      const TunnelNonce nonce_ptr(pkt->Data() + HMACSIZE);
      pktbuf.base += PacketOverhead;
      pktbuf.cur = pktbuf.base;
      pktbuf.sz -= PacketOverhead;
      CryptoManager::instance()->xchacha20(pktbuf, m_SessionKey, nonce_ptr);
      pktbuf.base = pkt->Data() + HMACSIZE;
      pktbuf.sz   = pkt->Size() - HMACSIZE;
      CryptoManager::instance()->hmac(pkt->Data(), pktbuf, m_SessionKey);
      pktbuf.base = pkt->Data();
      pktbuf.cur  = pkt->Data();
      pktbuf.sz   = pkt->Size();
      Send_LL(pktbuf);
    }

    void
    Session::EncryptWorker(CryptoQueue_ptr msgs)
    {
      LogDebug("encrypt worker ", msgs.size(), " messages");
      for(auto msg : msgs)
      {
        DoEncryptThenSend(msg);
      }
      m_Parent->logic()->queue_func(std::bind(
          &Session::FreePackets, shared_from_this(), std::move(msgs)));
    }

    void
    Session::Close()
    {
      if(m_State == State::Closed)
        return;

      auto close_msg = CreatePacket(Command::eCLOS, 0, 16, 16);
      if(close_msg)
      {
        EncryptAndSend(close_msg);
        if(m_State == State::Ready)
          m_Parent->UnmapAddr(m_RemoteAddr);
        m_State = State::Closed;
        LogInfo("closing connection to ", m_RemoteAddr);
      }
    }

    bool
    Session::SendMessageBuffer(ILinkSession::Message_t buf,
                               ILinkSession::CompletionHandler completed)
    {
      if(m_TXMsgs.size() >= MaxSendQueueSize)
        return false;
      const auto now   = m_Parent->Now();
      const auto msgid = m_TXID++;
      auto& msg =
          m_TXMsgs
              .emplace(msgid,
                       OutboundMessage{msgid, std::move(buf), now, completed})
              .first->second;
      LogDebug("send message ", msgid);
      msg.MaybeSendXMIT(this, now);
      msg.FlushUnAcked(this, now);
      return true;
    }

    void
    Session::SendMACK()
    {
      const auto now = m_Parent->Now();
      if(now < m_LastSendMACKs + Session::SendMACKsInterval)
      {
        return;
      }
      bool fail = false;
      // send multi acks
      while(not m_SendMACKs.empty())
      {
        const auto sz  = m_SendMACKs.size();
        const auto max = Session::MaxACKSInMACK;
        auto numAcks   = std::min(sz, max);
        auto mack =
            CreatePacket(Command::eMACK, 1 + (numAcks * sizeof(uint64_t)));
        if(mack)
        {
          mack->Data()[PacketOverhead + CommandOverhead] =
              byte_t{static_cast< byte_t >(numAcks)};
          byte_t* ptr = mack->Data() + CommandOverhead + PacketOverhead + 1;
          LogDebug("send ", numAcks, " macks to ", m_RemoteAddr);
          auto itr = m_SendMACKs.begin();
          while(numAcks > 0)
          {
            const auto rxid = *itr;
            LogDebug("sending MACK for rxid=", rxid, " for ", m_RemoteAddr);
            htobe64buf(ptr, rxid);
            itr = m_SendMACKs.erase(itr);
            numAcks--;
            ptr += sizeof(uint64_t);
          }
        }
        else
        {
          fail = true;
          break;
        }
        EncryptAndSend(mack);
      }
      if(not fail)
      {
        // send nacks
        auto itr = m_SendNACKs.begin();
        while(itr != m_SendNACKs.end())
        {
          const auto rxid = *itr;
          // send nack only if still applicable
          if(m_RXMsgs.find(rxid) == m_RXMsgs.end()
             && m_ReplayFilter.find(rxid) == m_ReplayFilter.end())
          {
            LogDebug("no rxid=", rxid, " for ", m_RemoteAddr, " sending NACK");
            auto nack = CreatePacket(Command::eNACK, 8);
            if(nack)
            {
              htobe64buf(nack->Data() + CommandOverhead + PacketOverhead, rxid);
            }
            else
            {
              fail = true;
              break;
            }
            EncryptAndSend(nack);
          }
          itr = m_SendNACKs.erase(itr);
        }
      }
      if(not fail)
      {
        m_LastSendMACKs = now;
      }
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
            item.second.SendACKS(this, now);
          }
        }
        for(auto& item : m_TXMsgs)
        {
          item.second.MaybeSendXMIT(this, now);
          if(item.second.ShouldFlush(now))
          {
            item.second.FlushUnAcked(this, now);
          }
        }
      }
      auto self = shared_from_this();
      if(not m_EncryptQueue.empty())
      {
        CryptoQueue_ptr work;
        while(not m_EncryptQueue.empty())
        {
          work.emplace_back(m_EncryptQueue.top());
          m_EncryptQueue.pop();
        }
        m_Parent->QueueWork(
            std::bind(&Session::EncryptWorker, self, std::move(work)));
      }

      if(not m_DecryptQueue.empty())
      {
        CryptoQueue_ptr work;
        while(not m_DecryptQueue.empty())
        {
          ILinkSession::Packet_t pkt = m_DecryptQueue.top();
          m_DecryptQueue.pop();
          if(pkt)
            work.emplace_back(pkt);
        }
        if(not work.empty())
        {
          m_Parent->QueueWork(
              std::bind(&Session::DecryptWorker, self, std::move(work)));
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
        const auto now = m_Parent->Now();
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
            m_ReplayFilter.emplace(itr->first, now);
            itr = m_RXMsgs.erase(itr);
          }
          else
            ++itr;
        }
      }
      {
        // decay replay window
        auto itr = m_ReplayFilter.begin();
        while(itr != m_ReplayFilter.end())
        {
          if(itr->second + ReplayWindow <= now)
          {
            itr = m_ReplayFilter.erase(itr);
          }
          else
            ++itr;
        }
      }
    }

    using Introduction = AlignedBuffer< PubKey::SIZE + PubKey::SIZE
                                        + TunnelNonce::SIZE + Signature::SIZE >;

    void
    Session::GenerateAndSendIntro()
    {
      if(m_State != State::Initial)
        return;
      auto req = m_Parent->ObtainPacket(m_EncryptSeqno++, nullptr,
                                        Introduction::SIZE + PacketOverhead);
      if(req == nullptr)
        return;
      TunnelNonce N;
      N.Randomize();
      const auto pk   = m_Parent->GetOurRC().pubkey;
      const auto e_pk = m_Parent->RouterEncryptionSecret().toPublic();
      auto itr        = req->Data() + PacketOverhead;
      std::copy_n(pk.data(), pk.size(), itr);
      itr += pk.size();
      std::copy_n(e_pk.data(), e_pk.size(), itr);
      itr += e_pk.size();
      std::copy_n(N.data(), N.size(), itr);
      Signature Z;
      llarp_buffer_t signbuf(req->Data() + PacketOverhead,
                             Introduction::SIZE - Signature::SIZE);
      m_Parent->Sign(Z, signbuf);
      std::copy_n(Z.data(), Z.size(),
                  req->Data() + PacketOverhead
                      + (Introduction::SIZE - Signature::SIZE));
      CryptoManager::instance()->randbytes(req->Data() + HMACSIZE,
                                           TUNNONCESIZE);
      m_State = State::Introduction;
      EncryptAndSend(req);
      if(not CryptoManager::instance()->transport_dh_client(
             m_SessionKey, m_ChosenAI.pubkey,
             m_Parent->RouterEncryptionSecret(), N))
      {
        LogError("failed to transport_dh_client on outbound session to ",
                 m_RemoteAddr);
        return;
      }
      LogDebug("sent intro to ", m_RemoteAddr);
    }

    void
    Session::HandleCreateSessionRequest(byte_t* ptr, size_t sz)
    {
      if(not DecryptBuffer(ptr, sz))
      {
        LogError("failed to decrypt session request from ", m_RemoteAddr);
        return;
      }
      if(sz < token.size() + PacketOverhead)
      {
        LogError("bad session request size, ", sz, " < ",
                 token.size() + PacketOverhead, " from ", m_RemoteAddr);
        return;
      }
      const auto begin = ptr + PacketOverhead;
      if(not std::equal(begin, begin + token.size(), token.data()))
      {
        LogError("token missmatch from ", m_RemoteAddr);
        return;
      }
      m_LastRX = m_Parent->Now();
      m_State  = State::LinkIntro;
      SendOurLIM();
    }

    void
    Session::HandleGotIntro(byte_t* pkt, size_t sz)
    {
      if(sz < (Introduction::SIZE + PacketOverhead))
      {
        LogWarn("intro too small from ", m_RemoteAddr);
        return;
      }
      byte_t* ptr = pkt + PacketOverhead;
      TunnelNonce N;
      std::copy_n(ptr, PubKey::SIZE, m_ExpectedIdent.data());
      ptr += PubKey::SIZE;
      std::copy_n(ptr, PubKey::SIZE, m_RemoteOnionKey.data());
      ptr += PubKey::SIZE;
      std::copy_n(ptr, TunnelNonce::SIZE, N.data());
      ptr += TunnelNonce::SIZE;
      Signature Z;
      std::copy_n(ptr, Z.size(), Z.data());
      const llarp_buffer_t verifybuf(pkt + PacketOverhead,
                                     Introduction::SIZE - Signature::SIZE);
      if(!CryptoManager::instance()->verify(m_ExpectedIdent, verifybuf, Z))
      {
        LogError("intro verify failed from ", m_RemoteAddr);
        return;
      }

      const PubKey pk = m_Parent->TransportSecretKey().toPublic();
      LogDebug("got intro: remote-pk=", m_RemoteOnionKey.ToHex(),
               " N=", N.ToHex(), " local-pk=", pk.ToHex());
      if(not CryptoManager::instance()->transport_dh_server(
             m_SessionKey, m_RemoteOnionKey, m_Parent->TransportSecretKey(), N))
      {
        LogError("failed to transport_dh_server on inbound intro from ",
                 m_RemoteAddr);
        return;
      }
      auto reply = m_Parent->ObtainPacket(m_EncryptSeqno++, nullptr,
                                          token.size() + PacketOverhead);
      if(reply == nullptr)
        return;

      // random nonce
      CryptoManager::instance()->randbytes(reply->Data() + HMACSIZE,
                                           TUNNONCESIZE);
      // set token
      std::copy_n(token.data(), token.size(), reply->Data() + PacketOverhead);
      m_LastRX = m_Parent->Now();
      EncryptAndSend(reply);
      LogDebug("sent intro ack to ", m_RemoteAddr);
      m_State = State::Introduction;
    }

    void
    Session::HandleGotIntroAck(byte_t* pkt, size_t sz)
    {
      if(sz < (token.size() + PacketOverhead))
      {
        LogError("bad intro ack size ", sz, " < ",
                 token.size() + PacketOverhead, " from ", m_RemoteAddr);
        return;
      }

      if(not DecryptBuffer(pkt, sz))
      {
        LogError("intro ack decrypt failed from ", m_RemoteAddr);
        return;
      }
      auto reply = m_Parent->ObtainPacket(m_EncryptSeqno++, nullptr,
                                          token.size() + PacketOverhead);
      if(reply == nullptr)
        return;
      m_LastRX = m_Parent->Now();
      std::copy_n(pkt + PacketOverhead, token.size(), token.data());
      std::copy_n(token.data(), token.size(), reply->Data() + PacketOverhead);
      // random nounce
      CryptoManager::instance()->randbytes(reply->Data() + HMACSIZE,
                                           TUNNONCESIZE);
      EncryptAndSend(reply);
      LogDebug("sent session request to ", m_RemoteAddr);
      m_State = State::LinkIntro;
    }

    bool
    Session::DecryptBuffer(byte_t* ptr, size_t sz)
    {
      if(sz <= PacketOverhead)
      {
        LogError("packet too small from ", m_RemoteAddr);
        return false;
      }
      const llarp_buffer_t buf(ptr, sz);
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
      const TunnelNonce N{curbuf.base};
      curbuf.base += 32;
      curbuf.sz -= 32;
      LogDebug("decrypt: ", curbuf.sz, " bytes from ", m_RemoteAddr);
      return CryptoManager::instance()->xchacha20(curbuf, m_SessionKey, N);
    }

    void
    Session::Start()
    {
      if(m_Inbound)
        return;
      GenerateAndSendIntro();
    }

    void
    Session::DecryptWorker(CryptoQueue_ptr msgs)
    {
      CryptoQueue_ptr discardMsgs;
      CryptoQueue_ptr recvMsgs;
      for(auto msg : msgs)
      {
        if(not DecryptBuffer(msg->Data(), msg->Size()))
        {
          LogError("failed to decrypt session data from ", m_RemoteAddr);
          discardMsgs.emplace_back(msg);
          continue;
        }
        if(msg->Data()[PacketOverhead] != LLARP_PROTO_VERSION)
        {
          LogError("protocol version missmatch ",
                   int(msg->Data()[PacketOverhead]),
                   " != ", LLARP_PROTO_VERSION);
          discardMsgs.emplace_back(msg);
          continue;
        }
        recvMsgs.emplace_back(msg);
      }
      LogDebug("decrypted ", recvMsgs.size(), " packets from ", m_RemoteAddr);
      if(not discardMsgs.empty())
      {
        m_Parent->logic()->queue_func(std::bind(
            &Session::FreePackets, shared_from_this(), std::move(recvMsgs)));
      }
      if(not recvMsgs.empty())
      {
        m_Parent->logic()->queue_func(std::bind(&Session::HandlePlaintext,
                                                shared_from_this(),
                                                std::move(recvMsgs)));
      }
    }

    void
    Session::FreePackets(CryptoQueue_ptr msgs)
    {
      for(auto msg : msgs)
      {
        m_Parent->FreeBuffer(msg);
      }
    }

    void
    Session::HandlePlaintext(CryptoQueue_ptr msgs)
    {
      if(m_RecvQueue.empty())
      {
        m_Parent->logic()->queue_func(
            std::bind(&Session::PumpRecv, shared_from_this()));
      }
      for(auto msg : msgs)
      {
        m_RecvQueue.emplace(msg);
      }
    }

    void
    Session::PumpRecv()
    {
      while(not m_RecvQueue.empty())
      {
        Packet_t pkt = m_RecvQueue.top();
        assert(pkt != nullptr);
        m_RecvQueue.pop();
        LogDebug("Command ", int(pkt->Data()[PacketOverhead + 1]));
        switch(pkt->Data()[PacketOverhead + 1])
        {
          case Command::eXMIT:
            HandleXMIT(pkt);
            break;
          case Command::eDATA:
            HandleDATA(pkt);
            break;
          case Command::eACKS:
            HandleACKS(pkt);
            break;
          case Command::ePING:
            HandlePING(pkt);
            break;
          case Command::eNACK:
            HandleNACK(pkt);
            break;
          case Command::eCLOS:
            HandleCLOS(pkt);
            break;
          case Command::eMACK:
            HandleMACK(pkt);
            break;
          case Command::eDROP:
            HandleDROP(pkt);
            break;
          default:
            LogError("invalid command ", int(pkt->Data()[PacketOverhead + 1]),
                     " from ", m_RemoteAddr);
        }
        m_Parent->FreeBuffer(pkt);
      }
      SendMACK();
      m_Parent->PumpDone(this);
    }

    void
    Session::HandleDROP(Packet_t data)
    {
      if(data->Size() < (CommandOverhead + sizeof(uint64_t) + PacketOverhead))
      {
        LogError("short DROP from ", m_RemoteAddr);
        return;
      }
      const uint64_t rxid =
          bufbe64toh(data->Data() + CommandOverhead + PacketOverhead);
      auto itr = m_RXMsgs.find(rxid);
      if(itr != m_RXMsgs.end())
      {
        LogDebug("dropping rxid=", rxid, " for ", m_RemoteAddr);
        const auto now = m_Parent->Now();
        m_ReplayFilter.emplace(rxid, now);
        m_RXMsgs.erase(itr);
      }
      else
      {
        LogDebug("not dropping rxid=", rxid, " for ", m_RemoteAddr);
      }
    }

    void
    Session::HandleMACK(Packet_t data)
    {
      if(data->Size() < (CommandOverhead + PacketOverhead + 1))
      {
        LogError("impossibly short mack from ", m_RemoteAddr);
        return;
      }
      byte_t numAcks = data->Data()[CommandOverhead + PacketOverhead];
      if(data->Size()
         < 1 + CommandOverhead + PacketOverhead + (numAcks * sizeof(uint64_t)))
      {
        LogError("short mack from ", m_RemoteAddr);
        return;
      }
      LogDebug("got ", int(numAcks), " mack from ", m_RemoteAddr);
      byte_t* ptr = data->Data() + CommandOverhead + PacketOverhead + 1;
      while(numAcks > 0)
      {
        uint64_t acked = bufbe64toh(ptr);
        LogDebug("mack containing txid=", acked, " from ", m_RemoteAddr);
        auto itr = m_TXMsgs.find(acked);
        if(itr != m_TXMsgs.end())
        {
          itr->second.Completed();
          m_TXMsgs.erase(itr);
        }
        else
        {
          LogDebug("ignored mack for txid=", acked, " from ", m_RemoteAddr);
          if(m_DROP)
          {
            auto msg = CreatePacket(Command::eDROP, 8);
            if(msg)
            {
              htobe64buf(msg->Data() + PacketOverhead + CommandOverhead, acked);
            }
            EncryptAndSend(msg);
          }
        }
        ptr += sizeof(uint64_t);
        numAcks--;
      }
    }

    void
    Session::HandleNACK(Packet_t data)
    {
      if(data->Size() < (CommandOverhead + sizeof(uint64_t) + PacketOverhead))
      {
        LogError("short nack from ", m_RemoteAddr);
        return;
      }
      const auto now = m_Parent->Now();
      const uint64_t txid =
          bufbe64toh(data->Data() + CommandOverhead + PacketOverhead);
      LogDebug("got nack on ", txid, " from ", m_RemoteAddr);
      auto itr = m_TXMsgs.find(txid);
      if(itr != m_TXMsgs.end())
      {
        itr->second.m_LastXMIT = now;
        EncryptAndSend(itr->second.XMIT(this));
      }
      m_LastRX = now;
    }

    void
    Session::HandleXMIT(Packet_t data)
    {
      if(data->Size() < (CommandOverhead + PacketOverhead + sizeof(uint16_t)
                         + sizeof(uint64_t) + ShortHash::SIZE))
      {
        LogError("short XMIT from ", m_RemoteAddr);
        return;
      }
      const auto now = m_Parent->Now();
      const uint16_t sz =
          bufbe16toh(data->Data() + CommandOverhead + PacketOverhead);
      const uint64_t rxid = bufbe64toh(data->Data() + CommandOverhead
                                       + sizeof(uint16_t) + PacketOverhead);
      ShortHash h{data->Data() + CommandOverhead + sizeof(uint16_t)
                  + sizeof(uint64_t) + PacketOverhead};
      LogDebug("rxid=", rxid, " sz=", sz, " h=", h.ToHex());
      m_LastRX = now;
      {
        // check for replay
        auto itr = m_ReplayFilter.find(rxid);
        if(itr != m_ReplayFilter.end())
        {
          if(m_MACK)
          {
            m_SendMACKs.emplace(rxid);
          }
          else
          {
            SendACKSFor(rxid, 0xff, true);
          }
          LogDebug("duplicate rxid=", rxid, " from ", m_RemoteAddr);
          return;
        }
      }
      {
        auto itr = m_RXMsgs.find(rxid);
        if(itr == m_RXMsgs.end())
        {
          m_RXMsgs.emplace(rxid, InboundMessage(rxid, sz, std::move(h), now));
        }
        else
        {
          LogDebug("got duplicate xmit on ", rxid, " from ", m_RemoteAddr);
          // send explict acks
          itr->second.m_LastACKSent = now;
          SendACKSFor(rxid, itr->second.AcksBitmask(), false);
        }
      }
    }

    void
    Session::SendACKSFor(uint64_t rxid, byte_t bitmask, bool replayHit)
    {
      if(replayHit)
      {
        auto msg = CreatePacket(Command::eACKS, 9);
        if(msg == nullptr)
          return;
        // data fragment for previosuly gotten message
        // send explicit ack
        LogDebug("replay hit for rxid=", rxid, " for ", m_RemoteAddr,
                 " sending explicit ACK");
        htobe64buf(msg->Data() + PacketOverhead + CommandOverhead, rxid);
        msg->Data()[PacketOverhead + CommandOverhead + sizeof(uint64_t)] =
            bitmask;
        EncryptAndSend(msg);
      }
      else
      {
        // data fragment with no xmit
        // send nack
        m_SendNACKs.emplace(rxid);
      }
    }

    void
    Session::HandleDATA(Packet_t data)
    {
      if(data->Size() < (CommandOverhead + sizeof(uint16_t) + sizeof(uint64_t)
                         + PacketOverhead))
      {
        LogError("short DATA from ", m_RemoteAddr, " ", data->Size());
        return;
      }
      const auto now = m_Parent->Now();
      m_LastRX       = now;
      const uint16_t sz =
          bufbe16toh(data->Data() + CommandOverhead + PacketOverhead);
      const uint64_t rxid = bufbe64toh(data->Data() + CommandOverhead
                                       + sizeof(uint16_t) + PacketOverhead);
      auto itr            = m_RXMsgs.find(rxid);
      if(itr == m_RXMsgs.end())
      {
        // no pending rx message
        const bool replayHit =
            m_ReplayFilter.find(rxid) != m_ReplayFilter.end();
        if(m_MACK)
        {
          if(replayHit)
          {
            // data fragment for previosuly gotten message
            // queue multiack
            m_SendMACKs.emplace(rxid);
          }
          else
          {
            // data fragment with no xmit
            m_SendNACKs.emplace(rxid);
          }
        }
        else
          SendACKSFor(rxid, 0xff, replayHit);
        return;
      }

      {
        constexpr auto offset = PacketOverhead + CommandOverhead
            + sizeof(uint16_t) + sizeof(uint64_t);
        const llarp_buffer_t buf(data->Data() + offset, data->Size() - offset);
        itr->second.HandleData(sz, buf, now);
      }

      if(itr->second.IsCompleted())
      {
        if(itr->second.Verify())
        {
          auto msg = std::move(itr->second);
          const llarp_buffer_t buf(msg.m_Data);
          m_Parent->HandleMessage(this, buf);
          m_ReplayFilter.emplace(rxid, now);
          if(m_MACK)
            m_SendMACKs.emplace(rxid);
          else
            msg.SendACKS(this, now);
        }
        else
        {
          LogError("hash missmatch for message ", itr->first);
        }
        m_RXMsgs.erase(itr);
      }
    }

    void
    Session::HandleACKS(Packet_t data)
    {
      if(data->Size()
         < (1 + PacketOverhead + CommandOverhead + sizeof(uint64_t)))
      {
        LogError("short ACKS from ", m_RemoteAddr);
        return;
      }
      const auto now = m_Parent->Now();
      m_LastRX       = now;
      const uint64_t txid =
          bufbe64toh(data->Data() + CommandOverhead + PacketOverhead);
      auto itr = m_TXMsgs.find(txid);
      if(itr == m_TXMsgs.end())
      {
        LogDebug("no txid=", txid, " for ", m_RemoteAddr);
        if(m_DROP)
        {
          auto msg = CreatePacket(Command::eDROP, 8);
          if(msg)
          {
            htobe64buf(msg->Data() + PacketOverhead + CommandOverhead, txid);
          }
          EncryptAndSend(msg);
        }
        return;
      }
      itr->second.Ack(
          data->Data()[CommandOverhead + PacketOverhead + sizeof(uint64_t)]);

      if(itr->second.IsTransmitted())
      {
        LogDebug("sent message ", itr->first);
        itr->second.Completed();
        itr = m_TXMsgs.erase(itr);
      }
      else
      {
        itr->second.FlushUnAcked(this, now);
      }
    }

    void Session::HandleCLOS(Packet_t)
    {
      LogInfo("remote closed by ", m_RemoteAddr);
      Close();
    }

    void Session::HandlePING(Packet_t)
    {
      m_LastRX = m_Parent->Now();
    }

    bool
    Session::SendKeepAlive()
    {
      if(m_State == State::Ready)
      {
        EncryptAndSend(CreatePacket(Command::ePING, 0));
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
    Session::Recv_LL(byte_t* buf, size_t sz)
    {
      LogDebug("got ", sz, " from ", m_RemoteAddr, " state=", int(m_State));
      switch(m_State)
      {
        case State::Closed:
          break;
        case State::Initial:
          if(m_Inbound)
          {
            // initial data
            // enter introduction phase
            if(DecryptBuffer(buf, sz))
              HandleGotIntro(buf, sz);
            else
              LogError("bad intro from ", m_RemoteAddr);
          }
          break;
        case State::Introduction:
          if(m_Inbound)
          {
            // we are replying to an intro ack
            HandleCreateSessionRequest(buf, sz);
          }
          else
          {
            // we got an intro ack
            // send a session request
            HandleGotIntroAck(buf, sz);
          }
          break;
        default:

          m_DecryptQueue.emplace(
              m_Parent->ObtainPacket(m_DecryptSeqno++, buf, sz));
          break;
      }
    }
  }  // namespace iwp
}  // namespace llarp
