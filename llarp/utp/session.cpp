#include <utp/session.hpp>

#include <utp/linklayer.hpp>
#include <messages/discard.hpp>
#include <messages/link_intro.hpp>
#include <util/metrics.hpp>
#include <util/memfn.hpp>

namespace llarp
{
  namespace utp
  {
    void
    Session::OnLinkEstablished(ILinkLayer* p)
    {
      parent = p;
      EnterState(eLinkEstablished);
      LogDebug("link established with ", remoteAddr);
    }

    /// pump tx queue
    void
    Session::PumpWrite()
    {
      if(!sock)
        return;
      ssize_t expect = 0;
      std::vector< utp_iovec > send;

      for(const auto& msg : sendq)
      {
        for(const auto& vec : msg.vecs)
        {
          if(vec.iov_len)
          {
            expect += vec.iov_len;
            send.emplace_back(vec);
          }
        }
      }
      if(expect)
      {
        ssize_t s = utp_writev(sock, send.data(), send.size());
        if(s <= 0)
          return;
        lastSend = parent->Now();

        metrics::integerTick("utp.session.tx", "writes", s, "id",
                             RouterID(remoteRC.pubkey).ToString());
        m_TXRate += s;
        size_t sz = s;
        do
        {
          auto& msg = sendq.front();
          while(msg.vecs.size() > 0 && sz >= msg.vecs.front().iov_len)
          {
            sz -= msg.vecs.front().iov_len;
            msg.vecs.pop_front();
          }
          if(msg.vecs.size() == 0)
          {
            msg.Delivered();
            sendq.pop_front();
          }
          else if(sz)
          {
            auto& front = msg.vecs.front();
            front.iov_len -= sz;
            front.iov_base = ((byte_t*)front.iov_base) + sz;
            return;
          }
          else
            return;
        } while(sendq.size());
      }
    }

    /// prune expired inbound messages
    void
    Session::PruneInboundMessages(llarp_time_t now)
    {
      auto itr = m_RecvMsgs.begin();
      while(itr != m_RecvMsgs.end())
      {
        if(itr->second.IsExpired(now))
        {
          itr = m_RecvMsgs.erase(itr);
        }
        else
          ++itr;
      }
    }

    void
    Session::OutboundLinkEstablished(LinkLayer* p)
    {
      OnLinkEstablished(p);
      metrics::integerTick("utp.session.open", "to", 1, "id",
                           RouterID(remoteRC.pubkey).ToString());
      OutboundHandshake();
    }

    template < bool (Crypto::*dh_func)(SharedSecret&, const PubKey&,
                                       const SecretKey&, const TunnelNonce&) >
    bool
    Session::DoKeyExchange(SharedSecret& K, const KeyExchangeNonce& n,
                           const PubKey& other, const SecretKey& secret)
    {
      ShortHash t_h;
      static constexpr size_t TMP_SIZE = 64;
      static_assert(SharedSecret::SIZE + KeyExchangeNonce::SIZE == TMP_SIZE,
                    "Invalid sizes");

      AlignedBuffer< TMP_SIZE > tmp;
      std::copy(K.begin(), K.end(), tmp.begin());
      std::copy(n.begin(), n.end(), tmp.begin() + K.size());
      // t_h = HS(K + L.n)
      if(!CryptoManager::instance()->shorthash(t_h, llarp_buffer_t(tmp)))
      {
        LogError("failed to mix key to ", remoteAddr);
        return false;
      }

      // K = TKE(a.p, B_a.e, sk, t_h)
      if(!(CryptoManager::instance()->*dh_func)(K, other, secret, t_h))
      {
        LogError("key exchange with ", other, " failed");
        return false;
      }
      LogDebug("keys mixed with session to ", remoteAddr);
      return true;
    }

    bool
    Session::MutateKey(SharedSecret& K, const AlignedBuffer< 24 >& A)
    {
      AlignedBuffer< 56 > tmp;
      llarp_buffer_t buf{tmp};
      std::copy(K.begin(), K.end(), buf.cur);
      buf.cur += K.size();
      std::copy(A.begin(), A.end(), buf.cur);
      buf.cur = buf.base;
      return CryptoManager::instance()->shorthash(K, buf);
    }

    void
    Session::Tick(llarp_time_t now)
    {
      PruneInboundMessages(now);
      m_TXRate = 0;
      m_RXRate = 0;
      metrics::integerTick("utp.session.sendq", "size", sendq.size(), "id",
                           RouterID(remoteRC.pubkey).ToString());
    }

