#include <llarp/link/utp.hpp>
#include "router.hpp"
#include <llarp/messages/link_intro.hpp>
#include <llarp/buffer.hpp>
#include <llarp/endian.h>
#include <utp.h>
#include <cassert>

namespace llarp
{
  namespace utp
  {
    constexpr size_t FragmentBufferSize = 1088;
    constexpr size_t FragmentHashSize   = 32;
    constexpr size_t FragmentNonceSize  = 24;
    constexpr size_t FragmentOverheadSize =
        FragmentHashSize + FragmentNonceSize;
    constexpr size_t FragmentBodySize =
        FragmentBufferSize - FragmentOverheadSize;

    typedef llarp::AlignedBuffer< FragmentBufferSize > FragmentBuffer;

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
      std::vector< byte_t > recvMsg;

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
      }

      void
      EnterState(State st);

      BaseSession();
      virtual ~BaseSession();

      void
      PumpWrite(utp_socket* s)
      {
        // TODO: use utp_writev
        while(sendq.size())
        {
          auto& front = sendq.front();
          write_ll(s, front.data(), front.size());
          sendq.pop();
        }
      }

      void
      write_ll(utp_socket* s, void* buf, size_t sz)
      {
        llarp::LogDebug("utp_write ", sz, " bytes to ", remoteAddr);
        ssize_t wrote = utp_write(s, buf, sz);
        if(wrote < 0)
        {
          llarp::LogWarn("utp_write returned ", wrote);
        }
        llarp::LogDebug("utp_write wrote ", wrote, " bytes to ", remoteAddr);
      }

      bool
      VerifyThenDecrypt(FragmentBuffer& buf);

      void
      EncryptThenHash(FragmentBuffer& buf, const byte_t* ptr, uint32_t sz,
                      bool isLastFragment);

