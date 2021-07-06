#include "session.hpp"

#include <llarp/messages/link_intro.hpp>
#include <llarp/messages/discard.hpp>
#include <llarp/util/meta/memfn.hpp>

namespace llarp
{
  namespace iwp
  {
    ILinkSession::Packet_t
    CreatePacket(Command cmd, size_t plainsize, size_t minpad, size_t variance)
    {
      const size_t pad = minpad > 0 ? minpad + (variance > 0 ? randint() % variance : 0) : 0;
      ILinkSession::Packet_t pkt(PacketOverhead + plainsize + pad + CommandOverhead);
      // randomize pad
      if (pad)
      {
        CryptoManager::instance()->randbytes(
            pkt.data() + PacketOverhead + CommandOverhead + plainsize, pad);
      }
      // randomize nounce
      CryptoManager::instance()->randbytes(pkt.data() + HMACSIZE, TUNNONCESIZE);
      pkt[PacketOverhead] = LLARP_PROTO_VERSION;
      pkt[PacketOverhead + 1] = cmd;
      return pkt;
    }

    constexpr size_t PlaintextQueueSize = 32;

    Session::Session(LinkLayer* p, const RouterContact& rc, const AddressInfo& ai)
        : m_State{State::Initial}
        , m_Inbound{false}
        , m_Parent(p)
        , m_CreatedAt{p->Now()}
        , m_RemoteAddr{ai}
        , m_ChosenAI(ai)
        , m_RemoteRC(rc)
        , m_PlaintextRecv{PlaintextQueueSize}
    {
      token.Zero();
      GotLIM = util::memFn(&Session::GotOutboundLIM, this);
      CryptoManager::instance()->shorthash(m_SessionKey, llarp_buffer_t(rc.pubkey));
    }

    Session::Session(LinkLayer* p, const SockAddr& from)
        : m_State{State::Initial}
        , m_Inbound{true}
        , m_Parent(p)
        , m_CreatedAt{p->Now()}
        , m_RemoteAddr{from}
        , m_PlaintextRecv{PlaintextQueueSize}
    {
      token.Randomize();
      GotLIM = util::memFn(&Session::GotInboundLIM, this);
      const PubKey pk = m_Parent->GetOurRC().pubkey;
      CryptoManager::instance()->shorthash(m_SessionKey, llarp_buffer_t(pk));
    }

    void
    Session::Send_LL(const byte_t* buf, size_t sz)
    {
      LogTrace("send ", sz, " to ", m_RemoteAddr);
      const llarp_buffer_t pkt(buf, sz);
      m_Parent->SendTo_LL(m_RemoteAddr, pkt);
      m_LastTX = time_now_ms();
      m_TXRate += sz;
    }

    bool
    Session::GotInboundLIM(const LinkIntroMessage* msg)
    {
      if (msg->rc.pubkey != m_ExpectedIdent)
      {
        LogError(
            "ident key mismatch from ", m_RemoteAddr, " ", msg->rc.pubkey, " != ", m_ExpectedIdent);
        return false;
      }
      m_State = State::Ready;
      GotLIM = util::memFn(&Session::GotRenegLIM, this);
      m_RemoteRC = msg->rc;
      m_Parent->MapAddr(m_RemoteRC.pubkey, this);
      return m_Parent->SessionEstablished(this, true);
    }