    /// low level read
    bool
    Session::Recv(const byte_t* buf, size_t sz)
    {
      // mark we are alive
      Alive();
      m_RXRate += sz;
      size_t s = sz;
      metrics::integerTick("utp.session.rx", "size", s, "id",
                           RouterID(remoteRC.pubkey).ToString());
      // process leftovers
      if(recvBufOffset)
      {
        auto left = FragmentBufferSize - recvBufOffset;
        if(s >= left)
        {
          // yes it fills it
          LogDebug("process leftovers, offset=", recvBufOffset, " sz=", s,
                   " left=", left);
          std::copy(buf, buf + left, recvBuf.begin() + recvBufOffset);
          s -= left;
          recvBufOffset = 0;
          buf += left;
          if(!VerifyThenDecrypt(recvBuf.data()))
            return false;
        }
      }
      // process full fragments
      while(s >= FragmentBufferSize)
      {
        recvBufOffset = 0;
        LogDebug("process full sz=", s);
        if(!VerifyThenDecrypt(buf))
          return false;
        buf += FragmentBufferSize;
        s -= FragmentBufferSize;
      }
      if(s)
      {
        // hold onto leftovers
        LogDebug("leftovers sz=", s);
        std::copy(buf, buf + s, recvBuf.begin() + recvBufOffset);
        recvBufOffset += s;
      }
      return true;
    }

    bool
    Session::TimedOut(llarp_time_t now) const
    {
      if(state == eClose)
        return true;
      if(state == eConnecting)
        return now - lastActive > 5000;
      if(state == eSessionReady)
      {
        // don't time out the connection if backlogged in downstream direction
        // for clients dangling off the side of the network
        const auto timestamp =
            remoteRC.IsPublicRouter() ? lastSend : lastActive;
        if(now <= timestamp)
          return false;
        return now - timestamp > 30000;
      }
      if(state == eLinkEstablished)
        return now - lastActive
            > 10000;  /// 10 second timeout for the whole handshake
      return true;
    }

    PubKey
    Session::GetPubKey() const
    {
      return remoteRC.pubkey;
    }

    Addr
    Session::GetRemoteEndpoint() const
    {
      return remoteAddr;
    }

    /// base constructor
    Session::Session(LinkLayer* p)
    {
      state         = eInitial;
      m_NextTXMsgID = 0;
      m_NextRXMsgID = 0;
      parent        = p;
      remoteTransportPubKey.Zero();

      gotLIM        = false;
      recvBufOffset = 0;

      lastActive = parent->Now();
    }

    bool
    Session::ShouldPing() const
    {
      if(state != eSessionReady)
        return false;
      const auto dlt = parent->Now() - lastSend;
      return dlt >= 10000;
    }

    ILinkLayer*
    Session::GetLinkLayer() const
    {
      return parent;
    }

    void
    Session::Pump()
    {
      // pump write queue
      PumpWrite();
      // prune inbound messages
      PruneInboundMessages(parent->Now());
    }

    bool
    Session::SendMessageBuffer(
        const llarp_buffer_t& buf,
        ILinkSession::CompletionHandler completionHandler)
    {
      if(SendQueueBacklog() >= MaxSendQueueSize)
      {
        // pump write queue if we seem to be full
        PumpWrite();
      }
      if(SendQueueBacklog() >= MaxSendQueueSize)
      {
        // we didn't pump anything wtf
        // this means we're stalled
        return false;
      }
      size_t sz      = buf.sz;
      byte_t* ptr    = buf.base;
      uint32_t msgid = m_NextTXMsgID++;
      sendq.emplace_back(msgid, completionHandler);
      while(sz)
      {
        uint32_t s = std::min(FragmentBodyPayloadSize, sz);
        if(!EncryptThenHash(ptr, msgid, s, sz - s))
        {
          LogError("EncryptThenHash failed?!");
          Close();
          return false;
        }
        LogDebug("encrypted ", s, " bytes");
        ptr += s;
        sz -= s;
      }
      if(state != eSessionReady)
        PumpWrite();
      return true;
    }

    bool
    Session::SendKeepAlive()
    {
      if(ShouldPing())
      {
        DiscardMessage msg;
        std::array< byte_t, 128 > tmp;
        llarp_buffer_t buf(tmp);
        if(!msg.BEncode(&buf))
          return false;
        buf.sz  = buf.cur - buf.base;
        buf.cur = buf.base;
        return this->SendMessageBuffer(buf, nullptr);
      }
      return true;
    }

