#include <llarp/link/utp.hpp>
#include "router.hpp"
#include <llarp/messages/link_intro.hpp>
#include <llarp/buffer.hpp>
#include <llarp/endian.h>
#include <utp.h>

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

#ifdef __int128
    typedef unsigned __int128 Long_t;
#else
    typedef uint64_t Long_t;
#endif

    typedef llarp::AlignedBuffer< FragmentBufferSize, false, Long_t >
        FragmentBuffer;
    typedef llarp::AlignedBuffer< MAX_LINK_MSG_SIZE, false, Long_t >
        MessageBuffer;

    struct LinkLayer;

    struct BaseSession : public ILinkSession
    {
      llarp_router* router = nullptr;
      RouterContact remoteRC;
      Addr remoteAddr;
      SharedSecret sessionKey;
      llarp_time_t lastActive;
      llarp_time_t sessionTimeout = 10 * 1000;
      std::queue< FragmentBuffer > sendq;
      FragmentBuffer recvBuf;
      size_t recvBufOffset = 0;
      MessageBuffer recvMsg;
      size_t recvMsgOffset = 0;

      std::function< void(utp_socket*) > handleLinkEstablish;

      enum State
      {
        eInitial,
        eConnecting,
        eLinkEstablished,  // when utp connection is established
        eCryptoHandshake,  // crypto handshake initiated
        eSessionReady,     // session is ready
        eClose             // utp connection is closed
      };

      State state;

      void
      EnterState(State st)
      {
        state      = st;
        lastActive = llarp_time_now_ms();
      }

      BaseSession();
      virtual ~BaseSession();

      void
      PumpWrite(utp_socket* sock)
      {
        // TODO: use utp_writev
        while(sendq.size())
        {
          auto& front = sendq.front();
          write_ll(sock, front.data(), front.size());
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
      VerifyThenDecrypt(FragmentBuffer& buf)
      {
        ShortHash digest;
        if(!router->crypto.hmac(
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

        router->crypto.xchacha20(body, sessionKey, nonce);

        uint32_t upper, lower;
        if(!(llarp_buffer_read_uint32(&body, &upper)
             && llarp_buffer_read_uint32(&body, &lower)))
          return false;
        bool fragmentEnd = upper == 0;
        if(lower > recvMsgOffset + recvMsg.size())
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
          auto msgbuf   = InitBuffer(recvMsg.data(), recvMsgOffset);
          recvMsgOffset = 0;
          return router->HandleRecvLinkMessageBuffer(this, msgbuf);
        }
        return true;
      }

      void
      EncryptThenHash(FragmentBuffer& buf, const byte_t* ptr, uint32_t sz,
                      bool isLastFragment)

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
        router->crypto.xchacha20(payload, sessionKey, nonce);
        router->crypto.hmac(buf, payload, sessionKey);
      }

      bool
      SendMessageBuffer(llarp_buffer_t buf)
      {
        if(state != eSessionReady)
        {
          llarp::LogError("state is ", state);
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

      bool
      DoKeyExchange(utp_socket* sock, llarp_transport_dh_func dh,
                    const KeyExchangeNonce& n, const PubKey& other,
                    const SecretKey& secret)
      {
        if(!dh(sessionKey, other, secret, n))
        {
          llarp::LogError("key exchange with ", other, " failed");
          Close(sock);
          return false;
        }
        EnterState(eSessionReady);
        return true;
      }

      void
      Tick(llarp_time_t now)
      {
      }

      bool
      SendKeepAlive()
      {
        return true;
      }

      void
      LinkEstablished(utp_socket* s)
      {
        if(handleLinkEstablish)
          handleLinkEstablish(s);
      }

      void
      SendClose()
      {
      }

      void
      Close(utp_socket* sock)
      {
        if(state != eClose)
        {
          utp_shutdown(sock, SHUT_RDWR);
          utp_close(sock);
          utp_set_userdata(sock, nullptr);
        }
        EnterState(eClose);
        sock = nullptr;
      }

      bool
      IsEstablished() const override
      {
        return state == eSessionReady;
      }

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

      void
      RecvHandshake(const void* buf, size_t sz, ILinkLayer* parent,
                    utp_socket* sock)
      {
        if(parent->HasSessionVia(remoteAddr))
        {
          Close(sock);
          return;
        }

        llarp::LogDebug("recv handshake ", sz, " from ", remoteAddr);
        if((recvBuf.size() - recvBufOffset) < sz)
        {
          llarp::LogDebug("handshake too big");
          Close(sock);
          return;
        }
        if(sz <= 8)
        {
          llarp::LogDebug("handshake too small");
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
          return;
        }
        ptr += sizeof(uint32_t);
        uint32_t limsz = bufbe32toh(ptr);
        ptr += sizeof(uint32_t);
        if(((sizeof(uint32_t) * 2) + limsz) > sz)
        {
          // not enough data
          // TODO: don't bail here, continue reading
          Close(sock);
          return;
        }
        LinkIntroMessage msg(this);
        auto mbuf = InitBuffer(ptr, limsz);
        if(!msg.BDecode(&mbuf))
        {
          llarp::LogError("malfromed LIM from ", remoteAddr);
          return;
        }
        if(!msg.HandleMessage(router))
        {
          llarp::LogError("failed to handle LIM from ", remoteAddr);
          Close(sock);
          return;
        }
        remoteRC = msg.rc;
        if(!DoKeyExchange(sock, router->crypto.dh_server, msg.N, msg.rc.enckey,
                          parent->TransportSecretKey()))
          return;
        llarp::LogInfo("we got a new session from ", remoteRC.pubkey);
        parent->MapAddr(remoteAddr, remoteRC.pubkey);
        router->HandleLinkSessionEstablished(remoteRC.pubkey);
      }

      bool
      TimedOut(llarp_time_t now) const
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

      static uint64
      OnRead(utp_callback_arguments* arg)
      {
        BaseSession* self =
            static_cast< BaseSession* >(utp_get_userdata(arg->socket));
        if(self)
        {
          if(self->state == BaseSession::eSessionReady)
            self->Recv(arg->buf, arg->len);
          else
          {
            LinkLayer* parent = static_cast< LinkLayer* >(
                utp_context_get_userdata(arg->context));
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

      static uint64
      SendTo(utp_callback_arguments* arg)
      {
        LinkLayer* l =
            static_cast< LinkLayer* >(utp_context_get_userdata(arg->context));
        llarp_ev_udp_sendto(&l->m_udp, arg->address, arg->buf, arg->len);
        return 0;
      }

      static uint64
      OnConnect(utp_callback_arguments* arg)
      {
        BaseSession* session =
            static_cast< BaseSession* >(utp_get_userdata(arg->socket));
        session->LinkEstablished(arg->socket);
        return 0;
      }

      static uint64
      OnAccept(utp_callback_arguments*);

      LinkLayer(llarp_router* r) : ILinkLayer(r)
      {
        m_router = r;
        _utp_ctx = utp_init(2);
        utp_context_set_userdata(_utp_ctx, this);
        utp_set_callback(_utp_ctx, UTP_SENDTO, &LinkLayer::SendTo);
        utp_set_callback(_utp_ctx, UTP_ON_ACCEPT, &LinkLayer::OnAccept);
        utp_set_callback(_utp_ctx, UTP_ON_CONNECT, &LinkLayer::OnConnect);
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

      bool
      KeyGen(SecretKey& k)
      {
        m_router->crypto.encryption_keygen(k);
        return true;
      }

      ILinkSession*
      NewOutboundSession(const RouterContact& rc, const AddressInfo& addr);

      ILinkSession*
      NewInboundSession(const Addr& addr);

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

    struct OutboundSession : public BaseSession
    {
      utp_socket* sock;
      PubKey remoteTransportPubKey;
      OutboundSession(llarp_router* r, utp_socket* s, const RouterContact& rc,
                      const AddressInfo& addr)
          : BaseSession()
      {
        sock   = s;
        router = r;
        utp_set_userdata(s, this);
        remoteRC              = rc;
        remoteAddr            = addr;
        remoteTransportPubKey = addr.pubkey;
        handleLinkEstablish   = [=](utp_socket* usock) {
          OutboundEstablish(r, usock);
        };
      }

      void
      OutboundEstablish(llarp_router* r, utp_socket* usock)
      {
        sock = usock;
        llarp::LogDebug("link established with ", remoteAddr);
        EnterState(eLinkEstablished);
        KeyExchangeNonce nonce;
        nonce.Randomize();
        SendHandshake(nonce, r, usock);
        EnterState(eCryptoHandshake);
        if(DoKeyExchange(sock, r->crypto.dh_client, nonce,
                         remoteTransportPubKey, r->encryption))
        {
          LinkLayer* parent = static_cast< LinkLayer* >(
              utp_context_get_userdata(utp_get_context(usock)));
          parent->MapAddr(remoteAddr, remoteRC.pubkey);
          r->HandleLinkSessionEstablished(remoteRC.pubkey);
        }
      }

      // send our RC to the remote
      void
      SendHandshake(const KeyExchangeNonce& n, llarp_router* r, utp_socket* s)
      {
        auto buf = InitBuffer(recvBuf.data(), recvBuf.size());
        // fastforward buffer for handshake to fit before
        buf.cur += sizeof(uint32_t) * 2;

        LinkIntroMessage msg;
        msg.rc = r->rc;

        msg.N = n;
        if(!msg.BEncode(&buf))
          return;

        uint32_t sz = buf.cur - buf.base;
        sz -= sizeof(uint32_t) * 2;
        // write handshake header
        buf.cur = buf.base;
        llarp_buffer_put_uint32(&buf, LLARP_PROTO_VERSION);
        llarp_buffer_put_uint32(&buf, sz);
        // send it
        write_ll(s, recvBuf.data(), sz);
        sock = s;
      }

      void
      Start()
      {
        utp_connect(sock, remoteAddr, remoteAddr.SockLen());
        EnterState(eConnecting);
      }
    };

    struct InboundSession : public BaseSession
    {
      utp_socket* sock;
      InboundSession(llarp_router* r, utp_socket* s, const Addr& addr)
          : BaseSession()
      {
        sock   = s;
        router = r;
        utp_set_userdata(sock, this);
        remoteAddr          = addr;
        handleLinkEstablish = [=](utp_socket* usock) {
          EnterState(eLinkEstablished);
          sock = usock;
        };
      }

      void
      Start()
      {
      }
    };

    BaseSession::BaseSession()
    {
      lastActive = llarp_time_now_ms();
    }

    BaseSession::~BaseSession()
    {
    }

    ILinkSession*
    LinkLayer::NewOutboundSession(const RouterContact& rc,
                                  const AddressInfo& addr)
    {
      auto router = m_router;
      if(router)
        return new OutboundSession(router, utp_create_socket(_utp_ctx), rc,
                                   addr);
      return nullptr;
    }

    ILinkSession*
    LinkLayer::NewInboundSession(const Addr& addr)
    {
      return nullptr;
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
      InboundSession* session =
          new InboundSession(self->m_router, arg->socket, remote);
      self->PutSession(remote, session);
      session->LinkEstablished(arg->socket);
      return 0;
    }

  }  // namespace utp

}  // namespace llarp
