#include <llarp/link/utp.hpp>
#include "router.hpp"
#include <llarp/messages/link_intro.hpp>
#include <llarp/messages/discard.hpp>
#include <llarp/buffer.hpp>
#include <llarp/endian.h>
#include <utp.h>
#include <cassert>

#ifdef __linux__
#include <linux/errqueue.h>
#include <netinet/ip_icmp.h>
#endif

namespace llarp
{
  namespace utp
  {
    constexpr size_t FragmentPadMax    = 32;
    constexpr size_t FragmentHashSize  = 32;
    constexpr size_t FragmentNonceSize = 32;
    constexpr size_t FragmentOverheadSize =
        FragmentHashSize + FragmentNonceSize;
    constexpr size_t FragmentBodySize = 512;

    constexpr size_t FragmentBufferSize =
        FragmentHashSize + FragmentNonceSize + FragmentBodySize;
    typedef llarp::AlignedBuffer< FragmentBufferSize > FragmentBuffer;

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
      const static llarp_time_t sessionTimeout = 10 * 1000;
      std::queue< FragmentBuffer > sendq;
      FragmentBuffer recvBuf;
      size_t recvBufOffset;
      MessageBuffer recvMsg;
      size_t recvMsgOffset;
      bool stalled = false;

      void
      Alive();

      /// base
      BaseSession(llarp_router* r);

      /// outbound
      BaseSession(llarp_router* r, utp_socket* s, const RouterContact& rc,
                  const AddressInfo& addr);

      /// inbound
      BaseSession(llarp_router* r, utp_socket* s, const Addr& remote);

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
        Alive();
      }

      void
      EnterState(State st);

      BaseSession();
      virtual ~BaseSession();

      void
      PumpWrite()
      {
        while(sendq.size() && !stalled)
        {
          auto& front = sendq.front();
          if(write_ll(front.data(), front.size()) == 0)
          {
            stalled = true;
            return;
          }
          sendq.pop();
        }
      }

      ssize_t
      write_ll(void* buf, size_t sz)
      {
        if(sock == nullptr)
        {
          llarp::LogWarn("write_ll failed: no socket");
          return 0;
        }
        return utp_write(sock, buf, sz);
      }

      bool
      VerifyThenDecrypt(FragmentBuffer& buf);

      void
      EncryptThenHash(FragmentBuffer& buf, const byte_t* ptr, uint32_t sz,
                      bool isLastFragment);