    void
    Session::OutboundHandshake()
    {
      std::array< byte_t, LinkIntroMessage::MaxSize > tmp;
      llarp_buffer_t buf(tmp);
      // build our RC
      LinkIntroMessage msg;
      msg.rc = parent->GetOurRC();
      if(!msg.rc.Verify(parent->Now()))
      {
        LogError("our RC is invalid? closing session to", remoteAddr);
        Close();
        return;
      }
      msg.N.Randomize();
      msg.P = DefaultLinkSessionLifetime;
      if(!msg.Sign(parent->Sign))
      {
        LogError("failed to sign LIM for outbound handshake to ", remoteAddr);
        Close();
        return;
      }
      // encode
      if(!msg.BEncode(&buf))
      {
        LogError("failed to encode LIM for handshake to ", remoteAddr);
        Close();
        return;
      }
      // rewind
      buf.sz  = buf.cur - buf.base;
      buf.cur = buf.base;
      // send
      if(!SendMessageBuffer(buf, nullptr))
      {
        LogError("failed to send handshake to ", remoteAddr);
        Close();
        return;
      }

      if(!DoClientKeyExchange(txKey, msg.N, remoteTransportPubKey,
                              parent->RouterEncryptionSecret()))
      {
        LogError("failed to mix keys for outbound session to ", remoteAddr);
        Close();
        return;
      }
    }

    Session::~Session()
    {
      if(sock)
      {
        utp_set_userdata(sock, nullptr);
        sock = nullptr;
      }
    }

    bool
    Session::EncryptThenHash(const byte_t* ptr, uint32_t msgid, uint16_t length,
                             uint16_t remaining)

    {
      auto& msg = sendq.back();
      msg.vecs.emplace_back();
      auto& vec = msg.vecs.back();
      msg.fragments.emplace_back();
      auto& buf    = msg.fragments.back();
      vec.iov_base = buf.data();
      vec.iov_len  = FragmentBufferSize;
      buf.Randomize();
      byte_t* noncePtr = buf.data() + FragmentHashSize;
      byte_t* body     = noncePtr + FragmentNonceSize;
      byte_t* base     = body;
      AlignedBuffer< 24 > A(base);
      // skip inner nonce
      body += A.size();
      // put msgid
      htobe32buf(body, msgid);
      body += sizeof(uint32_t);
      // put length
      htobe16buf(body, length);
      body += sizeof(uint16_t);
      // put remaining
      htobe16buf(body, remaining);
      body += sizeof(uint16_t);
      // put body
      memcpy(body, ptr, length);

      llarp_buffer_t payload(base, base,
                             FragmentBufferSize - FragmentOverheadSize);

      TunnelNonce nonce(noncePtr);

      // encrypt
      if(!CryptoManager::instance()->xchacha20(payload, txKey, nonce))
        return false;

      payload.base = noncePtr;
      payload.cur  = payload.base;
      payload.sz   = FragmentBufferSize - FragmentHashSize;
      // key'd hash
      if(!CryptoManager::instance()->hmac(buf.data(), payload, txKey))
        return false;
      return MutateKey(txKey, A);
    }

    void
    Session::EnterState(State st)
    {
      state = st;
      if(st != eClose)
      {
        Alive();
      }
      if(st == eSessionReady)
      {
        parent->MapAddr(remoteRC.pubkey.as_array(), this);
        if(!parent->SessionEstablished(this))
          Close();
      }
    }

    util::StatusObject
    Session::ExtractStatus() const
    {
      return {{"client", !remoteRC.IsPublicRouter()},
              {"sendBacklog", uint64_t(SendQueueBacklog())},
              {"tx", m_TXRate},
              {"rx", m_RXRate},
              {"remoteAddr", remoteAddr.ToString()},
              {"pubkey", remoteRC.pubkey.ToHex()}};
    }

    bool
    Session::GotSessionRenegotiate(const LinkIntroMessage* msg)
    {
      // check with parent and possibly process and store new rc
      if(!parent->SessionRenegotiate(msg->rc, remoteRC))
      {
        // failed to renegotiate
        Close();
        return false;
      }
      // set remote rc
      remoteRC = msg->rc;
      // recalculate rx key
      return DoServerKeyExchange(rxKey, msg->N, remoteRC.enckey,
                                 parent->RouterEncryptionSecret());
    }

