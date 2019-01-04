#include <link/dtls_internal.hpp>
#include <crypto.hpp>
#include <router.hpp>
#include <endian.hpp>

namespace llarp
{
  namespace dtls
  {
    const mbedtls_ecp_group_id LinkLayer::AllowedCurve[2] = {
        MBEDTLS_ECP_DP_CURVE25519, MBEDTLS_ECP_DP_NONE};
    const int LinkLayer::AllowedHash[2] = {MBEDTLS_MD_SHA256, MBEDTLS_MD_NONE};

    const int LinkLayer::CipherSuite[2] = {
        MBEDTLS_TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256, 0};

    const mbedtls_x509_crt_profile LinkLayer::X509Profile = {
        MBEDTLS_X509_ID_FLAG(MBEDTLS_MD_SHA256),
        MBEDTLS_X509_ID_FLAG(MBEDTLS_PK_ECDSA),
        MBEDTLS_X509_ID_FLAG(MBEDTLS_ECP_DP_CURVE25519), 0};

    static int
    Random(void *ctx, unsigned char *buf, size_t sz)
    {
      static_cast< llarp::Crypto * >(ctx)->randbytes(buf, sz);
      return 0;
    }

    static int
    WriteCookie(void *ctx, unsigned char **p, unsigned char *,
                const unsigned char *info, size_t ilen)
    {
      Session *self = static_cast< Session * >(ctx);
      if(!self->crypto->hmac(*p, llarp::InitBuffer(info, ilen),
                             self->m_Parent->CookieSec()))
        return -1;
      *p += 32;
      return 0;
    }

    static int
    VerifyCookie(void *ctx, const unsigned char *cookie, size_t clen,
                 const unsigned char *info, size_t ilen)
    {
      if(clen != 32)
        return -1;
      Session *self = static_cast< Session * >(ctx);
      ShortHash check;
      if(!self->crypto->hmac(check.data(), llarp::InitBuffer(info, ilen),
                             self->m_Parent->CookieSec()))
        return -1;
      if(memcmp(check.data(), cookie, clen) == 0)
        return 0;
      return -1;
    }

    static int
    InboundVerifyCert(void *, mbedtls_x509_crt *, int, unsigned int *)
    {
      return 0;
    }

    static int
    OutboundVerifyCert(void *, mbedtls_x509_crt *, int, unsigned int *)
    {
      return 0;
    }

    Session::Session(LinkLayer *parent) : ILinkSession(), crypto(parent->crypto)
    {
      m_Parent = parent;
      mbedtls_ssl_config_init(&m_config);
      mbedtls_ssl_conf_transport(&m_config, MBEDTLS_SSL_TRANSPORT_DATAGRAM);
      mbedtls_ssl_conf_authmode(&m_config, MBEDTLS_SSL_VERIFY_REQUIRED);
      mbedtls_ssl_conf_sig_hashes(&m_config, LinkLayer::AllowedHash);
      m_config.p_vrfy         = this;
      m_config.key_cert       = &m_Parent->ourKeys;
      m_config.p_cookie       = this;
      m_config.f_cookie_write = &WriteCookie;
      m_config.f_cookie_check = &VerifyCookie;
    }

    Session::Session(LinkLayer *parent, const llarp::Addr &from)
        : Session(parent)
    {
      remoteAddr      = from;
      m_config.f_vrfy = &InboundVerifyCert;
      byte_t buf[20]  = {0};
      parent->crypto->randbytes(buf, sizeof(buf));
      htobe16buf(buf, from.port());
      memcpy(buf + 2, from.addr6()->s6_addr, 16);
      mbedtls_ssl_set_client_transport_id(&m_ctx, buf, sizeof(buf));
    }

    Session::Session(LinkLayer *parent, const RouterContact &rc,
                     const AddressInfo &ai)
        : Session(parent)
    {
      remoteRC        = rc;
      remoteAddr      = ai;
      m_config.f_vrfy = &OutboundVerifyCert;
    }

    Session::~Session()
    {
      mbedtls_ssl_session_free(&m_session);
      mbedtls_ssl_free(&m_ctx);
      mbedtls_ssl_config_free(&m_config);
    }

    void
    Session::Connect()
    {
      mbedtls_ssl_conf_endpoint(&m_config, MBEDTLS_SSL_IS_CLIENT);
      Configure();
    }

    void
    Session::Accept()
    {
      mbedtls_ssl_conf_endpoint(&m_config, MBEDTLS_SSL_IS_SERVER);
      Configure();
    }

    void
    Session::Configure()
    {
      m_config.ciphersuite_list[0] = LinkLayer::CipherSuite;
      m_config.ciphersuite_list[1] = LinkLayer::CipherSuite;
      m_config.ciphersuite_list[2] = LinkLayer::CipherSuite;
      m_config.ciphersuite_list[3] = LinkLayer::CipherSuite;
      m_config.p_dbg               = nullptr;
      m_config.f_dbg               = &Session::Debug;
      m_config.p_rng               = m_Parent->crypto;
      m_config.f_rng               = &Random;

      const auto *conf = &m_config;
      mbedtls_ssl_setup(&m_ctx, conf);
    }