      bool
      QueueWriteBuffers(llarp_buffer_t buf)
      {
        llarp::LogDebug("write ", buf.sz, " bytes to ", remoteAddr);
        if(state != eSessionReady)
        {
          llarp::LogWarn("failed to send ", buf.sz,
                         " bytes on non ready session state=", state);
          return false;
        }
        size_t sz = buf.sz;
        while(sz)
        {
          uint32_t s = std::min(
              (FragmentBodySize - (llarp_randint() % FragmentPadMax)), sz);
          sendq.emplace();
          EncryptThenHash(sendq.back(), buf.cur, s, ((sz - s) == 0));
          buf.cur += s;
          sz -= s;
        }
        return true;
      }

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
        KeyExchangeNonce nonce;
        nonce.Randomize();
        gotLIM = true;
        EnterState(eCryptoHandshake);
        if(DoKeyExchange(Router()->crypto.transport_dh_client, nonce,
                         remoteTransportPubKey, Router()->encryption))
        {
          SendHandshake(nonce, sock);
          EnterState(eSessionReady);
        }
      }

      // send our RC to the remote
      void
      SendHandshake(const KeyExchangeNonce& n, utp_socket* s)
      {
        auto buf = InitBuffer(recvBuf.data(), recvBuf.size());
        // fastforward buffer for handshake to fit before
        buf.cur += sizeof(uint32_t) * 2;
        byte_t* begin = buf.cur;
        LinkIntroMessage msg;
        msg.rc = Router()->rc;
        msg.N  = n;
        if(!msg.BEncode(&buf))
        {
          llarp::LogError("failed to encode our RC for handshake");
          Close();
          return;
        }

        uint32_t sz = buf.cur - begin;
        llarp::LogDebug("handshake is of size ", sz, " bytes");
        // write handshake header
        buf.cur = buf.base;
        llarp_buffer_put_uint32(&buf, LLARP_PROTO_VERSION);
        llarp_buffer_put_uint32(&buf, sz);
        // send it
        write_ll(recvBuf.data(), sz + (sizeof(uint32_t) * 2));
        sock = s;
      }

      bool
      DoKeyExchange(llarp_transport_dh_func dh, const KeyExchangeNonce& n,
                    const PubKey& other, const SecretKey& secret)
      {
        PubKey us = llarp::seckey_topublic(secret);
        llarp::LogDebug("DH us=", us, " then=", other, " n=", n);
        if(!dh(sessionKey, other, secret, n))
        {
          llarp::LogError("key exchange with ", other, " failed");
          Close();
          return false;
        }
        return true;
      }

      void
      TickImpl(llarp_time_t now)
      {
      }

      void
      Close(bool remove = true);

      void
      RecvHandshake(const void* buf, size_t bufsz, LinkLayer* p, utp_socket* s);

      bool
      Recv(const void* buf, size_t sz)
      {
        const byte_t* ptr = (const byte_t*)buf;
        llarp::LogDebug("utp read ", sz, " from ", remoteAddr);
        size_t s = sz;
        while(s > 0)
        {
          auto dlt = recvBuf.size() - recvBufOffset;
          if(dlt > 0)
          {
            llarp::LogDebug("dlt=", dlt, " offset=", recvBufOffset, " s=", s);
            memcpy(recvBuf.data() + recvBufOffset, ptr, dlt);
            if(!VerifyThenDecrypt(recvBuf))
              return false;
            recvBufOffset = 0;
            ptr += dlt;
            s -= dlt;
          }
          else
          {
            if(!VerifyThenDecrypt(recvBuf))
              return false;
            recvBufOffset = 0;
          }
        }
        Alive();
        return true;
      }

      bool
      IsTimedOut(llarp_time_t now) const
      {
        if(now < lastActive)
          return false;
        return now - lastActive > sessionTimeout;
      }

      const PubKey&
      RemotePubKey() const
      {
        return remoteRC.pubkey;
      }

      const Addr&
      GetRemoteEndpoint() const
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
        if(sendto(l->m_udp.fd, arg->buf, arg->len, arg->flags, arg->address,
                  arg->address_len)
           == -1)
        {
          llarp::LogError("sendto failed: ", strerror(errno));
        }
        return 0;
      }

      static uint64
      OnError(utp_callback_arguments* arg)
      {
        llarp::LogError(utp_error_code_names[arg->error_code]);
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
        router   = r;
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
        utp_context_set_option(_utp_ctx, UTP_SNDBUF, MAX_LINK_MSG_SIZE * 64);
        utp_context_set_option(_utp_ctx, UTP_RCVBUF, MAX_LINK_MSG_SIZE * 64);
        utp_set_callback(
            _utp_ctx, UTP_GET_UDP_MTU,
            [](utp_callback_arguments*) -> uint64 { return 1392; });
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
      Pump()
      {
        ReadPump();
        ILinkLayer::Pump();
      }

      void
      ReadPump()
      {
#ifdef __linux__
        ProcessICMP();
#endif
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
#ifdef __linux__
        ProcessICMP();
#endif
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

    BaseSession::BaseSession(llarp_router* r)
    {
      parent        = nullptr;
      recvMsgOffset = 0;
      SendKeepAlive = [&]() -> bool {
        if(sendq.size() == 0)
        {
          DiscardMessage msg;
          byte_t tmp[128] = {0};
          auto buf        = llarp::StackBuffer< decltype(tmp) >(tmp);
          if(!msg.BEncode(&buf))
            return false;
          buf.sz  = buf.cur - buf.base;
          buf.cur = buf.base;
          if(!this->QueueWriteBuffers(buf))
            return false;
        }
        return true;
      };
      recvBufOffset = 0;
      TimedOut      = [&](llarp_time_t now) -> bool {
        return this->IsTimedOut(now);
      };
      GetPubKey  = std::bind(&BaseSession::RemotePubKey, this);
      lastActive = llarp_time_now_ms();
      Pump       = std::bind(&BaseSession::PumpWrite, this);
      Tick = std::bind(&BaseSession::TickImpl, this, std::placeholders::_1);
      SendMessageBuffer = [&](llarp_buffer_t buf) -> bool {
        return this->QueueWriteBuffers(buf);
      };
      IsEstablished          = [&]() { return this->state == eSessionReady; };
      HandleLinkIntroMessage = [](const LinkIntroMessage*) -> bool {
        return false;
      };
      SendClose = std::bind(&BaseSession::Close, this, false);
    }

    BaseSession::BaseSession(llarp_router* r, utp_socket* s,
                             const RouterContact& rc, const AddressInfo& addr)
        : BaseSession(r)
    {
      remoteRC.Clear();
      remoteTransportPubKey = addr.pubkey;
      remoteRC              = rc;
      sock                  = s;
      assert(utp_set_userdata(sock, this) == this);
      assert(s == sock);
      remoteAddr = addr;
      Start      = std::bind(&BaseSession::Connect, this);
    }

    BaseSession::BaseSession(llarp_router* r, utp_socket* s, const Addr& addr)
        : BaseSession(r)
    {
      remoteRC.Clear();
      sock = s;
      assert(s == sock);
      assert(utp_set_userdata(sock, this) == this);
      remoteAddr = addr;
      Start      = []() {};
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
        utp_close(sock);
        utp_set_userdata(sock, nullptr);
      }
    }

    ILinkSession*
    LinkLayer::NewOutboundSession(const RouterContact& rc,
                                  const AddressInfo& addr)
    {
      return new BaseSession(router, utp_create_socket(_utp_ctx), rc, addr);
    }

    uint64
    LinkLayer::OnRead(utp_callback_arguments* arg)
    {
      LinkLayer* parent =
          static_cast< LinkLayer* >(utp_context_get_userdata(arg->context));
      BaseSession* self =
          static_cast< BaseSession* >(utp_get_userdata(arg->socket));
#ifdef __linux__
      parent->ProcessICMP();
#endif
      if(self)
      {
        if(self->state == BaseSession::eClose)
        {
          return 0;
        }
        else if(self->state == BaseSession::eSessionReady)
        {
          self->Recv(arg->buf, arg->len);
        }
        else if(self->state == BaseSession::eLinkEstablished)
        {
          self->RecvHandshake(arg->buf, arg->len, parent, arg->socket);
        }
      }
      else
      {
        llarp::LogWarn("utp_socket got data with no underlying session");
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
          session->stalled = false;
          session->PumpWrite();
        }
        else if(arg->state == UTP_STATE_EOF)
        {
          session->Close(false);
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

      if(self->HasSessionVia(remote))
      {
        // TODO should we do this?
        llarp::LogWarn(
            "utp socket closed because we already have a session "
            "via ",
            remote);
        utp_close(arg->socket);
        return 0;
      }
      BaseSession* session = new BaseSession(self->router, arg->socket, remote);
      self->PutSession(remote, session);
      session->OnLinkEstablished(self);
      return 0;
    }

    void
    BaseSession::EncryptThenHash(FragmentBuffer& buf, const byte_t* ptr,
                                 uint32_t sz, bool isLastFragment)

    {
      llarp::LogDebug("encrypt then hash ", sz, " bytes last=", isLastFragment);
      buf.Randomize();
      const byte_t* nonce = buf.data() + FragmentHashSize;
      byte_t* body        = buf.data() + FragmentOverheadSize;
      byte_t* base        = body;
      if(isLastFragment)
        htobe32buf(body, 0);
      body += sizeof(uint32_t);
      htobe32buf(body, sz);
      body += sizeof(uint32_t);
      memcpy(body, ptr, sz);
      auto payload = InitBuffer(base, FragmentBodySize);
      parent->router->crypto.xchacha20(payload, sessionKey, nonce);
      payload.base = buf.data() + FragmentHashSize;
      payload.cur  = payload.base;
      payload.sz   = FragmentBufferSize - FragmentHashSize;
      parent->router->crypto.hmac(buf.data(), payload, sessionKey);
    }

    void
    BaseSession::EnterState(State st)
    {
      state = st;
      if(st == eSessionReady)
      {
        llarp::LogInfo("map ", remoteRC.pubkey, " to ", remoteAddr);
        parent->MapAddr(this->remoteAddr, remoteRC.pubkey);
        Router()->HandleLinkSessionEstablished(remoteRC);
      }
      Alive();
    }

    bool
    BaseSession::VerifyThenDecrypt(FragmentBuffer& buf)
    {
      llarp::LogDebug("verify then decrypt ", remoteAddr);
      ShortHash digest;

      auto hbuf = InitBuffer(buf.data() + FragmentHashSize,
                             buf.size() - FragmentHashSize);
      if(!Router()->crypto.hmac(digest, hbuf, sessionKey))
      {
        llarp::LogError("keyed hash failed");
        return false;
      }
      if(memcmp(buf.data(), digest.data(), 32))
      {
        llarp::LogError("Message Integrity Failed: got ", digest, " from ",
                        remoteAddr);
        llarp::DumpBuffer(hbuf);
        return false;
      }
      AlignedBuffer< FragmentNonceSize > nonce(buf.data() + FragmentHashSize);

      auto body = InitBuffer(buf.data() + FragmentOverheadSize,
                             FragmentBufferSize - FragmentOverheadSize);

      Router()->crypto.xchacha20(body, sessionKey, nonce);

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
      byte_t* ptr = recvMsg.data() + recvMsgOffset;
      memcpy(ptr, body.cur, lower);
      recvMsgOffset += lower;
      if(fragmentEnd)
      {
        // got a message
        llarp::LogDebug("end of message from ", remoteAddr);
        auto mbuf   = InitBuffer(recvMsg.data(), recvMsgOffset);
        auto result = Router()->HandleRecvLinkMessageBuffer(this, mbuf);
        if(!result)
        {
          llarp::LogWarn("failed to handle message from ", remoteAddr);
          llarp::DumpBuffer(mbuf);
        }
        recvMsgOffset = 0;
        return result;
      }
      return true;
    }

    void
    BaseSession::RecvHandshake(const void* buf, size_t bufsz, LinkLayer* p,
                               utp_socket* s)
    {
      size_t sz = bufsz;
      parent    = p;
      sock      = s;

      llarp::LogDebug("recv handshake ", sz, " from ", remoteAddr);
      if(recvBuf.size() < sz)
      {
        llarp::LogDebug("handshake too big from ", remoteAddr);
        Close();
        return;
      }
      if(sz <= 8)
      {
        llarp::LogDebug("handshake too small from ", remoteAddr);
        Close();
        return;
      }
      memcpy(recvBuf.data(), buf, sz);

      // process handshake header
      uint8_t* ptr     = recvBuf.data();
      uint32_t version = bufbe32toh(ptr);
      if(version != LLARP_PROTO_VERSION)
      {
        llarp::LogWarn("protocol version missmatch ", version,
                       " != ", LLARP_PROTO_VERSION);
        Close();
        return;
      }
      ptr += sizeof(uint32_t);
      sz -= sizeof(uint32_t);
      uint32_t limsz = bufbe32toh(ptr);
      ptr += sizeof(uint32_t);
      sz -= sizeof(uint32_t);
      if(limsz > sz)
      {
        // not enough data
        // TODO: don't bail here, continue reading
        llarp::LogDebug("not enough data for handshake, want ", limsz,
                        " bytes but got ", sz);
        Close();
        return;
      }
      llarp::LogInfo("read LIM from ", remoteAddr);

      // process LIM
      auto mbuf = InitBuffer(ptr, limsz);
      LinkIntroMessage msg(this);
      if(!msg.BDecode(&mbuf))
      {
        llarp::LogError("Failed to parse LIM from ", remoteAddr);
        llarp::DumpBuffer(mbuf);
        Close();
        return;
      }
      if(!msg.HandleMessage(Router()))
      {
        llarp::LogError("failed to verify signature of rc");
        return;
      }
      remoteRC = msg.rc;
      if(!DoKeyExchange(Router()->crypto.transport_dh_server, msg.N,
                        remoteRC.enckey, parent->TransportSecretKey()))
        return;
      gotLIM = true;
      llarp::LogInfo("we got a new session from ", GetPubKey());
      EnterState(eSessionReady);
      Alive();
    }

    void
    BaseSession::Close(bool remove)
    {
      if(state != eClose)
      {
        utp_shutdown(sock, SHUT_RDWR);
        utp_close(sock);
        utp_set_userdata(sock, nullptr);
      }
      EnterState(eClose);
      sock = nullptr;
      if(remove)
        parent->RemoveSessionVia(remoteAddr);
    }

    void
    BaseSession::Alive()
    {
      lastActive = llarp_time_now_ms();
      if(sock)
        utp_read_drained(sock);
      if(parent)
        utp_issue_deferred_acks(parent->_utp_ctx);
    }

  }  // namespace utp

}  // namespace llarp