    bool
    Session::GotOutboundLIM(const LinkIntroMessage* msg)
    {
      if (msg->rc.pubkey != m_RemoteRC.pubkey)
      {
        LogError("ident key mismatch");
        return false;
      }

      m_RemoteRC = msg->rc;
      GotLIM = util::memFn(&Session::GotRenegLIM, this);
      auto self = shared_from_this();
      assert(self.use_count() > 1);
      SendOurLIM([self](ILinkSession::DeliveryStatus st) {
        if (st == ILinkSession::DeliveryStatus::eDeliverySuccess)
        {
          self->m_State = State::Ready;
          self->m_Parent->MapAddr(self->m_RemoteRC.pubkey, self.get());
          self->m_Parent->SessionEstablished(self.get(), false);
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
      if (not msg.Sign(m_Parent->Sign))
      {
        LogError("failed to sign our RC for ", m_RemoteAddr);
        return;
      }
      ILinkSession::Message_t data(LinkIntroMessage::MaxSize + PacketOverhead);
      llarp_buffer_t buf(data);
      if (not msg.BEncode(&buf))
      {
        LogError("failed to encode LIM for ", m_RemoteAddr);
      }
      if (!SendMessageBuffer(std::move(data), h))
      {
        LogError("failed to send LIM to ", m_RemoteAddr);
      }
      LogTrace("sent LIM to ", m_RemoteAddr);
    }

    void
    Session::EncryptAndSend(ILinkSession::Packet_t data)
    {
      m_EncryptNext.emplace_back(std::move(data));
      if (!IsEstablished())
      {
        EncryptWorker(std::move(m_EncryptNext));
        m_EncryptNext = CryptoQueue_t{};
      }
    }

    void
    Session::EncryptWorker(CryptoQueue_t msgs)
    {
      LogTrace("encrypt worker ", msgs.size(), " messages");
      for (auto& pkt : msgs)
      {
        llarp_buffer_t pktbuf{pkt};
        const TunnelNonce nonce_ptr{pkt.data() + HMACSIZE};
        pktbuf.base += PacketOverhead;
        pktbuf.cur = pktbuf.base;
        pktbuf.sz -= PacketOverhead;
        CryptoManager::instance()->xchacha20(pktbuf, m_SessionKey, nonce_ptr);
        pktbuf.base = pkt.data() + HMACSIZE;
        pktbuf.sz = pkt.size() - HMACSIZE;
        CryptoManager::instance()->hmac(pkt.data(), pktbuf, m_SessionKey);
        Send_LL(pkt.data(), pkt.size());
      }
    }

    void
    Session::Close()
    {
      if (m_State == State::Closed)
        return;
      auto close_msg = CreatePacket(Command::eCLOS, 0, 16, 16);
      if (m_State == State::Ready)
        m_Parent->UnmapAddr(m_RemoteAddr);
      m_State = State::Closed;
      EncryptAndSend(std::move(close_msg));
      LogInfo(m_Parent->PrintableName(), " closing connection to ", m_RemoteAddr);
    }

    bool
    Session::SendMessageBuffer(
        ILinkSession::Message_t buf, ILinkSession::CompletionHandler completed)
    {
      if (m_TXMsgs.size() >= MaxSendQueueSize)
      {
        if (completed)
          completed(ILinkSession::DeliveryStatus::eDeliveryDropped);
        return false;
      }
      const auto now = m_Parent->Now();
      const auto msgid = m_TXID++;
      const auto bufsz = buf.size();
      auto& msg = m_TXMsgs.emplace(msgid, OutboundMessage{msgid, std::move(buf), now, completed})
                      .first->second;
      EncryptAndSend(msg.XMIT());
      if (bufsz > FragmentSize)
      {
        msg.FlushUnAcked(util::memFn(&Session::EncryptAndSend, this), now);
      }
      m_Stats.totalInFlightTX++;
      LogDebug("send message ", msgid, " to ", m_RemoteAddr);
      return true;
    }

    void
    Session::SendMACK()
    {
      // send multi acks
      while (not m_SendMACKs.empty())
      {
        const auto sz = m_SendMACKs.size();
        const auto max = Session::MaxACKSInMACK;
        auto numAcks = std::min(sz, max);
        auto mack = CreatePacket(Command::eMACK, 1 + (numAcks * sizeof(uint64_t)));
        mack[PacketOverhead + CommandOverhead] = byte_t{static_cast<byte_t>(numAcks)};
        byte_t* ptr = mack.data() + 3 + PacketOverhead;
        LogTrace("send ", numAcks, " macks to ", m_RemoteAddr);
        const auto& itr = m_SendMACKs.top();
        while (numAcks > 0)
        {
          htobe64buf(ptr, itr);
          m_SendMACKs.pop();
          numAcks--;
          ptr += sizeof(uint64_t);
        }
        EncryptAndSend(std::move(mack));
      }
    }

    void
    Session::Pump()
    {
      const auto now = m_Parent->Now();
      if (m_State == State::Ready || m_State == State::LinkIntro)
      {
        if (ShouldPing())
          SendKeepAlive();
        for (auto& item : m_RXMsgs)
        {
          if (item.second.ShouldSendACKS(now))
          {
            item.second.SendACKS(util::memFn(&Session::EncryptAndSend, this), now);
          }
        }
        for (auto& item : m_TXMsgs)
        {
          if (item.second.ShouldFlush(now))
          {
            item.second.FlushUnAcked(util::memFn(&Session::EncryptAndSend, this), now);
          }
        }
      }
      auto self = shared_from_this();
      assert(self.use_count() > 1);
      if (not m_EncryptNext.empty())
      {
        m_Parent->QueueWork([self, data = m_EncryptNext] { self->EncryptWorker(data); });
        m_EncryptNext.clear();
      }

      if (not m_DecryptNext.empty())
      {
        m_Parent->AddWakeup(weak_from_this());
        m_Parent->QueueWork([self, data = m_DecryptNext] { self->DecryptWorker(data); });
        m_DecryptNext.clear();
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
      if (m_State == State::Ready)
      {
        const auto now = m_Parent->Now();
        return now - m_LastTX > PingInterval;
      }
      return false;
    }

    SessionStats
    Session::GetSessionStats() const
    {
      // TODO: thread safety
      return m_Stats;
    }

    util::StatusObject
    Session::ExtractStatus() const
    {
      const auto now = m_Parent->Now();

      return {
          {"txRateCurrent", m_Stats.currentRateTX},
          {"rxRateCurrent", m_Stats.currentRateRX},
          {"rxPktsRcvd", m_Stats.totalPacketsRX},

          // leave 'tx' and 'rx' as duplicates of 'xRateCurrent' for compat
          {"tx", m_Stats.currentRateTX},
          {"rx", m_Stats.currentRateRX},

          {"txPktsAcked", m_Stats.totalAckedTX},
          {"txPktsDropped", m_Stats.totalDroppedTX},
          {"txPktsInFlight", m_Stats.totalInFlightTX},

          {"state", StateToString(m_State)},
          {"inbound", m_Inbound},
          {"replayFilter", m_ReplayFilter.size()},
          {"txMsgQueueSize", m_TXMsgs.size()},
          {"rxMsgQueueSize", m_RXMsgs.size()},
          {"remoteAddr", m_RemoteAddr.toString()},
          {"remoteRC", m_RemoteRC.ExtractStatus()},
          {"created", to_json(m_CreatedAt)},
          {"uptime", to_json(now - m_CreatedAt)}};
    }

    bool
    Session::TimedOut(llarp_time_t now) const
    {
      if (m_State == State::Ready || m_State == State::LinkIntro)
      {
        return now > m_LastRX
            && now - m_LastRX
            > (m_Inbound and not m_RemoteRC.IsPublicRouter() ? DefaultLinkSessionLifetime
                                                             : SessionAliveTimeout);
      }
      return now - m_CreatedAt >= LinkLayerConnectTimeout;
    }

    bool
    Session::ShouldResetRates(llarp_time_t now) const
    {
      return now >= m_ResetRatesAt;
    }

    void
    Session::ResetRates()
    {
      m_Stats.currentRateTX = m_TXRate;
      m_Stats.currentRateRX = m_RXRate;
      m_RXRate = 0;
      m_TXRate = 0;
    }

    void
    Session::Tick(llarp_time_t now)
    {
      if (ShouldResetRates(now))
      {
        ResetRates();
        m_ResetRatesAt = now + 1s;
      }
      // remove pending outbound messsages that timed out
      // inform waiters
      {
        auto itr = m_TXMsgs.begin();
        while (itr != m_TXMsgs.end())
        {
          if (itr->second.IsTimedOut(now))
          {
            m_Stats.totalDroppedTX++;
            m_Stats.totalInFlightTX--;
            LogTrace("Dropped unacked packet to ", m_RemoteAddr);
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
        while (itr != m_RXMsgs.end())
        {
          if (itr->second.IsTimedOut(now))
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
        while (itr != m_ReplayFilter.end())
        {
          if (itr->second + ReplayWindow <= now)
          {
            itr = m_ReplayFilter.erase(itr);
          }
          else
            ++itr;
        }
      }
    }

    using Introduction =
        AlignedBuffer<PubKey::SIZE + PubKey::SIZE + TunnelNonce::SIZE + Signature::SIZE>;

    void
    Session::GenerateAndSendIntro()
    {
      TunnelNonce N;
      N.Randomize();
      {
        ILinkSession::Packet_t req(Introduction::SIZE + PacketOverhead);
        const auto pk = m_Parent->GetOurRC().pubkey;
        const auto e_pk = m_Parent->RouterEncryptionSecret().toPublic();
        auto itr = req.data() + PacketOverhead;
        std::copy_n(pk.data(), pk.size(), itr);
        itr += pk.size();
        std::copy_n(e_pk.data(), e_pk.size(), itr);
        itr += e_pk.size();
        std::copy_n(N.data(), N.size(), itr);
        Signature Z;
        llarp_buffer_t signbuf(req.data() + PacketOverhead, Introduction::SIZE - Signature::SIZE);
        m_Parent->Sign(Z, signbuf);
        std::copy_n(
            Z.data(),
            Z.size(),
            req.data() + PacketOverhead + (Introduction::SIZE - Signature::SIZE));
        CryptoManager::instance()->randbytes(req.data() + HMACSIZE, TUNNONCESIZE);
        EncryptAndSend(std::move(req));
      }
      m_State = State::Introduction;
      if (not CryptoManager::instance()->transport_dh_client(
              m_SessionKey, m_ChosenAI.pubkey, m_Parent->RouterEncryptionSecret(), N))
      {
        LogError("failed to transport_dh_client on outbound session to ", m_RemoteAddr);
        return;
      }
      LogTrace("sent intro to ", m_RemoteAddr);
    }

    void
    Session::HandleCreateSessionRequest(Packet_t pkt)
    {
      if (not DecryptMessageInPlace(pkt))
      {
        LogError(
            m_Parent->PrintableName(), " failed to decrypt session request from ", m_RemoteAddr);
        return;
      }
      if (pkt.size() < token.size() + PacketOverhead)
      {
        LogError(
            m_Parent->PrintableName(),
            " bad session request size, ",
            pkt.size(),
            " < ",
            token.size() + PacketOverhead,
            " from ",
            m_RemoteAddr);
        return;
      }
      const auto begin = pkt.data() + PacketOverhead;
      if (not std::equal(begin, begin + token.size(), token.data()))
      {
        LogError(m_Parent->PrintableName(), " token mismatch from ", m_RemoteAddr);
        return;
      }
      m_LastRX = m_Parent->Now();
      m_State = State::LinkIntro;
      SendOurLIM();
    }

    void
    Session::HandleGotIntro(Packet_t pkt)
    {
      if (pkt.size() < (Introduction::SIZE + PacketOverhead))
      {
        LogWarn(m_Parent->PrintableName(), " intro too small from ", m_RemoteAddr);
        return;
      }
      byte_t* ptr = pkt.data() + PacketOverhead;
      TunnelNonce N;
      std::copy_n(ptr, PubKey::SIZE, m_ExpectedIdent.data());
      ptr += PubKey::SIZE;
      std::copy_n(ptr, PubKey::SIZE, m_RemoteOnionKey.data());
      ptr += PubKey::SIZE;
      std::copy_n(ptr, TunnelNonce::SIZE, N.data());
      ptr += TunnelNonce::SIZE;
      Signature Z;
      std::copy_n(ptr, Z.size(), Z.data());
      const llarp_buffer_t verifybuf(
          pkt.data() + PacketOverhead, Introduction::SIZE - Signature::SIZE);
      if (!CryptoManager::instance()->verify(m_ExpectedIdent, verifybuf, Z))
      {
        LogError(m_Parent->PrintableName(), " intro verify failed from ", m_RemoteAddr);
        return;
      }
      const PubKey pk = m_Parent->TransportSecretKey().toPublic();
      LogDebug(
          "got intro: remote-pk=",
          m_RemoteOnionKey.ToHex(),
          " N=",
          N.ToHex(),
          " local-pk=",
          pk.ToHex());
      if (not CryptoManager::instance()->transport_dh_server(
              m_SessionKey, m_RemoteOnionKey, m_Parent->TransportSecretKey(), N))
      {
        LogError("failed to transport_dh_server on inbound intro from ", m_RemoteAddr);
        return;
      }
      Packet_t reply(token.size() + PacketOverhead);
      // random nonce
      CryptoManager::instance()->randbytes(reply.data() + HMACSIZE, TUNNONCESIZE);
      // set token
      std::copy_n(token.data(), token.size(), reply.data() + PacketOverhead);
      m_LastRX = m_Parent->Now();
      EncryptAndSend(std::move(reply));
      LogDebug("sent intro ack to ", m_RemoteAddr);
      m_State = State::Introduction;
    }

    void
    Session::HandleGotIntroAck(Packet_t pkt)
    {
      if (pkt.size() < (token.size() + PacketOverhead))
      {
        LogError(
            m_Parent->PrintableName(),
            " bad intro ack size ",
            pkt.size(),
            " < ",
            token.size() + PacketOverhead,
            " from ",
            m_RemoteAddr);
        return;
      }
      Packet_t reply(token.size() + PacketOverhead);
      if (not DecryptMessageInPlace(pkt))
      {
        LogError(m_Parent->PrintableName(), " intro ack decrypt failed from ", m_RemoteAddr);
        return;
      }
      m_LastRX = m_Parent->Now();
      std::copy_n(pkt.data() + PacketOverhead, token.size(), token.data());
      std::copy_n(token.data(), token.size(), reply.data() + PacketOverhead);
      // random nounce
      CryptoManager::instance()->randbytes(reply.data() + HMACSIZE, TUNNONCESIZE);
      EncryptAndSend(std::move(reply));
      LogDebug("sent session request to ", m_RemoteAddr);
      m_State = State::LinkIntro;
    }

    bool
    Session::DecryptMessageInPlace(Packet_t& pkt)
    {
      if (pkt.size() <= PacketOverhead)
      {
        LogError("packet too small from ", m_RemoteAddr);
        return false;
      }
      const llarp_buffer_t buf(pkt);
      ShortHash H;
      llarp_buffer_t curbuf(buf.base, buf.sz);
      curbuf.base += ShortHash::SIZE;
      curbuf.sz -= ShortHash::SIZE;
      if (not CryptoManager::instance()->hmac(H.data(), curbuf, m_SessionKey))
      {
        LogError("failed to caclulate keyed hash for ", m_RemoteAddr);
        return false;
      }
      const ShortHash expected{buf.base};
      if (H != expected)
      {
        LogError(
            m_Parent->PrintableName(),
            " keyed hash mismatch ",
            H,
            " != ",
            expected,
            " from ",
            m_RemoteAddr,
            " state=",
            int(m_State),
            " size=",
            buf.sz);
        return false;
      }
      const TunnelNonce N{curbuf.base};
      curbuf.base += 32;
      curbuf.sz -= 32;
      LogTrace("decrypt: ", curbuf.sz, " bytes from ", m_RemoteAddr);
      return CryptoManager::instance()->xchacha20(curbuf, m_SessionKey, N);
    }

    void
    Session::Start()
    {
      if (m_Inbound)
        return;
      GenerateAndSendIntro();
    }

    void
    Session::HandleSessionData(Packet_t pkt)
    {
      m_DecryptNext.emplace_back(std::move(pkt));
    }

    void
    Session::DecryptWorker(CryptoQueue_t msgs)
    {
      auto itr = msgs.begin();
      while (itr != msgs.end())
      {
        auto& pkt = *itr;
        if (not DecryptMessageInPlace(pkt))
        {
          itr = msgs.erase(itr);
          LogError("failed to decrypt session data from ", m_RemoteAddr);
          continue;
        }
        if (pkt[PacketOverhead] != LLARP_PROTO_VERSION)
        {
          LogError(
              "protocol version mismatch ", int(pkt[PacketOverhead]), " != ", LLARP_PROTO_VERSION);
          itr = msgs.erase(itr);
          continue;
        }
        ++itr;
      }
      m_PlaintextRecv.tryPushBack(std::move(msgs));
      m_Parent->WakeupPlaintext();
    }

    void
    Session::HandlePlaintext()
    {
      while (not m_PlaintextRecv.empty())
      {
        auto queue = m_PlaintextRecv.popFront();
        for (auto& result : queue)
        {
          LogTrace("Command ", int(result[PacketOverhead + 1]), " from ", m_RemoteAddr);
          switch (result[PacketOverhead + 1])
          {
            case Command::eXMIT:
              HandleXMIT(std::move(result));
              break;
            case Command::eDATA:
              HandleDATA(std::move(result));
              break;
            case Command::eACKS:
              HandleACKS(std::move(result));
              break;
            case Command::ePING:
              HandlePING(std::move(result));
              break;
            case Command::eNACK:
              HandleNACK(std::move(result));
              break;
            case Command::eCLOS:
              HandleCLOS(std::move(result));
              break;
            case Command::eMACK:
              HandleMACK(std::move(result));
              break;
            default:
              LogError("invalid command ", int(result[PacketOverhead + 1]), " from ", m_RemoteAddr);
          }
        }
      }
      SendMACK();
      Pump();
    }

    void
    Session::HandleMACK(Packet_t data)
    {
      if (data.size() < (3 + PacketOverhead))
      {
        LogError("impossibly short mack from ", m_RemoteAddr);
        return;
      }
      byte_t numAcks = data[CommandOverhead + PacketOverhead];
      if (data.size() < 1 + CommandOverhead + PacketOverhead + (numAcks * sizeof(uint64_t)))
      {
        LogError("short mack from ", m_RemoteAddr);
        return;
      }
      LogTrace("got ", int(numAcks), " mack from ", m_RemoteAddr);
      byte_t* ptr = data.data() + CommandOverhead + PacketOverhead + 1;
      while (numAcks > 0)
      {
        uint64_t acked = bufbe64toh(ptr);
        LogTrace("mack containing txid=", acked, " from ", m_RemoteAddr);
        auto itr = m_TXMsgs.find(acked);
        if (itr != m_TXMsgs.end())
        {
          m_Stats.totalAckedTX++;
          m_Stats.totalInFlightTX--;
          itr->second.Completed();
          m_TXMsgs.erase(itr);
        }
        else
        {
          LogTrace("ignored mack for txid=", acked, " from ", m_RemoteAddr);
        }
        ptr += sizeof(uint64_t);
        numAcks--;
      }
    }

    void
    Session::HandleNACK(Packet_t data)
    {
      if (data.size() < (CommandOverhead + sizeof(uint64_t) + PacketOverhead))
      {
        LogError("short nack from ", m_RemoteAddr);
        return;
      }
      uint64_t txid = bufbe64toh(data.data() + CommandOverhead + PacketOverhead);
      LogTrace("got nack on ", txid, " from ", m_RemoteAddr);
      auto itr = m_TXMsgs.find(txid);
      if (itr != m_TXMsgs.end())
      {
        EncryptAndSend(itr->second.XMIT());
      }
      m_LastRX = m_Parent->Now();
    }

    void
    Session::HandleXMIT(Packet_t data)
    {
      static constexpr size_t XMITOverhead =
          (CommandOverhead + PacketOverhead + sizeof(uint16_t) + sizeof(uint64_t)
           + ShortHash::SIZE);
      if (data.size() < XMITOverhead)
      {
        LogError("short XMIT from ", m_RemoteAddr);
        return;
      }
      uint16_t sz = bufbe16toh(data.data() + CommandOverhead + PacketOverhead);
      uint64_t rxid = bufbe64toh(data.data() + CommandOverhead + sizeof(uint16_t) + PacketOverhead);
      ShortHash h{
          data.data() + CommandOverhead + sizeof(uint16_t) + sizeof(uint64_t) + PacketOverhead};
      LogTrace("rxid=", rxid, " sz=", sz, " h=", h.ToHex(), " from ", m_RemoteAddr);
      m_LastRX = m_Parent->Now();
      {
        // check for replay
        auto itr = m_ReplayFilter.find(rxid);
        if (itr != m_ReplayFilter.end())
        {
          m_SendMACKs.emplace(rxid);
          LogTrace("duplicate rxid=", rxid, " from ", m_RemoteAddr);
          return;
        }
      }
      {
        const auto now = m_Parent->Now();
        auto itr = m_RXMsgs.find(rxid);
        if (itr == m_RXMsgs.end())
        {
          itr =
              m_RXMsgs.emplace(rxid, InboundMessage{rxid, sz, std::move(h), m_Parent->Now()}).first;
          sz = std::min(sz, uint16_t{FragmentSize});
          if ((data.size() - XMITOverhead) == sz)
          {
            {
              const llarp_buffer_t buf(data.data() + (data.size() - sz), sz);
              itr->second.HandleData(0, buf, now);
              if (not itr->second.IsCompleted())
              {
                return;
              }

              if (not itr->second.Verify())
              {
                LogError("bad short xmit hash from ", m_RemoteAddr);
                return;
              }
            }
            HandleRecvMsgCompleted(itr->second);
          }
        }
        else
          LogTrace("got duplicate xmit on ", rxid, " from ", m_RemoteAddr);
      }
    }

    void
    Session::HandleDATA(Packet_t data)
    {
      if (data.size() < (CommandOverhead + sizeof(uint16_t) + sizeof(uint64_t) + PacketOverhead))
      {
        LogError("short DATA from ", m_RemoteAddr, " ", data.size());
        return;
      }
      m_LastRX = m_Parent->Now();
      uint16_t sz = bufbe16toh(data.data() + CommandOverhead + PacketOverhead);
      uint64_t rxid = bufbe64toh(data.data() + CommandOverhead + sizeof(uint16_t) + PacketOverhead);
      auto itr = m_RXMsgs.find(rxid);
      if (itr == m_RXMsgs.end())
      {
        if (m_ReplayFilter.find(rxid) == m_ReplayFilter.end())
        {
          LogTrace("no rxid=", rxid, " for ", m_RemoteAddr);
          auto nack = CreatePacket(Command::eNACK, 8);
          htobe64buf(nack.data() + PacketOverhead + CommandOverhead, rxid);
          EncryptAndSend(std::move(nack));
        }
        else
        {
          LogTrace("replay hit for rxid=", rxid, " for ", m_RemoteAddr);
          m_SendMACKs.emplace(rxid);
        }
        return;
      }

      {
        const llarp_buffer_t buf(
            data.data() + PacketOverhead + 12, data.size() - (PacketOverhead + 12));
        itr->second.HandleData(sz, buf, m_Parent->Now());
      }

      if (itr->second.IsCompleted())
      {
        if (itr->second.Verify())
        {
          HandleRecvMsgCompleted(itr->second);
        }
        else
        {
          LogError("hash mismatch for message ", itr->first);
        }
      }
    }

    void
    Session::HandleRecvMsgCompleted(const InboundMessage& msg)
    {
      const auto rxid = msg.m_MsgID;
      if (m_ReplayFilter.emplace(rxid, m_Parent->Now()).second)
      {
        m_Parent->HandleMessage(this, msg.m_Data);
        EncryptAndSend(msg.ACKS());
        LogDebug("recv'd message ", rxid, " from ", m_RemoteAddr);
      }
      m_RXMsgs.erase(rxid);
    }

    void
    Session::HandleACKS(Packet_t data)
    {
      if (data.size() < (11 + PacketOverhead))
      {
        LogError("short ACKS from ", m_RemoteAddr);
        return;
      }
      const auto now = m_Parent->Now();
      m_LastRX = now;
      uint64_t txid = bufbe64toh(data.data() + 2 + PacketOverhead);
      auto itr = m_TXMsgs.find(txid);
      if (itr == m_TXMsgs.end())
      {
        LogTrace("no txid=", txid, " for ", m_RemoteAddr);
        return;
      }
      itr->second.Ack(data[10 + PacketOverhead]);

      if (itr->second.IsTransmitted())
      {
        LogDebug("sent message ", itr->first, " to ", m_RemoteAddr);
        itr->second.Completed();
        itr = m_TXMsgs.erase(itr);
      }
      else
      {
        itr->second.FlushUnAcked(util::memFn(&Session::EncryptAndSend, this), now);
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
      if (m_State == State::Ready)
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

    bool
    Session::Recv_LL(ILinkSession::Packet_t data)
    {
      m_RXRate += data.size();

      // TODO: differentiate between good and bad RX packets here
      m_Stats.totalPacketsRX++;
      switch (m_State)
      {
        case State::Initial:
          if (m_Inbound)
          {
            // initial data
            // enter introduction phase
            if (DecryptMessageInPlace(data))
            {
              HandleGotIntro(std::move(data));
            }
            else
            {
              LogWarn("bad intro from ", m_RemoteAddr);
              return false;
            }
          }
          break;
        case State::Introduction:
          if (m_Inbound)
          {
            // we are replying to an intro ack
            HandleCreateSessionRequest(std::move(data));
          }
          else
          {
            // we got an intro ack
            // send a session request
            HandleGotIntroAck(std::move(data));
          }
          break;
        case State::LinkIntro:
        default:
          HandleSessionData(std::move(data));
          break;
      }
      return true;
    }

    std::string
    Session::StateToString(State state)
    {
      switch (state)
      {
        case State::Initial:
          return "Initial";
        case State::Introduction:
          return "Introduction";
        case State::LinkIntro:
          return "LinkIntro";
        case State::Ready:
          return "Ready";
        case State::Closed:
          return "Close";
        default:
          return "Invalid";
      }
    }
  }  // namespace iwp
}  // namespace llarp
