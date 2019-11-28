#ifndef LLARP_IWP_LINKLAYER_HPP
#define LLARP_IWP_LINKLAYER_HPP

#include <constants/link_layer.hpp>
#include <crypto/crypto.hpp>
#include <crypto/encrypted.hpp>
#include <crypto/types.hpp>
#include <link/server.hpp>
#include <util/thread/thread_pool.hpp>

namespace llarp
{
  namespace iwp
  {
    struct LinkLayer final : public ILinkLayer
    {
      LinkLayer(const SecretKey &routerEncSecret, GetRCFunc getrc,
                LinkMessageHandler h, SignBufferFunc sign,
                SessionEstablishedHandler est, SessionRenegotiateHandler reneg,
                TimeoutHandler timeout, SessionClosedHandler closed,
                PumpDoneHandler pumpDone, bool permitInbound);

      ~LinkLayer() override;

      std::shared_ptr< ILinkSession >
      NewOutboundSession(const RouterContact &rc,
                         const AddressInfo &ai) override;

      bool
      KeyGen(SecretKey &k) override;

      const char *
      Name() const override;

      uint16_t
      Rank() const override;

      void
      RecvFrom(const Addr &from, ILinkSession::Packet_t pkt) override;

      bool
      MapAddr(const RouterID &pk, ILinkSession *s) override;

      void
      UnmapAddr(const Addr &addr);

      void
      QueueWork(std::function< void(void) > work);

     private:
      std::unordered_map< Addr, RouterID, Addr::Hash > m_AuthedAddrs;
      const bool permitInbound;
    };

    using LinkLayer_ptr = std::shared_ptr< LinkLayer >;
  }  // namespace iwp
}  // namespace llarp

#endif
