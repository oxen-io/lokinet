#include <link/utp.hpp>

#include <crypto/crypto.hpp>
#include <link/server.hpp>
#include <messages/discard.hpp>
#include <messages/link_intro.hpp>
#include <router/abstractrouter.hpp>
#include <util/buffer.hpp>
#include <util/endian.hpp>

#ifdef __linux__
#include <linux/errqueue.h>
#include <netinet/ip_icmp.h>
#endif

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <wspiapi.h>
#endif

#ifndef IP_DONTFRAGMENT
#define IP_DONTFRAGMENT IP_DONTFRAG
#endif

#include <link/utp_internal.hpp>

namespace llarp
{
  namespace utp
  {
    using namespace std::placeholders;

    bool
    InboundMessage::IsExpired(llarp_time_t now) const
    {
      return now > lastActive && now - lastActive >= 2000;
    }

    bool
    InboundMessage::AppendData(const byte_t* ptr, uint16_t sz)
    {
      if(llarp_buffer_size_left(buffer) < sz)
        return false;
      memcpy(buffer.cur, ptr, sz);
      buffer.cur += sz;
      return true;
    }

    void
    Session::OnLinkEstablished(LinkLayer* p)
    {
      parent = p;
      EnterState(eLinkEstablished);
      LogDebug("link established with ", remoteAddr);
    }

    Crypto*
    Session::OurCrypto()
    {
      return parent->OurCrypto();
    }

    Crypto*
    LinkLayer::OurCrypto()
    {
      return _crypto;
    }

    /// pump tx queue
    void
    Session::PumpWrite()
    {
      if(!sock)
        return;
      ssize_t expect = 0;
      std::vector< utp_iovec > vecs;
      for(const auto& vec : vecq)
      {
        expect += vec.iov_len;
        vecs.push_back(vec);
      }
      if(expect)
      {
        ssize_t s = utp_writev(sock, vecs.data(), vecs.size());

        while(s > static_cast< ssize_t >(vecq.front().iov_len))
        {
          s -= vecq.front().iov_len;
          vecq.pop_front();
          sendq.pop_front();
        }
        if(vecq.size())
        {
          auto& front = vecq.front();
          front.iov_len -= s;
          front.iov_base = ((byte_t*)front.iov_base) + s;
        }
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
          itr = m_RecvMsgs.erase(itr);
        else
          ++itr;
      }
    }

    void
    Session::Connect()
    {
      utp_connect(sock, remoteAddr, remoteAddr.SockLen());
      EnterState(eConnecting);
    }

    void
    Session::OutboundLinkEstablished(LinkLayer* p)
    {
      OnLinkEstablished(p);
      OutboundHandshake();
    }

    bool
    Session::DoKeyExchange(transport_dh_func dh, SharedSecret& K,
                           const KeyExchangeNonce& n, const PubKey& other,
                           const SecretKey& secret)
    {
      ShortHash t_h;
      static constexpr size_t TMP_SIZE = 64;
      static_assert(SharedSecret::SIZE + KeyExchangeNonce::SIZE == TMP_SIZE,
                    "Invalid sizes");

      AlignedBuffer< TMP_SIZE > tmp;
      std::copy(K.begin(), K.end(), tmp.begin());
      std::copy(n.begin(), n.end(), tmp.begin() + K.size());
      // t_h = HS(K + L.n)
      if(!OurCrypto()->shorthash(t_h, llarp_buffer_t(tmp)))
      {
        LogError("failed to mix key to ", remoteAddr);
        return false;
      }

      // K = TKE(a.p, B_a.e, sk, t_h)
      if(!dh(K, other, secret, t_h))
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
      return OurCrypto()->shorthash(K, buf);
    }

    void
    Session::TickImpl(llarp_time_t now)
    {
      PruneInboundMessages(now);
    }