    bool
    Session::RenegotiateSession()
    {
      LinkIntroMessage lim;
      lim.rc = parent->GetOurRC();
      lim.N.Randomize();
      lim.P = 60 * 1000 * 10;
      if(!lim.Sign(parent->Sign))
        return false;

      std::array< byte_t, LinkIntroMessage::MaxSize > tmp;
      llarp_buffer_t buf(tmp);
      if(!lim.BEncode(&buf))
        return false;
      // rewind and resize buffer
      buf.sz  = buf.cur - buf.base;
      buf.cur = buf.base;
      // send message
      if(!SendMessageBuffer(buf, nullptr))
        return false;
      // regen our tx Key
      return DoClientKeyExchange(txKey, lim.N, remoteRC.enckey,
                                 parent->RouterEncryptionSecret());
    }

    bool
    Session::VerifyThenDecrypt(const byte_t* ptr)
    {
      LogDebug("verify then decrypt ", remoteAddr);
      ShortHash digest;

      llarp_buffer_t hbuf(ptr + FragmentHashSize,
                          FragmentBufferSize - FragmentHashSize);
      if(!CryptoManager::instance()->hmac(digest.data(), hbuf, rxKey))
      {
        LogError("keyed hash failed");
        return false;
      }
      const ShortHash expected(ptr);
      if(expected != digest)
      {
        LogError("Message Integrity Failed: got ", digest, " from ", remoteAddr,
                 " instead of ", expected);
        Close();
        return false;
      }

      llarp_buffer_t in(ptr + FragmentOverheadSize,
                        FragmentBufferSize - FragmentOverheadSize);

      llarp_buffer_t out(rxFragBody);

      // decrypt
      if(!CryptoManager::instance()->xchacha20_alt(out, in, rxKey,
                                                   ptr + FragmentHashSize))
      {
        LogError("failed to decrypt message from ", remoteAddr);
        return false;
      }
      // get inner nonce
      AlignedBuffer< 24 > A(out.base);
      // advance buffer
      out.cur += A.size();
      // read msgid
      uint32_t msgid;
      if(!out.read_uint32(msgid))
      {
        LogError("failed to read msgid");
        return false;
      }
      // read length and remaining
      uint16_t length, remaining;
      if(!(out.read_uint16(length) && out.read_uint16(remaining)))
      {
        LogError("failed to read the rest of the header");
        return false;
      }
      if(length > (out.sz - (out.cur - out.base)))
      {
        // too big length
        LogError("fragment body too big");
        return false;
      }
      if(msgid < m_NextRXMsgID)
        return false;
      m_NextRXMsgID = msgid;

      // get message
      if(m_RecvMsgs.find(msgid) == m_RecvMsgs.end())
      {
        m_RecvMsgs.emplace(msgid, InboundMessage());
      }

      auto itr = m_RecvMsgs.find(msgid);
      // add message activity
      itr->second.lastActive = parent->Now();
      // append data
      if(!itr->second.AppendData(out.cur, length))
      {
        LogError("inbound buffer is full");
        m_RecvMsgs.erase(itr);
        return false;  // not enough room
      }
      // mutate key
      if(!MutateKey(rxKey, A))
      {
        LogError("failed to mutate rx key");
        return false;
      }

      if(remaining == 0)
      {
        ManagedBuffer buf{itr->second.buffer};
        // resize
        buf.underlying.sz = buf.underlying.cur - buf.underlying.base;
        // rewind
        buf.underlying.cur = buf.underlying.base;
        // process buffer
        LogDebug("got message ", msgid, " from ", remoteAddr);
        parent->HandleMessage(this, buf.underlying);
        m_RecvMsgs.erase(itr);
      }
      return true;
    }

    void
    Session::Close()
    {
      if(state != eClose)
      {
        if(sock)
        {
          if(state == eLinkEstablished || state == eSessionReady)
          {
            // only call shutdown when we are actually connected
            utp_shutdown(sock, SHUT_RDWR);
          }
          utp_close(sock);
          utp_set_userdata(sock, nullptr);
          sock = nullptr;
          LogDebug("utp_close ", remoteAddr);
          if(remoteRC.IsPublicRouter())
          {
            metrics::integerTick("utp.session.close", "to", 1, "id",
                                 RouterID(remoteRC.pubkey).ToString());
          }
          // discard sendq
          // TODO: retry on another session ?
          while(sendq.size())
          {
            sendq.front().Dropped();
            sendq.pop_front();
          }
        }
      }
      EnterState(eClose);
    }

