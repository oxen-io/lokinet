#include <llarp/link/utp.hpp>
#include "router.hpp"
#include <llarp/messages/link_intro.hpp>
#include <llarp/messages/discard.hpp>
#include <llarp/buffer.hpp>
#include <llarp/endian.h>
#include <utp.h>
#include <cassert>
#include <tuple>
#include <deque>

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

namespace llarp
{
  namespace utp
  {
    constexpr size_t FragmentHashSize  = 32;
    constexpr size_t FragmentNonceSize = 24;
    constexpr size_t FragmentOverheadSize =
        FragmentHashSize + FragmentNonceSize;
    constexpr size_t FragmentBodyPayloadSize = 1024;
    constexpr size_t FragmentBodyOverhead    = sizeof(uint32_t) * 2;
    constexpr size_t FragmentBodySize =
        FragmentBodyOverhead + FragmentBodyPayloadSize;

    constexpr size_t FragmentBufferSize =
        FragmentOverheadSize + FragmentBodySize;
    typedef llarp::AlignedBuffer< FragmentBufferSize > FragmentBuffer;

    /// maximum size for send queue for a session before we drop
    constexpr size_t MaxSendQueueSize = 1024;

    typedef llarp::AlignedBuffer< MAX_LINK_MSG_SIZE > MessageBuffer;

    struct LinkLayer;

    struct BaseSession : public ILinkSession
    {
      RouterContact remoteRC;
      utp_socket* sock;
      LinkLayer* parent;
      bool gotLIM;
      PubKey remoteTransportPubKey;
      Addr remoteAddr;
      SharedSecret sessionKey;
      llarp_time_t lastActive;
      const static llarp_time_t sessionTimeout = 30 * 1000;

      std::deque< utp_iovec > vecq;
      std::deque< FragmentBuffer > sendq;

      FragmentBuffer recvBuf;
      size_t recvBufOffset;
      MessageBuffer recvMsg;
      size_t recvMsgOffset;
      bool stalled = false;

      void
      Alive();

      /// base
      BaseSession(LinkLayer* p);

      /// outbound
      BaseSession(LinkLayer* p, utp_socket* s, const RouterContact& rc,
                  const AddressInfo& addr);

      /// inbound
      BaseSession(LinkLayer* p, utp_socket* s, const Addr& remote);

      enum State
      {
        eInitial,
        eConnecting,
        eLinkEstablished,  // when utp connection is established
        eCryptoHandshake,  // crypto handshake initiated
        eSessionReady,     // session is ready
        eClose             // utp connection is closed
      };

      llarp_router*
      Router();

      State state;

      void
      OnLinkEstablished(LinkLayer* p)
      {
        parent = p;
        EnterState(eLinkEstablished);
        llarp::LogDebug("link established with ", remoteAddr);
      }

      void
      EnterState(State st);

      BaseSession();
      ~BaseSession();

      void
      PumpWrite()
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
          llarp::LogDebug("utp_writev wrote=", s, " expect=", expect,
                          " to=", remoteAddr);

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

        /*
        while(sendq.size() > 0 && !stalled)
        {
          ssize_t expect = FragmentBufferSize - sendBufOffset;
          ssize_t s      = write_ll(sendq.front().data() + sendBufOffset,
        expect); if(s != expect)
          {
            llarp::LogDebug("stalled at offset=", sendBufOffset, " sz=", s,
                            " to ", remoteAddr);
            sendBufOffset += s;
            stalled = true;
          }
          else
          {
            sendBufOffset = 0;
            sendq.pop_front();
          }
        }
        */
      }

      ssize_t
      write_ll(byte_t* buf, size_t sz)
      {
        if(sock == nullptr)
        {
          llarp::LogWarn("write_ll failed: no socket");
          return 0;
        }
        ssize_t s = utp_write(sock, buf, sz);
        llarp::LogDebug("write_ll ", s, " of ", sz, " bytes to ", remoteAddr);
        return s;
      }

      bool
      VerifyThenDecrypt(byte_t* buf);

      void
      EncryptThenHash(const byte_t* ptr, uint32_t sz, bool isLastFragment);