    /// low level read
    bool
    Session::Recv(const byte_t* buf, size_t sz)
    {
      // mark we are alive
      Alive();
      size_t s = sz;
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
    Session::IsTimedOut(llarp_time_t now) const
    {
      if(state == eClose)
        return true;
      if(now < lastActive)
        return false;
      auto dlt = now - lastActive;
      if(dlt >= sessionTimeout)
      {
        LogDebug("session timeout reached for ", remoteAddr);
        return true;
      }
      return false;
    }

    const PubKey&
    Session::RemotePubKey() const
    {
      return remoteRC.pubkey;
    }

    Addr
    Session::RemoteEndpoint()
    {
      return remoteAddr;
    }

    uint64
    LinkLayer::SendTo(utp_callback_arguments* arg)
    {
      LinkLayer* l =
          static_cast< LinkLayer* >(utp_context_get_userdata(arg->context));
      if(l == nullptr)
        return 0;
      LogDebug("utp_sendto ", Addr(*arg->address), " ", arg->len, " bytes");
      // For whatever reason, the UTP_UDP_DONTFRAG flag is set
      // on the socket itself....which isn't correct and causes
      // winsock (at minimum) to reeee
      // here, we check its value, then set fragmentation the _right_
      // way. Naturally, Linux has its own special procedure.
      // Of course, the flag itself is cleared. -rick
#ifndef _WIN32
      // No practical method of doing this on NetBSD or Darwin
      // without resorting to raw sockets
#if !(__NetBSD__ || __OpenBSD__ || (__APPLE__ && __MACH__))
#ifndef __linux__
      if(arg->flags == 2)
      {
        int val = 1;
        setsockopt(l->m_udp.fd, IPPROTO_IP, IP_DONTFRAGMENT, &val, sizeof(val));
      }
      else
      {
        int val = 0;
        setsockopt(l->m_udp.fd, IPPROTO_IP, IP_DONTFRAGMENT, &val, sizeof(val));
      }
#else
      if(arg->flags == 2)
      {
        int val = IP_PMTUDISC_DO;
        setsockopt(l->m_udp.fd, IPPROTO_IP, IP_MTU_DISCOVER, &val, sizeof(val));
      }
      else
      {
        int val = IP_PMTUDISC_DONT;
        setsockopt(l->m_udp.fd, IPPROTO_IP, IP_MTU_DISCOVER, &val, sizeof(val));
      }
#endif
#endif
      arg->flags = 0;
      if(::sendto(l->m_udp.fd, (char*)arg->buf, arg->len, arg->flags,
                  arg->address, arg->address_len)
             == -1
         && errno)
#else
      if(arg->flags == 2)
      {
        char val = 1;
        setsockopt(l->m_udp.fd, IPPROTO_IP, IP_DONTFRAGMENT, &val, sizeof(val));
      }
      else
      {
        char val = 0;
        setsockopt(l->m_udp.fd, IPPROTO_IP, IP_DONTFRAGMENT, &val, sizeof(val));
      }
      arg->flags = 0;
      if(::sendto(l->m_udp.fd, (char*)arg->buf, arg->len, arg->flags,
                  arg->address, arg->address_len)
         == -1)
#endif
      {
#ifdef _WIN32
        char buf[1024];
        int err = WSAGetLastError();
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, err, LANG_NEUTRAL,
                      buf, 1024, nullptr);
        LogError("sendto failed: ", buf);
#else
        LogError("sendto failed: ", strerror(errno));
#endif
      }
      return 0;
    }

    uint64
    LinkLayer::OnError(utp_callback_arguments* arg)
    {
      Session* session = static_cast< Session* >(utp_get_userdata(arg->socket));

      LinkLayer* link =
          static_cast< LinkLayer* >(utp_context_get_userdata(arg->context));

      if(session && link)
      {
        link->HandleTimeout(session);
        LogError(utp_error_code_names[arg->error_code], " via ",
                 session->remoteAddr);
        if(arg->error_code != UTP_ETIMEDOUT)
          session->Close();
        link->RemovePending(session);
      }
      return 0;
    }