    void
    Session::Recv_ll(const void *buf, size_t sz)
    {
      ll_recv.emplace_back(sz);
      auto &back = ll_recv.back();
      memcpy(back.data(), buf, sz);
    }

    void
    Session::PumpIO()
    {
      llarp_time_t now = m_Parent->Now();
      if(m_ctx.state == MBEDTLS_SSL_HANDSHAKE_OVER)
      {
        // pump inbound acks
        {
          auto itr = m_Inbound.begin();
          while(itr != m_Inbound.end())
          {
            if(!itr->second.IsExpired(now))
            {
              if(itr->second.IsDone())
              {
                m_Parent->HandleMessage(this, itr->second.msg.as_buffer());
                itr = m_Inbound.erase(itr);
                continue;
              }
              else if(itr->second.ShouldRetransmit(now))
              {
                itr->second.TransmitAcks(&m_ctx, itr->first);
              }
              ++itr;
            }
            else
              itr = m_Inbound.erase(itr);
          }
        }
        // pump outbound fragments
        {
          auto itr = m_Outbound.begin();
          while(itr != m_Outbound.end())
          {
            if(itr->second.IsExpired(now) || itr->second.IsDone())
            {
              itr = m_Outbound.erase(itr);
              continue;
            }
            else if(itr->second.ShouldRetransmit(now))
              itr->second.TransmitUnacked(&m_ctx, itr->first);

            ++itr;
          }
        }
      }
      else
      {
        /// step the handshake
        int res = mbedtls_ssl_handshake_step(&m_ctx);
        switch(res)
        {
          case MBEDTLS_ERR_SSL_WANT_READ:
          case MBEDTLS_ERR_SSL_WANT_WRITE:
          case MBEDTLS_ERR_SSL_ASYNC_IN_PROGRESS:
          case MBEDTLS_ERR_SSL_CRYPTO_IN_PROGRESS:
            break;
          default:
            // drop send queue
            ll_send.clear();
            // drop recv queue
            ll_recv.clear();
            // reset session
            mbedtls_ssl_session_reset(&m_ctx);
            return;
        }
      }
      // low level sendto
      while(ll_send.size())
      {
        m_Parent->SendTo_LL(remoteAddr, llarp::ConstBuffer(ll_send.front()));
        ll_send.pop_front();
      }
    }

    void
    Session::Debug(void *, int, const char *fname, int lineno, const char *msg)
    {
      llarp::_Log(llarp::eLogInfo, fname, lineno, msg);
    }

    LinkLayer::LinkLayer(llarp::Crypto *c, const SecretKey &encryptionSecretKey,
                         const SecretKey &identitySecretKey,
                         llarp::GetRCFunc getrc, llarp::LinkMessageHandler h,
                         llarp::SignBufferFunc sign,
                         llarp::SessionEstablishedHandler established,
                         llarp::SessionRenegotiateHandler reneg,
                         llarp::TimeoutHandler timeout,
                         llarp::SessionClosedHandler closed)
        : llarp::ILinkLayer(encryptionSecretKey, getrc, h, sign, established,
                            reneg, timeout, closed)
        , crypto(c)
        , m_IdentityKey(identitySecretKey)
    {
    }

    bool
    LinkLayer::Start(llarp::Logic *l)
    {
      if(!ILinkLayer::Start(l))
        return false;
      return crypto->shorthash(m_CookieSec, llarp::ConstBuffer(m_IdentityKey));
    }

    void
    LinkLayer::RecvFrom(const llarp::Addr &from, const void *buf, size_t sz)
    {
      auto itr = m_Pending.find(from);
      if(itr == m_Pending.end())
      {
        itr = m_Pending.insert(std::make_pair(from, new Session(this, from)))
                  .first;
        itr->second->Start();
      }
      static_cast< Session * >(itr->second.get())->Recv_ll(buf, sz);
    }

    ILinkSession *
    LinkLayer::NewOutboundSession(const llarp::RouterContact &rc,
                                  const llarp::AddressInfo &ai)
    {
      return new Session(this, rc, ai);
    }

    void
    LinkLayer::Pump()
    {
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
        for(const auto &pk : sessions)
        {
          if(m_AuthedLinks.count(pk) == 0)
          {
            // all sessions were removed
            SessionClosed(pk);
          }
        }
      }
    }

    std::unique_ptr< ILinkLayer >
    NewServerFromRouter(llarp::Router *r)
    {
      return std::unique_ptr< LinkLayer >(new LinkLayer(
          &r->crypto, r->encryption, r->identity,
          std::bind(&llarp::Router::rc, r),
          std::bind(&llarp::Router::HandleRecvLinkMessageBuffer, r,
                    std::placeholders::_1, std::placeholders::_2),
          std::bind(&llarp::Router::Sign, r, std::placeholders::_1,
                    std::placeholders::_2),
          std::bind(&llarp::Router::OnSessionEstablished, r,
                    std::placeholders::_1),
          std::bind(&llarp::Router::CheckRenegotiateValid, r,
                    std::placeholders::_1, std::placeholders::_2),
          std::bind(&llarp::Router::OnConnectTimeout, r, std::placeholders::_1),
          std::bind(&llarp::Router::SessionClosed, r, std::placeholders::_1)));
    }

  }  // namespace dtls
}  // namespace llarp