      bool
      QueueWriteBuffers(llarp_buffer_t buf);

      void
      Connect()
      {
        utp_connect(sock, remoteAddr, remoteAddr.SockLen());
        EnterState(eConnecting);
      }

      void
      OutboundLinkEstablished(LinkLayer* p)
      {
        OnLinkEstablished(p);
        OutboundHandshake();
      }

      // send first message
      void
      OutboundHandshake();

      // mix keys
      bool
      DoKeyExchange(llarp_transport_dh_func dh, const KeyExchangeNonce& n,
                    const PubKey& other, const SecretKey& secret)
      {
        ShortHash t_h;
        AlignedBuffer< 64 > tmp;
        memcpy(tmp.data(), sessionKey, sessionKey.size());
        memcpy(tmp.data() + sessionKey.size(), n, n.size());
        // t_h = HS(K + L.n)
        if(!Router()->crypto.shorthash(t_h, ConstBuffer(tmp)))
        {
          llarp::LogError("failed to mix key to ", remoteAddr);
          return false;
        }

        // K = TKE(a.p, B_a.e, sk, t_h)
        if(!dh(sessionKey, other, secret, t_h))
        {
          llarp::LogError("key exchange with ", other, " failed");
          return false;
        }
        llarp::LogDebug("keys mixed with session to ", remoteAddr);
        return true;
      }

      void
      TickImpl(__attribute__((unused)) llarp_time_t now)
      {
      }

      void
      Close();

      bool
      Recv(const void* buf, size_t sz)
      {
        Alive();
        byte_t* ptr = (byte_t*)buf;
        llarp::LogDebug("utp read ", sz, " from ", remoteAddr);
        size_t s = sz;
        // process leftovers
        if(recvBufOffset)
        {
          auto left = FragmentBufferSize - recvBufOffset;
          if(s >= left)
          {
            // yes it fills it
            llarp::LogDebug("process leftovers, offset=", recvBufOffset,
                            " sz=", s, " left=", left);
            memcpy(recvBuf.data() + recvBufOffset, ptr, left);
            s -= left;
            recvBufOffset = 0;
            ptr += left;
            if(!VerifyThenDecrypt(recvBuf.data()))
              return false;
          }
        }
        // process full fragments
        while(s >= FragmentBufferSize)
        {
          recvBufOffset = 0;
          llarp::LogDebug("process full sz=", s);
          if(!VerifyThenDecrypt(ptr))
            return false;
          ptr += FragmentBufferSize;
          s -= FragmentBufferSize;
        }
        if(s)
        {
          // hold onto leftovers
          llarp::LogDebug("leftovers sz=", s);
          memcpy(recvBuf.data() + recvBufOffset, ptr, s);
          recvBufOffset += s;
        }
        return true;
      }

      bool
      InboundLIM(const LinkIntroMessage* msg);

      bool
      OutboundLIM(const LinkIntroMessage* msg);

      bool
      IsTimedOut(llarp_time_t now) const
      {
        if(state == eClose)
          return true;
        if(now < lastActive)
          return false;
        auto dlt = now - lastActive;
        if(dlt >= sessionTimeout)
        {
          llarp::LogDebug("session timeout reached for ", remoteAddr);
          return true;
        }
        return false;
      }

      const PubKey&
      RemotePubKey() const
      {
        return remoteRC.pubkey;
      }

      const Addr&
      RemoteEndpoint() const
      {
        return remoteAddr;
      }

      void
      MarkEstablished();
    };  // namespace utp

    struct LinkLayer : public ILinkLayer
    {
      utp_context* _utp_ctx = nullptr;
      llarp_router* router  = nullptr;
      static uint64
      OnRead(utp_callback_arguments* arg);