    uint64
    LinkLayer::OnLog(utp_callback_arguments* arg)
    {
      LogDebug(arg->buf);
      return 0;
    }

    LinkLayer::LinkLayer(Crypto* crypto, const SecretKey& routerEncSecret,
                         GetRCFunc getrc, LinkMessageHandler h,
                         SignBufferFunc sign,
                         SessionEstablishedHandler established,
                         SessionRenegotiateHandler reneg,
                         TimeoutHandler timeout, SessionClosedHandler closed)
        : ILinkLayer(routerEncSecret, getrc, h, sign, established, reneg,
                     timeout, closed)
    {
      _crypto  = crypto;
      _utp_ctx = utp_init(2);
      utp_context_set_userdata(_utp_ctx, this);
      utp_set_callback(_utp_ctx, UTP_SENDTO, &LinkLayer::SendTo);
      utp_set_callback(_utp_ctx, UTP_ON_ACCEPT, &LinkLayer::OnAccept);
      utp_set_callback(_utp_ctx, UTP_ON_STATE_CHANGE,
                       &LinkLayer::OnStateChange);
      utp_set_callback(_utp_ctx, UTP_ON_READ, &LinkLayer::OnRead);
      utp_set_callback(_utp_ctx, UTP_ON_ERROR, &LinkLayer::OnError);
      utp_set_callback(_utp_ctx, UTP_LOG, &LinkLayer::OnLog);
      utp_context_set_option(_utp_ctx, UTP_LOG_NORMAL, 1);
      utp_context_set_option(_utp_ctx, UTP_LOG_MTU, 1);
      utp_context_set_option(_utp_ctx, UTP_LOG_DEBUG, 1);
      utp_context_set_option(_utp_ctx, UTP_SNDBUF, MAX_LINK_MSG_SIZE * 16);
      utp_context_set_option(_utp_ctx, UTP_RCVBUF, MAX_LINK_MSG_SIZE * 64);
    }

    LinkLayer::~LinkLayer()
    {
      utp_destroy(_utp_ctx);
    }

    uint16_t
    LinkLayer::Rank() const
    {
      return 1;
    }

    void
    LinkLayer::RecvFrom(const Addr& from, const void* buf, size_t sz)
    {
      utp_process_udp(_utp_ctx, (const byte_t*)buf, sz, from, from.SockLen());
    }

#ifdef __linux__
    void
    LinkLayer::ProcessICMP()
    {
      do
      {
        byte_t vec_buf[4096], ancillary_buf[4096];
        struct iovec iov = {vec_buf, sizeof(vec_buf)};
        struct sockaddr_in remote;
        struct msghdr msg;
        ssize_t len;
        struct cmsghdr* cmsg;
        struct sock_extended_err* e;
        struct sockaddr* icmp_addr;
        struct sockaddr_in* icmp_sin;

        memset(&msg, 0, sizeof(msg));

        msg.msg_name       = &remote;
        msg.msg_namelen    = sizeof(remote);
        msg.msg_iov        = &iov;
        msg.msg_iovlen     = 1;
        msg.msg_flags      = 0;
        msg.msg_control    = ancillary_buf;
        msg.msg_controllen = sizeof(ancillary_buf);

        len = recvmsg(m_udp.fd, &msg, MSG_ERRQUEUE | MSG_DONTWAIT);
        if(len < 0)
        {
          if(errno == EAGAIN || errno == EWOULDBLOCK)
            errno = 0;
          else
            LogError("failed to read icmp for utp ", strerror(errno));
          return;
        }

        for(cmsg = CMSG_FIRSTHDR(&msg); cmsg; cmsg = CMSG_NXTHDR(&msg, cmsg))
        {
          if(cmsg->cmsg_type != IP_RECVERR)
          {
            continue;
          }
          if(cmsg->cmsg_level != SOL_IP)
          {
            continue;
          }
          e = (struct sock_extended_err*)CMSG_DATA(cmsg);
          if(!e)
            continue;
          if(e->ee_origin != SO_EE_ORIGIN_ICMP)
          {
            continue;
          }
          icmp_addr = (struct sockaddr*)SO_EE_OFFENDER(e);
          icmp_sin  = (struct sockaddr_in*)icmp_addr;
          if(icmp_sin->sin_port != 0)
          {
            continue;
          }
          if(e->ee_type == 3 && e->ee_code == 4)
          {
            utp_process_icmp_fragmentation(_utp_ctx, vec_buf, len,
                                           (struct sockaddr*)&remote,
                                           sizeof(remote), e->ee_info);
          }
          else
          {
            utp_process_icmp_error(_utp_ctx, vec_buf, len,
                                   (struct sockaddr*)&remote, sizeof(remote));
          }
        }
      } while(true);
    }
#endif