      bool
      QueueWriteBuffers(llarp_buffer_t buf)
      {
        if(state != eSessionReady)
        {
          return false;
        }
        size_t sz = buf.sz;
        while(sz)
        {
          uint32_t s =
              std::min((FragmentBodySize - (llarp_randint() % 128)), sz);
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
        SendHandshake(nonce, sock);
        gotLIM = true;
        EnterState(eCryptoHandshake);
        auto router = Router();
        if(DoKeyExchange(sock, router->crypto.dh_client, nonce,
                         remoteTransportPubKey, router->encryption))
        {
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

        msg.N = n;
        if(!msg.BEncode(&buf))
        {
          llarp::LogError("failed to encode our RC for handshake");
          Close(s);
          return;
        }

        uint32_t sz = buf.cur - begin;
        llarp::LogDebug("handshake is of size ", sz, " bytes");
        // write handshake header
        buf.cur = buf.base;
        llarp_buffer_put_uint32(&buf, LLARP_PROTO_VERSION);
        llarp_buffer_put_uint32(&buf, sz);
        // send it
        write_ll(s, recvBuf.data(), sz + (sizeof(uint32_t) * 2));
        sock = s;
      }

      bool
      DoKeyExchange(utp_socket* s, llarp_transport_dh_func dh,
                    const KeyExchangeNonce& n, const PubKey& other,
                    const SecretKey& secret)
      {
        sock = s;
        if(!dh(sessionKey, other, secret, n))
        {
          llarp::LogError("key exchange with ", other, " failed");
          Close(sock);
          return false;
        }
        return true;
      }

      void
      TickImpl(llarp_time_t now)
      {
      }

      bool
      SendKeepAlive()
      {
        return true;
      }

      void
      SendClose()
      {
      }

      void
      Close(utp_socket* s)
      {
        if(state != eClose)
        {
          utp_shutdown(s, SHUT_RDWR);
          utp_close(s);
          utp_set_userdata(s, nullptr);
        }
        EnterState(eClose);
        sock = nullptr;
      }

      void
      RecvHandshake(const void* buf, size_t bufsz, LinkLayer* p, utp_socket* s);

      bool
      Recv(const void* buf, size_t sz)
      {
        const byte_t* ptr = (const byte_t*)buf;
        llarp::LogDebug("utp read ", sz, " from ", remoteAddr);
        while(sz + recvBufOffset > FragmentBufferSize)
        {
          memcpy(recvBuf.data() + recvBufOffset, ptr, FragmentBufferSize);
          sz -= FragmentBufferSize;
          ptr += FragmentBufferSize;
          VerifyThenDecrypt(recvBuf);
          recvBufOffset = 0;
        }
        memcpy(recvBuf.data() + recvBufOffset, ptr, sz);
        if(sz + recvBufOffset <= FragmentBufferSize)
        {
          recvBufOffset = 0;
          VerifyThenDecrypt(recvBuf);
        }
        else
          recvBufOffset += sz;
        return true;
      }

      bool
      IsTimedOut(llarp_time_t now) const
      {
        if(now < lastActive)
          return false;
        return lastActive - now > sessionTimeout;
      }

      const PubKey&
      GetPubKey() const
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
        llarp_ev_udp_sendto(&l->m_udp, arg->address, arg->buf, arg->len);
        return 0;
      }

      static uint64
      OnStateChange(utp_callback_arguments*);

      static uint64
      OnAccept(utp_callback_arguments*);

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
        utp_context_set_option(_utp_ctx, UTP_LOG_NORMAL, 1);
        utp_context_set_option(_utp_ctx, UTP_LOG_MTU, 1);
        utp_context_set_option(_utp_ctx, UTP_LOG_DEBUG, 1);
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

      void
      Pump()
      {
        ILinkLayer::Pump();
        utp_issue_deferred_acks(_utp_ctx);
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

      std::unique_ptr< ILinkSession >
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
      recvBufOffset = 0;
      TimedOut      = [&](llarp_time_t now) -> bool {
        return this->IsTimedOut(now);
      };
      lastActive = llarp_time_now_ms();
      Pump       = [&]() { PumpWrite(this->sock); };
      Tick = std::bind(&BaseSession::TickImpl, this, std::placeholders::_1);
      SendMessageBuffer      = std::bind(&BaseSession::QueueWriteBuffers, this,
                                    std::placeholders::_1);
      IsEstablished          = [&]() { return this->state == eSessionReady; };
      HandleLinkIntroMessage = [](const LinkIntroMessage*) -> bool {
        return false;
      };
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
      remoteAddr = addr;
      Start      = std::bind(&BaseSession::Connect, this);
    }

    BaseSession::BaseSession(llarp_router* r, utp_socket* s, const Addr& addr)
        : BaseSession(r)
    {
      remoteRC.Clear();
      sock = s;
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
    }

    std::unique_ptr< ILinkSession >
    LinkLayer::NewOutboundSession(const RouterContact& rc,
                                  const AddressInfo& addr)
    {
      return std::make_unique< BaseSession >(
          router, utp_create_socket(_utp_ctx), rc, addr);
    }

    uint64
    LinkLayer::OnRead(utp_callback_arguments* arg)
    {
      BaseSession* self =
          static_cast< BaseSession* >(utp_get_userdata(arg->socket));
      if(self)
      {
        assert(self->sock);
        assert(self->sock == arg->socket);
        if(self->state == BaseSession::eSessionReady)
          self->Recv(arg->buf, arg->len);
        else if(self->state == BaseSession::eLinkEstablished)
        {
          LinkLayer* parent =
              static_cast< LinkLayer* >(utp_context_get_userdata(arg->context));
          self->RecvHandshake(arg->buf, arg->len, parent, arg->socket);
        }
        utp_read_drained(arg->socket);
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
      if(arg->state == UTP_STATE_CONNECT)
      {
        assert(session->sock);
        assert(session->sock == arg->socket);
        session->OutboundLinkEstablished(l);
      }
      else if(arg->state == UTP_STATE_EOF)
      {
        session->SendClose();
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
      parent->router->crypto.hmac(buf, payload, sessionKey);
    }

    void
    BaseSession::EnterState(State st)
    {
      state      = st;
      lastActive = llarp_time_now_ms();
      if(st == eSessionReady)
      {
        parent->MapAddr(this->remoteAddr, remoteRC.pubkey);
        Router()->HandleLinkSessionEstablished(remoteRC);
      }
    }

    bool
    BaseSession::VerifyThenDecrypt(FragmentBuffer& buf)
    {
      ShortHash digest;
      if(!Router()->crypto.hmac(
             digest,
             InitBuffer(buf.data() + FragmentHashSize,
                        FragmentBufferSize - FragmentHashSize),
             sessionKey))
      {
        llarp::LogError("keyed hash failed");
        return false;
      }
      if(digest != ShortHash(buf.data()))
      {
        llarp::LogError("Message Integrity Failed");
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
      if(recvMsg.size() + lower > MAX_LINK_MSG_SIZE)
      {
        llarp::LogError("Fragment too big: ", lower, " bytes");
        return false;
      }
      size_t newsz = recvMsg.size() + lower;
      recvMsg.reserve(newsz);
      byte_t* ptr = recvMsg.data() + (newsz - lower);
      memcpy(ptr, body.cur, lower);
      if(fragmentEnd)
      {
        // got a message
        auto mbuf   = Buffer(recvMsg);
        auto result = Router()->HandleRecvLinkMessageBuffer(this, mbuf);
        recvMsg.clear();
        recvMsg.shrink_to_fit();
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
      if(parent->HasSessionVia(remoteAddr))
      {
        llarp::LogDebug("already have session via ", remoteAddr,
                        " so closing before processing handshake");
        Close(sock);
        return;
      }

      llarp::LogDebug("recv handshake ", sz, " from ", remoteAddr);
      if(recvBuf.size() < sz)
      {
        llarp::LogDebug("handshake too big from ", remoteAddr);
        Close(sock);
        return;
      }
      if(sz <= 8)
      {
        llarp::LogDebug("handshake too small from ", remoteAddr);
        Close(sock);
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
        Close(sock);
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
        Close(sock);
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
        Close(sock);
        return;
      }
      if(!msg.HandleMessage(Router()))
      {
        llarp::LogError("failed to verify signature of rc");
        return;
      }
      if(!DoKeyExchange(sock, Router()->crypto.dh_server, msg.N, msg.rc.enckey,
                        parent->TransportSecretKey()))
        return;
      remoteRC = msg.rc;
      gotLIM   = true;
      llarp::LogInfo("we got a new session from ", GetPubKey());
      EnterState(eSessionReady);
    }

  }  // namespace utp

}  // namespace llarp