      static uint64
      SendTo(utp_callback_arguments* arg)
      {
        LinkLayer* l =
            static_cast< LinkLayer* >(utp_context_get_userdata(arg->context));
        llarp::LogDebug("utp_sendto ", Addr(*arg->address), " ", arg->len,
                        " bytes");
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
          setsockopt(l->m_udp.fd, IPPROTO_IP, IP_DONTFRAGMENT, &val,
                     sizeof(val));
        }
        else
        {
          int val = 0;
          setsockopt(l->m_udp.fd, IPPROTO_IP, IP_DONTFRAGMENT, &val,
                     sizeof(val));
        }
#else
        if(arg->flags == 2)
        {
          int val = IP_PMTUDISC_DO;
          setsockopt(l->m_udp.fd, IPPROTO_IP, IP_MTU_DISCOVER, &val,
                     sizeof(val));
        }
        else
        {
          int val = IP_PMTUDISC_DONT;
          setsockopt(l->m_udp.fd, IPPROTO_IP, IP_MTU_DISCOVER, &val,
                     sizeof(val));
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
          setsockopt(l->m_udp.fd, IPPROTO_IP, IP_DONTFRAGMENT, &val,
                     sizeof(val));
        }
        else
        {
          char val = 0;
          setsockopt(l->m_udp.fd, IPPROTO_IP, IP_DONTFRAGMENT, &val,
                     sizeof(val));
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
          llarp::LogError("sendto failed: ", buf);
#else
          llarp::LogError("sendto failed: ", strerror(errno));
#endif
        }
        return 0;
      }

      static uint64
      OnError(utp_callback_arguments* arg)
      {
        BaseSession* session =
            static_cast< BaseSession* >(utp_get_userdata(arg->socket));
        if(session)
        {
          session->Router()->OnConnectTimeout(session->GetPubKey());
          llarp::LogError(utp_error_code_names[arg->error_code], " via ",
                          session->remoteAddr);
          session->Close();
        }
        return 0;
      }

      static uint64
      OnStateChange(utp_callback_arguments*);

      static uint64
      OnAccept(utp_callback_arguments*);

      static uint64
      OnLog(utp_callback_arguments* arg)
      {
        llarp::LogDebug(arg->buf);
        return 0;
      }

