#ifndef LLARP_IWP_LINKLAYER_HPP
#define LLARP_IWP_LINKLAYER_HPP

#include <constants/link_layer.hpp>
#include <crypto/crypto.hpp>
#include <crypto/encrypted.hpp>
#include <crypto/types.hpp>
#include <link/server.hpp>
#include <util/thread/thread_pool.hpp>
#include <util/buffer_pool.hpp>
#include <unordered_set>

namespace llarp
{
  namespace iwp
  {
    using PacketPool_t = util::BufferPool< PacketEvent, false, 256 >;

    struct LinkLayer final : public ILinkLayer, public PacketPool_t
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

      void
      Pump() override;

      bool
      KeyGen(SecretKey &k) override;

      const char *
      Name() const override;

      uint16_t
      Rank() const override;

      void
      RecvFrom(const Addr &from, byte_t *pkt, size_t sz) override;

      bool
      MapAddr(const RouterID &pk, ILinkSession *s) override;

      void
      UnmapAddr(const Addr &addr);

      template < typename F >
      void
      QueueWork(F &&work)
      {
        m_Worker->addJob(work);
      }

      PacketEvent::Ptr_t
      ObtainPacket(uint64_t sqno, byte_t *ptr, size_t sz);

      void
      ReleasePacket(PacketEvent::Ptr_t p);

     private:
      std::unordered_map< Addr, RouterID, Addr::Hash > m_AuthedAddrs;
      const bool permitInbound;
    };

    using LinkLayer_ptr = std::shared_ptr< LinkLayer >;
  }  // namespace iwp
}  // namespace llarp

#endif
