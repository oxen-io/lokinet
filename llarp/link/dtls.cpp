#include <link/dtls_internal.hpp>
#include <crypto.hpp>

namespace llarp
{
  namespace dtls
  {
    const mbedtls_ecp_group_id LinkLayer::AllowedCurve[2] = {
        MBEDTLS_ECP_DP_CURVE25519, MBEDTLS_ECP_DP_NONE};
    const mbedtls_md_type_t LinkLayer::AllowedHash[2] = {MBEDTLS_MD_SHA256,
                                                         MBEDTLS_MD_NONE};

    const int LinkLayer::CipherSuite[2] = {
        MBEDTLS_TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256, 0};

    Session::Session(LinkLayer *parent) : ILinkSession()
    {
      m_Parent = parent;
      mbedtls_ssl_config_init(&m_config);
    }

    Session::Session(LinkLayer *parent, const llarp::Addr &from)
        : Session(parent)
    {
      remoteAddr = from;
    }

    Session::Session(LinkLayer *parent, const RouterContact &rc,
                     const AddressInfo &ai)
        : Session(parent)
    {
      remoteRC   = rc;
      remoteAddr = ai;
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
      m_config.ciphersuite_list[0] = LinkLayer::CipherSuite;
      m_config.ciphersuite_list[1] = LinkLayer::CipherSuite;
      m_config.ciphersuite_list[2] = LinkLayer::CipherSuite;
      m_config.ciphersuite_list[3] = LinkLayer::CipherSuite;
      Configure();
    }

    void
    Session::Configure()
    {
      const auto *conf = &m_config;
      mbedtls_ssl_setup(&m_ctx, conf);
    }

    LinkLayer::LinkLayer(llarp::Crypto *c, const byte_t *encryptionSecretKey,
                         const byte_t *identitySecretKey,
                         llarp::GetRCFunc getrc, llarp::LinkMessageHandler h,
                         llarp::SessionEstablishedHandler established,
                         llarp::SessionRenegotiateHandler reneg,
                         llarp::TimeoutHandler timeout,
                         llarp::SessionClosedHandler closed)
        : ILinkLayer(encryptionSecretKey, getrc, h,
                     std::bind(&LinkLayer::SignBuffer, this,
                               std::placeholders::_1, std::placeholders::_2),
                     established, reneg, timeout, closed)
        , crypto(c)
        , m_IdentityKey(identitySecretKey)
    {
    }

    static void
    Debug(void *, int, const char *fname, int line, const char *msg)
    {
      llarp::_Log(eLogDebug, fname, line, msg);
    }

    static int
    Random(void *ctx, unsigned char *buf, size_t sz)
    {
      static_cast< llarp::Crypto * >(ctx)->randbytes(buf, sz);
      return 0;
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

  }  // namespace dtls
}  // namespace llarp