      LinkLayer(llarp_router* r) : ILinkLayer()
      {
        router = r;
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

      ~LinkLayer()
      {
        utp_destroy(_utp_ctx);
      }

      uint16_t
      Rank() const
      {
        return 1;
      }

      void
      RecvFrom(const Addr& from, const void* buf, size_t sz)
      {
        utp_process_udp(_utp_ctx, (const byte_t*)buf, sz, from, from.SockLen());
      }

#ifdef __linux__
      void
      ProcessICMP()
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

          msg.msg_name = &remote;
          msg.msg_namelen = sizeof(remote);
          msg.msg_iov = &iov;
          msg.msg_iovlen = 1;
          msg.msg_flags = 0;
          msg.msg_control = ancillary_buf;
          msg.msg_controllen = sizeof(ancillary_buf);

          len = recvmsg(m_udp.fd, &msg, MSG_ERRQUEUE | MSG_DONTWAIT);
          if(len < 0)
          {
            if(errno == EAGAIN || errno == EWOULDBLOCK)
              errno = 0;
            else
              llarp::LogError("failed to read icmp for utp ", strerror(errno));
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
            icmp_sin = (struct sockaddr_in*)icmp_addr;
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
      Pump()
      {
        utp_issue_deferred_acks(_utp_ctx);
#ifdef __linux__
        ProcessICMP();
#endif
        std::set< PubKey > sessions;
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
              router->SessionClosed(pk);
            }
          }
        }
      }

      void
      Stop()
      {
      }

      llarp_router*
      GetRouter();

      bool
      KeyGen(SecretKey& k)
      {
        router->crypto.encryption_keygen(k);
        return true;
      }

      void
      Tick(llarp_time_t now)
      {
        utp_check_timeouts(_utp_ctx);
        ILinkLayer::Tick(now);
      }

      ILinkSession*
      NewOutboundSession(const RouterContact& rc, const AddressInfo& addr);

      utp_socket*
      NewSocket()
      {
        return utp_create_socket(_utp_ctx);
      }

      const char*
      Name() const
      {
        return "utp";
      }
    };

    std::unique_ptr< ILinkLayer >
    NewServer(llarp_router* r)
    {
      return std::unique_ptr< LinkLayer >(new LinkLayer(r));
    }

    BaseSession::BaseSession(LinkLayer* p)
    {
      parent = p;
      remoteTransportPubKey.Zero();
      recvMsgOffset = 0;

      SendQueueBacklog = [&]() -> size_t { return sendq.size(); };

      SendKeepAlive = [&]() -> bool {
        auto now = parent->now();
        if(sendq.size() == 0 && state == eSessionReady && now > lastActive
           && now - lastActive > (sessionTimeout / 4))
        {
          DiscardMessage msg;
          byte_t tmp[128] = {0};
          auto buf = llarp::StackBuffer< decltype(tmp) >(tmp);
          if(!msg.BEncode(&buf))
            return false;
          buf.sz = buf.cur - buf.base;
          buf.cur = buf.base;
          if(!this->QueueWriteBuffers(buf))
            return false;
        }
        return true;
      };
      gotLIM = false;
      recvBufOffset = 0;
      TimedOut = [&](llarp_time_t now) -> bool {
        return this->IsTimedOut(now) || this->state == eClose;
      };
      GetPubKey = std::bind(&BaseSession::RemotePubKey, this);
      lastActive = parent->now();
      // Pump       = []() {};
      Pump = std::bind(&BaseSession::PumpWrite, this);
      Tick = std::bind(&BaseSession::TickImpl, this, std::placeholders::_1);
      SendMessageBuffer = std::bind(&BaseSession::QueueWriteBuffers, this,
                                    std::placeholders::_1);

      IsEstablished = [=]() {
        return this->state == eSessionReady || this->state == eLinkEstablished;
      };

      SendClose = std::bind(&BaseSession::Close, this);
      GetRemoteEndpoint = std::bind(&BaseSession::RemoteEndpoint, this);
    }

    BaseSession::BaseSession(LinkLayer* p, utp_socket* s,
                             const RouterContact& rc, const AddressInfo& addr)
        : BaseSession(p)
    {
      remoteRC.Clear();
      remoteTransportPubKey = addr.pubkey;
      remoteRC = rc;
      sock = s;
      assert(utp_set_userdata(sock, this) == this);
      assert(s == sock);
      remoteAddr = addr;
      Start = std::bind(&BaseSession::Connect, this);
      GotLIM =
          std::bind(&BaseSession::OutboundLIM, this, std::placeholders::_1);
    }

    BaseSession::BaseSession(LinkLayer* p, utp_socket* s, const Addr& addr)
        : BaseSession(p)
    {
      p->router->crypto.shorthash(sessionKey,
                                  InitBuffer(p->router->pubkey(), PUBKEYSIZE));
      remoteRC.Clear();
      sock = s;
      assert(s == sock);
      assert(utp_set_userdata(sock, this) == this);
      remoteAddr = addr;
      Start = []() {};
      GotLIM = std::bind(&BaseSession::InboundLIM, this, std::placeholders::_1);
    }

    bool
    BaseSession::InboundLIM(const LinkIntroMessage* msg)
    {
      if(gotLIM && remoteRC.pubkey != msg->rc.pubkey)
      {
        return false;
      }
      remoteRC = msg->rc;
      gotLIM = true;
      if(!DoKeyExchange(Router()->crypto.transport_dh_server, msg->N,
                        remoteRC.enckey, parent->TransportSecretKey()))
        return false;
      EnterState(eSessionReady);
      return true;
    }

    bool
    BaseSession::QueueWriteBuffers(llarp_buffer_t buf)
    {
      if(sendq.size() >= MaxSendQueueSize)
        return false;
      llarp::LogDebug("write ", buf.sz, " bytes to ", remoteAddr);
      lastActive = parent->now();
      size_t sz = buf.sz;
      byte_t* ptr = buf.base;
      while(sz)
      {
        uint32_t s = std::min(FragmentBodyPayloadSize, sz);
        EncryptThenHash(ptr, s, ((sz - s) == 0));
        ptr += s;
        sz -= s;
      }
      return true;
    }

    bool
    BaseSession::OutboundLIM(const LinkIntroMessage* msg)
    {
      if(gotLIM && remoteRC.pubkey != msg->rc.pubkey)
      {
        return false;
      }
      remoteRC = msg->rc;
      gotLIM = true;
      // TODO: update address info pubkey
      return DoKeyExchange(Router()->crypto.transport_dh_client, msg->N,
                           remoteTransportPubKey, Router()->encryption);
    }

    void
    BaseSession::OutboundHandshake()
    {
      // set session key
      Router()->crypto.shorthash(sessionKey, ConstBuffer(remoteRC.pubkey));

      byte_t tmp[LinkIntroMessage::MaxSize];
      auto buf = StackBuffer< decltype(tmp) >(tmp);
      // build our RC
      LinkIntroMessage msg;
      msg.rc = Router()->rc();
      if(!msg.rc.Verify(&Router()->crypto))
      {
        llarp::LogError("our RC is invalid? closing session to", remoteAddr);
        Close();
        return;
      }
      msg.N.Randomize();
      msg.P = DefaultLinkSessionLifetime;
      if(!msg.Sign(&Router()->crypto, Router()->identity))
      {
        llarp::LogError("failed to sign LIM for outbound handshake to ",
                        remoteAddr);
        Close();
        return;
      }
      // encode
      if(!msg.BEncode(&buf))
      {
        llarp::LogError("failed to encode LIM for handshake to ", remoteAddr);
        Close();
        return;
      }
      // rewind
      buf.sz = buf.cur - buf.base;
      buf.cur = buf.base;
      // send
      if(!SendMessageBuffer(buf))
      {
        llarp::LogError("failed to send handshake to ", remoteAddr);
        Close();
        return;
      }
      // mix keys
      if(!DoKeyExchange(Router()->crypto.transport_dh_client, msg.N,
                        remoteTransportPubKey, Router()->encryption))
      {
        llarp::LogError("failed to mix keys for outbound session to ",
                        remoteAddr);
        Close();
        return;
      }
      EnterState(eSessionReady);
    }

    llarp_router*
    BaseSession::Router()
    {
      return parent->router;
    }

    BaseSession::~BaseSession()
    {
      if(sock)
      {
        utp_shutdown(sock, SHUT_RDWR);
        utp_close(sock);
        utp_set_userdata(sock, nullptr);
      }
    }

    ILinkSession*
    LinkLayer::NewOutboundSession(const RouterContact& rc,
                                  const AddressInfo& addr)
    {
      return new BaseSession(this, utp_create_socket(_utp_ctx), rc, addr);
    }

    uint64
    LinkLayer::OnRead(utp_callback_arguments* arg)
    {
      BaseSession* self =
          static_cast< BaseSession* >(utp_get_userdata(arg->socket));

      if(self)
      {
        if(self->state == BaseSession::eClose)
        {
          return 0;
        }
        if(!self->Recv(arg->buf, arg->len))
        {
          llarp::LogDebug("recv fail for ", self->remoteAddr);
          self->Close();
          return 0;
        }
        utp_read_drained(arg->socket);
      }
      else
      {
        llarp::LogWarn("utp_socket got data with no underlying session");
        utp_close(arg->socket);
      }
      return 0;
    }

    uint64
    LinkLayer::OnStateChange(utp_callback_arguments* arg)
    {
      LinkLayer* l =
          static_cast< LinkLayer* >(utp_context_get_userdata(arg->context));
      BaseSession* session =
          static_cast< BaseSession* >(utp_get_userdata(arg->socket));
      if(session)
      {
        if(arg->state == UTP_STATE_CONNECT)
        {
          if(session->state == BaseSession::eClose)
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
          llarp::LogDebug("got eof from ", session->remoteAddr);
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
      llarp::LogDebug("utp accepted from ", remote);
      BaseSession* session = new BaseSession(self, arg->socket, remote);
      self->PutSession(session);
      session->OnLinkEstablished(self);
      return 0;
    }

    void
    BaseSession::EncryptThenHash(const byte_t* ptr, uint32_t sz,
                                 bool isLastFragment)

    {
      sendq.emplace_back();
      auto& buf = sendq.back();
      vecq.emplace_back();
      auto& vec = vecq.back();
      vec.iov_base = buf.data();
      vec.iov_len = FragmentBufferSize;
      llarp::LogDebug("encrypt then hash ", sz, " bytes last=", isLastFragment);
      buf.Randomize();
      byte_t* nonce = buf.data() + FragmentHashSize;
      byte_t* body = nonce + FragmentNonceSize;
      byte_t* base = body;
      if(isLastFragment)
        htobe32buf(body, 0);
      else
        htobe32buf(body, 1);
      body += sizeof(uint32_t);
      htobe32buf(body, sz);
      body += sizeof(uint32_t);
      memcpy(body, ptr, sz);

      auto payload =
          InitBuffer(base, FragmentBufferSize - FragmentOverheadSize);

      // encrypt
      Router()->crypto.xchacha20(payload, sessionKey, nonce);

      payload.base = nonce;
      payload.cur = payload.base;
      payload.sz = FragmentBufferSize - FragmentHashSize;
      // key'd hash
      Router()->crypto.hmac(buf.data(), payload, sessionKey);
    }

    void
    BaseSession::EnterState(State st)
    {
      state = st;
      if(st == eSessionReady)
      {
        parent->MapAddr(remoteRC.pubkey, this);
        Router()->HandleLinkSessionEstablished(remoteRC);
      }
      Alive();
    }

    bool
    BaseSession::VerifyThenDecrypt(byte_t* buf)
    {
      llarp::LogDebug("verify then decrypt ", remoteAddr);
      ShortHash digest;

      auto hbuf = InitBuffer(buf + FragmentHashSize,
                             FragmentBufferSize - FragmentHashSize);
      if(!Router()->crypto.hmac(digest.data(), hbuf, sessionKey))
      {
        llarp::LogError("keyed hash failed");
        return false;
      }
      ShortHash expected(buf);
      if(expected != digest)
      {
        llarp::LogError("Message Integrity Failed: got ", digest, " from ",
                        remoteAddr, " instead of ", expected);
        llarp::DumpBuffer(InitBuffer(buf, FragmentBufferSize));
        return false;
      }

      auto body = InitBuffer(buf + FragmentOverheadSize,
                             FragmentBufferSize - FragmentOverheadSize);

      Router()->crypto.xchacha20(body, sessionKey, buf + FragmentHashSize);

      uint32_t upper, lower;
      if(!(llarp_buffer_read_uint32(&body, &upper)
           && llarp_buffer_read_uint32(&body, &lower)))
        return false;
      bool fragmentEnd = upper == 0;
      llarp::LogDebug("fragment size ", lower, " from ", remoteAddr);
      if(lower + recvMsgOffset > recvMsg.size())
      {
        llarp::LogError("Fragment too big: ", lower, " bytes");
        return false;
      }
      memcpy(recvMsg.data() + recvMsgOffset, body.cur, lower);
      recvMsgOffset += lower;
      if(fragmentEnd)
      {
        // got a message
        llarp::LogDebug("end of message from ", remoteAddr);
        auto mbuf = InitBuffer(recvMsg.data(), recvMsgOffset);
        if(!Router()->HandleRecvLinkMessageBuffer(this, mbuf))
        {
          llarp::LogWarn("failed to handle message from ", remoteAddr);
          llarp::DumpBuffer(mbuf);
        }
        recvMsgOffset = 0;
      }
      return true;
    }

    void
    BaseSession::Close()
    {
      if(state != eClose)
      {
        if(sock)
        {
          utp_shutdown(sock, SHUT_RDWR);
          utp_close(sock);
          llarp::LogDebug("utp_close ", remoteAddr);
          utp_set_userdata(sock, nullptr);
        }
      }
      EnterState(eClose);
      sock = nullptr;
    }

    void
    BaseSession::Alive()
    {
      lastActive = parent->now();
    }

  }  // namespace utp

}  // namespace llarp