    void
    Session::Alive()
    {
      lastActive = parent->Now();
    }

    InboundSession::InboundSession(LinkLayer* p, utp_socket* s,
                                   const Addr& addr)
        : Session(p)
    {
      sock         = s;
      remoteAddr   = addr;
      RouterID rid = p->GetOurRC().pubkey;
      CryptoManager::instance()->shorthash(rxKey, llarp_buffer_t(rid));
      remoteRC.Clear();

      ABSL_ATTRIBUTE_UNUSED void* res = utp_set_userdata(sock, this);
      assert(res == this);
      assert(s == sock);
      GotLIM = util::memFn(&InboundSession::InboundLIM, this);
    }

    bool
    InboundSession::InboundLIM(const LinkIntroMessage* msg)
    {
      if(gotLIM && remoteRC.pubkey != msg->rc.pubkey)
      {
        Close();
        return false;
      }
      if(!gotLIM)
      {
        remoteRC = msg->rc;
        CryptoManager::instance()->shorthash(txKey,
                                             llarp_buffer_t(remoteRC.pubkey));

        if(!DoServerKeyExchange(rxKey, msg->N, remoteRC.enckey,
                                parent->TransportSecretKey()))
          return false;

        std::array< byte_t, LinkIntroMessage::MaxSize > tmp;
        llarp_buffer_t buf(tmp);
        LinkIntroMessage replymsg;
        replymsg.rc = parent->GetOurRC();
        if(!replymsg.rc.Verify(parent->Now()))
        {
          LogError("our RC is invalid? closing session to", remoteAddr);
          Close();
          return false;
        }
        replymsg.N.Randomize();
        replymsg.P = DefaultLinkSessionLifetime;
        if(!replymsg.Sign(parent->Sign))
        {
          LogError("failed to sign LIM for inbound handshake from ",
                   remoteAddr);
          Close();
          return false;
        }
        // encode
        if(!replymsg.BEncode(&buf))
        {
          LogError("failed to encode LIM for handshake from ", remoteAddr);
          Close();
          return false;
        }
        // rewind
        buf.sz  = buf.cur - buf.base;
        buf.cur = buf.base;
        // send
        if(!SendMessageBuffer(buf, nullptr))
        {
          LogError("failed to repl to handshake from ", remoteAddr);
          Close();
          return false;
        }
        if(!DoClientKeyExchange(txKey, replymsg.N, remoteRC.enckey,
                                parent->RouterEncryptionSecret()))

          return false;
        LogDebug("Sent reply LIM");
        gotLIM = true;
        EnterState(eSessionReady);
        /// future LIM are used for session renegotiation
        GotLIM = util::memFn(&Session::GotSessionRenegotiate, this);
      }
      return true;
    }

    OutboundSession::OutboundSession(LinkLayer* p, utp_socket* s,
                                     const RouterContact& rc,
                                     const AddressInfo& addr)
        : Session(p)
    {
      remoteTransportPubKey = addr.pubkey;
      remoteRC              = rc;
      sock                  = s;
      remoteAddr            = addr;

      RouterID rid = remoteRC.pubkey;
      CryptoManager::instance()->shorthash(txKey, llarp_buffer_t(rid));
      rid = p->GetOurRC().pubkey;
      CryptoManager::instance()->shorthash(rxKey, llarp_buffer_t(rid));

      ABSL_ATTRIBUTE_UNUSED void* res = utp_set_userdata(sock, this);
      assert(res == this);
      assert(s == sock);

      GotLIM = util::memFn(&OutboundSession::OutboundLIM, this);
    }

    void
    OutboundSession::Start()
    {
      utp_connect(sock, remoteAddr, remoteAddr.SockLen());
      EnterState(eConnecting);
    }

    bool
    OutboundSession::OutboundLIM(const LinkIntroMessage* msg)
    {
      if(gotLIM && remoteRC.pubkey != msg->rc.pubkey)
      {
        return false;
      }
      remoteRC = msg->rc;
      gotLIM   = true;

      if(!DoServerKeyExchange(rxKey, msg->N, remoteRC.enckey,
                              parent->RouterEncryptionSecret()))
      {
        Close();
        return false;
      }
      /// future LIM are used for session renegotiation
      GotLIM = util::memFn(&Session::GotSessionRenegotiate, this);
      EnterState(eSessionReady);
      return true;
    }
  }  // namespace utp
}  // namespace llarp