    void
    LinkLayer::Pump()
    {
      utp_issue_deferred_acks(_utp_ctx);
#ifdef __linux__
      ProcessICMP();
#endif
      std::set< RouterID > sessions;
      {
        Lock l(m_AuthedLinksMutex);
        auto itr = m_AuthedLinks.begin();
        while(itr != m_AuthedLinks.end())
        {
          sessions.insert(itr->first);
          ++itr;
        }
      }
      ILinkLayer::Pump();
      {
        Lock l(m_AuthedLinksMutex);
        for(const auto& pk : sessions)
        {
          if(m_AuthedLinks.count(pk) == 0)
          {
            // all sessions were removed
            SessionClosed(pk);
          }
        }
      }
    }

    void
    LinkLayer::Stop()
    {
      ForEachSession([](ILinkSession* s) { s->SendClose(); });
    }

    bool
    LinkLayer::KeyGen(SecretKey& k)
    {
      OurCrypto()->encryption_keygen(k);
      return true;
    }

    void
    LinkLayer::Tick(llarp_time_t now)
    {
      utp_check_timeouts(_utp_ctx);
      ILinkLayer::Tick(now);
    }

    utp_socket*
    LinkLayer::NewSocket()
    {
      return utp_create_socket(_utp_ctx);
    }

    const char*
    LinkLayer::Name() const
    {
      return "utp";
    }

