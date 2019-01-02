#ifndef LLARP_LINK_DTLS_INTERNAL_HPP
#define LLARP_LINK_DTLS_INTERNAL_HPP
#include <link/server.hpp>
#include <link/session.hpp>

#include <mbedtls/ssl.h>
#include <mbedtls/ecp.h>
#include <mbedtls/md.h>

namespace llarp
{
  namespace dtls
  {
    struct LinkLayer;

    struct Session final : public llarp::ILinkSession
    {
      /// base
      Session(LinkLayer *parent);
      /// inbound
      Session(LinkLayer *parent, const llarp::Addr &from);
      /// outbound
      Session(LinkLayer *parent, const RouterContact &rc,
              const AddressInfo &ai);
      ~Session();

      void
      PumpIO();

      void
      TickIO();

      bool
      QueueBuffer(llarp_buffer_t buf);

      /// inbound start
      void
      Accept();

      /// sendclose
      void
      Close();

      /// outbound start
      void
      Connect();

      // set tls config
      void
      Configure();

      /// ll recv
      void
      Recv_ll(const void *buf, size_t sz);

      static int
      ssl_recv(void *ctx, unsigned char *buf, size_t sz);

      static int
      ssl_send(void *ctx, const unsigned char *buf, size_t sz);

      mbedtls_ssl_config m_config;
      mbedtls_ssl_context m_ctx;
      mbedtls_ssl_session m_session;
      LinkLayer *m_Parent;
      llarp::RouterContact remoteRC;
      llarp::Addr remoteAddr;
    };

    struct LinkLayer final : public llarp::ILinkLayer
    {
      LinkLayer(llarp::Crypto *crypto, const byte_t *encryptionSecretKey,
                const byte_t *identitySecretKey, llarp::GetRCFunc getrc,
                llarp::LinkMessageHandler h,
                llarp::SessionEstablishedHandler established,
                llarp::SessionRenegotiateHandler reneg,
                llarp::TimeoutHandler timeout,
                llarp::SessionClosedHandler closed);

      ~LinkLayer();
      llarp::Crypto *const crypto;

      static const mbedtls_ecp_group_id AllowedCurve[2];
      static const mbedtls_md_type_t AllowedHash[2];
      static const int CipherSuite[2];

      bool
      Start(llarp::Logic *l) override;

      ILinkSession *
      NewOutboundSession(const llarp::RouterContact &rc,
                         const llarp::AddressInfo &ai) override;

      void
      Stop() override;

      void
      Pump() override;

     private:
      bool
      SignBuffer(llarp::Signature &sig, llarp_buffer_t buf) const
      {
        return crypto->sign(sig, m_IdentityKey, buf);
      }
      const byte_t *m_IdentityKey;

      /// handle ll recv
      void
      RecvFrom(const llarp::Addr &from, const void *buf, size_t sz) override;
    };
  }  // namespace dtls
}  // namespace llarp

#endif