    std::unique_ptr< ILinkLayer >
    NewServer(Crypto* crypto, const SecretKey& routerEncSecret, GetRCFunc getrc,
              LinkMessageHandler h, SessionEstablishedHandler est,
              SessionRenegotiateHandler reneg, SignBufferFunc sign,
              TimeoutHandler timeout, SessionClosedHandler closed)
    {
      return std::unique_ptr< ILinkLayer >(
          new LinkLayer(crypto, routerEncSecret, getrc, h, sign, est, reneg,
                        timeout, closed));
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

    /// base constructor
    Session::Session(LinkLayer* p)
    {
      m_NextTXMsgID = 0;
      m_NextRXMsgID = 0;
      parent        = p;
      remoteTransportPubKey.Zero();

      SendQueueBacklog = [&]() -> size_t { return sendq.size(); };

      SendKeepAlive = [&]() -> bool {
        auto now = parent->Now();
        if(sendq.size() == 0 && state == eSessionReady && now > lastActive
           && now - lastActive > 5000)
        {
          DiscardMessage msg;
          std::array< byte_t, 128 > tmp;
          llarp_buffer_t buf(tmp);
          if(!msg.BEncode(&buf))
            return false;
          buf.sz  = buf.cur - buf.base;
          buf.cur = buf.base;
          if(!this->QueueWriteBuffers(buf))
            return false;
        }
        return true;
      };
      gotLIM        = false;
      recvBufOffset = 0;
      TimedOut  = std::bind(&Session::IsTimedOut, this, std::placeholders::_1);
      GetPubKey = std::bind(&Session::RemotePubKey, this);
      GetRemoteRC  = [&]() -> RouterContact { return this->remoteRC; };
      GetLinkLayer = std::bind(&Session::GetParent, this);

      lastActive = parent->Now();

      Pump = std::bind(&Session::DoPump, this);
      Tick = std::bind(&Session::TickImpl, this, std::placeholders::_1);
      SendMessageBuffer =
          std::bind(&Session::QueueWriteBuffers, this, std::placeholders::_1);

      IsEstablished = [=]() {
        return this->state == eSessionReady || this->state == eLinkEstablished;
      };

      SendClose          = std::bind(&Session::Close, this);
      GetRemoteEndpoint  = std::bind(&Session::RemoteEndpoint, this);
      RenegotiateSession = std::bind(&Session::Rehandshake, this);
    }

    /// outbound session
    Session::Session(LinkLayer* p, utp_socket* s, const RouterContact& rc,
                     const AddressInfo& addr)
        : Session(p)
    {
      remoteTransportPubKey = addr.pubkey;
      remoteRC              = rc;
      RouterID rid          = remoteRC.pubkey;
      OurCrypto()->shorthash(txKey, llarp_buffer_t(rid));
      rid = p->GetOurRC().pubkey;
      OurCrypto()->shorthash(rxKey, llarp_buffer_t(rid));

      sock = s;
      assert(utp_set_userdata(sock, this) == this);
      assert(s == sock);
      remoteAddr = addr;
      Start      = std::bind(&Session::Connect, this);
      GotLIM = std::bind(&Session::OutboundLIM, this, std::placeholders::_1);
    }

    /// inbound session
    Session::Session(LinkLayer* p, utp_socket* s, const Addr& addr) : Session(p)
    {
      RouterID rid = p->GetOurRC().pubkey;
      OurCrypto()->shorthash(rxKey, llarp_buffer_t(rid));
      remoteRC.Clear();
      sock = s;
      assert(s == sock);
      assert(utp_set_userdata(sock, this) == this);
      remoteAddr = addr;
      Start      = []() {};
      GotLIM     = std::bind(&Session::InboundLIM, this, std::placeholders::_1);
    }

    ILinkLayer*
    Session::GetParent()
    {
      return parent;
    }

    bool
    Session::InboundLIM(const LinkIntroMessage* msg)
    {
      if(gotLIM && remoteRC.pubkey != msg->rc.pubkey)
      {
        Close();
        return false;
      }
      if(!gotLIM)
      {
        remoteRC = msg->rc;
        OurCrypto()->shorthash(txKey, llarp_buffer_t(remoteRC.pubkey));

        if(!DoKeyExchange(std::bind(&Crypto::transport_dh_server, OurCrypto(),
                                    _1, _2, _3, _4),
                          rxKey, msg->N, remoteRC.enckey,
                          parent->TransportSecretKey()))
          return false;

        std::array< byte_t, LinkIntroMessage::MaxSize > tmp;
        llarp_buffer_t buf(tmp);
        LinkIntroMessage replymsg;
        replymsg.rc = parent->GetOurRC();
        if(!replymsg.rc.Verify(OurCrypto(), parent->Now()))
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
        if(!SendMessageBuffer(buf))
        {
          LogError("failed to repl to handshake from ", remoteAddr);
          Close();
          return false;
        }
        if(!DoKeyExchange(std::bind(&Crypto::transport_dh_client, OurCrypto(),
                                    _1, _2, _3, _4),
                          txKey, replymsg.N, remoteRC.enckey,
                          parent->RouterEncryptionSecret()))

          return false;
        LogDebug("Sent reply LIM");
        gotLIM = true;
        EnterState(eSessionReady);
        /// future LIM are used for session renegotiation
        GotLIM = std::bind(&Session::GotSessionRenegotiate, this,
                           std::placeholders::_1);
      }
      return true;
    }

    void
    Session::DoPump()
    {
      // pump write queue
      PumpWrite();
      // prune inbound messages
      PruneInboundMessages(parent->Now());
    }

    bool
    Session::QueueWriteBuffers(const llarp_buffer_t& buf)
    {
      if(sendq.size() >= MaxSendQueueSize)
        return false;
      size_t sz      = buf.sz;
      byte_t* ptr    = buf.base;
      uint32_t msgid = m_NextTXMsgID++;
      while(sz)
      {
        uint32_t s = std::min(FragmentBodyPayloadSize, sz);
        if(!EncryptThenHash(ptr, msgid, s, sz - s))
        {
          LogError("EncryptThenHash failed?!");
          return false;
        }
        LogDebug("encrypted ", s, " bytes");
        ptr += s;
        sz -= s;
      }
      return true;
    }

    bool
    Session::OutboundLIM(const LinkIntroMessage* msg)
    {
      if(gotLIM && remoteRC.pubkey != msg->rc.pubkey)
      {
        return false;
      }
      remoteRC = msg->rc;
      gotLIM   = true;

      if(!DoKeyExchange(std::bind(&Crypto::transport_dh_server, OurCrypto(), _1,
                                  _2, _3, _4),
                        rxKey, msg->N, remoteRC.enckey,
                        parent->RouterEncryptionSecret()))
      {
        Close();
        return false;
      }
      EnterState(eSessionReady);
      /// future LIM are used for session renegotiation
      GotLIM = std::bind(&Session::GotSessionRenegotiate, this,
                         std::placeholders::_1);
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
      if(!msg.rc.Verify(OurCrypto(), parent->Now()))
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
      if(!SendMessageBuffer(buf))
      {
        LogError("failed to send handshake to ", remoteAddr);
        Close();
        return;
      }

      if(!DoKeyExchange(std::bind(&Crypto::transport_dh_client, OurCrypto(), _1,
                                  _2, _3, _4),
                        txKey, msg.N, remoteTransportPubKey,
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
      }
    }

    ILinkSession*
    LinkLayer::NewOutboundSession(const RouterContact& rc,
                                  const AddressInfo& addr)
    {
      return new Session(this, utp_create_socket(_utp_ctx), rc, addr);
    }

    uint64
    LinkLayer::OnRead(utp_callback_arguments* arg)
    {
      Session* self = static_cast< Session* >(utp_get_userdata(arg->socket));

      if(self)
      {
        if(self->state == Session::eClose)
        {
          return 0;
        }
        if(!self->Recv(arg->buf, arg->len))
        {
          LogDebug("recv fail for ", self->remoteAddr);
          self->Close();
          return 0;
        }
        utp_read_drained(arg->socket);
      }
      else
      {
        LogWarn("utp_socket got data with no underlying session");
        utp_close(arg->socket);
      }
      return 0;
    }

    uint64
    LinkLayer::OnStateChange(utp_callback_arguments* arg)
    {
      LinkLayer* l =
          static_cast< LinkLayer* >(utp_context_get_userdata(arg->context));
      Session* session = static_cast< Session* >(utp_get_userdata(arg->socket));
      if(session)
      {
        if(arg->state == UTP_STATE_CONNECT)
        {
          if(session->state == Session::eClose)
          {
            return 0;
          }
          session->OutboundLinkEstablished(l);
        }
        else if(arg->state == UTP_STATE_WRITABLE)
        {
          session->PumpWrite();
        }
        else if(arg->state == UTP_STATE_EOF)
        {
          LogDebug("got eof from ", session->remoteAddr);
          session->Close();
        }
      }
      return 0;
    }

    uint64
    LinkLayer::OnAccept(utp_callback_arguments* arg)
    {
      LinkLayer* self =
          static_cast< LinkLayer* >(utp_context_get_userdata(arg->context));
      Addr remote(*arg->address);
      LogDebug("utp accepted from ", remote);
      Session* session = new Session(self, arg->socket, remote);
      if(!self->PutSession(session))
      {
        session->Close();
        delete session;
      }
      else
        session->OnLinkEstablished(self);

      return 0;
    }

    bool
    Session::EncryptThenHash(const byte_t* ptr, uint32_t msgid, uint16_t length,
                             uint16_t remaining)

    {
      sendq.emplace_back();
      auto& buf = sendq.back();
      vecq.emplace_back();
      auto& vec    = vecq.back();
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
      if(!OurCrypto()->xchacha20(payload, txKey, nonce))
        return false;

      payload.base = noncePtr;
      payload.cur  = payload.base;
      payload.sz   = FragmentBufferSize - FragmentHashSize;
      // key'd hash
      if(!OurCrypto()->hmac(buf.data(), payload, txKey))
        return false;
      return MutateKey(txKey, A);
    }

    void
    Session::EnterState(State st)
    {
      state = st;
      Alive();
      if(st == eSessionReady)
      {
        parent->MapAddr(remoteRC.pubkey.as_array(), this);
        parent->SessionEstablished(remoteRC);
      }
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
      return DoKeyExchange(
          std::bind(&Crypto::transport_dh_server, OurCrypto(), _1, _2, _3, _4),
          rxKey, msg->N, remoteRC.enckey, parent->RouterEncryptionSecret());
    }

    bool
    Session::Rehandshake()
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
      if(!SendMessageBuffer(buf))
        return false;
      // regen our tx Key
      return DoKeyExchange(
          std::bind(&Crypto::transport_dh_client, OurCrypto(), _1, _2, _3, _4),
          txKey, lim.N, remoteRC.enckey, parent->RouterEncryptionSecret());
    }

    bool
    Session::VerifyThenDecrypt(const byte_t* ptr)
    {
      LogDebug("verify then decrypt ", remoteAddr);
      ShortHash digest;

      llarp_buffer_t hbuf(ptr + FragmentHashSize,
                          FragmentBufferSize - FragmentHashSize);
      if(!OurCrypto()->hmac(digest.data(), hbuf, rxKey))
      {
        LogError("keyed hash failed");
        return false;
      }
      ShortHash expected(ptr);
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
      if(!OurCrypto()->xchacha20_alt(out, in, rxKey, ptr + FragmentHashSize))
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
      if(!llarp_buffer_read_uint32(&out, &msgid))
      {
        LogError("failed to read msgid");
        return false;
      }
      // read length and remaining
      uint16_t length, remaining;
      if(!(llarp_buffer_read_uint16(&out, &length)
           && llarp_buffer_read_uint16(&out, &remaining)))
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
        m_RecvMsgs.emplace(msgid, InboundMessage{});
      }

      auto itr = m_RecvMsgs.find(msgid);
      // add message activity
      itr->second.lastActive = parent->Now();
      // append data
      if(!itr->second.AppendData(out.cur, length))
      {
        LogError("inbound buffer is full");
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
        // we done with this guy, prune next tick
        itr->second.lastActive = 0;
        ManagedBuffer buf(itr->second.buffer);
        // resize
        buf.underlying.sz = buf.underlying.cur - buf.underlying.base;
        // rewind
        buf.underlying.cur = buf.underlying.base;
        // process buffer
        LogDebug("got message ", msgid, " from ", remoteAddr);
        return parent->HandleMessage(this, buf.underlying);
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
            // only call shutdown and close when we are actually connected
            utp_shutdown(sock, SHUT_RDWR);
            utp_close(sock);
          }
          LogDebug("utp_close ", remoteAddr);
          utp_set_userdata(sock, nullptr);
        }
      }
      EnterState(eClose);
      sock = nullptr;
    }

    void
    Session::Alive()
    {
      lastActive = parent->Now();
    }

  }  // namespace utp

}  // namespace llarp
