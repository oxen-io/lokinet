#ifndef LLARP_IWP_LINKLAYER_HPP
#define LLARP_IWP_LINKLAYER_HPP

#include <constants/link_layer.hpp>
#include <crypto/crypto.hpp>
#include <crypto/encrypted.hpp>
#include <crypto/types.hpp>
#include <link/server.hpp>
#include <iwp/outermessage.hpp>

namespace llarp
{
  namespace iwp
  {
    struct LinkLayer final : public ILinkLayer
    {
      LinkLayer(const SecretKey &encryptionSecretKey, GetRCFunc getrc,
                LinkMessageHandler h, SessionEstablishedHandler established,
                SessionRenegotiateHandler reneg, SignBufferFunc sign,
                TimeoutHandler timeout, SessionClosedHandler closed);

      ~LinkLayer() override;

      bool
      Start(std::shared_ptr< Logic > l) override;

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

      /// verify that a new flow id matches addresses and pubkey
      bool
      VerifyFlowID(const PubKey &pk, const Addr &from,
                   const FlowID_t &flow) const;

      void
      RecvFrom(const Addr &from, const void *buf, size_t sz) override;

     private:
      bool
      GenFlowIDFor(const PubKey &pk, const Addr &from, FlowID_t &flow) const;

      bool
      ShouldSendFlowID(const Addr &from) const;

      void
      SendReject(const Addr &to, const char *msg);

      void
      SendFlowID(const Addr &to, const FlowID_t &flow);

      using ActiveFlows_t =
          std::unordered_map< FlowID_t, RouterID, FlowID_t::Hash >;

      ActiveFlows_t m_ActiveFlows;

      using PendingFlows_t = std::unordered_map< Addr, FlowID_t, Addr::Hash >;
      /// flows that are pending authentication
      PendingFlows_t m_PendingFlows;

      /// cookie used in flow id computation
      AlignedBuffer< 32 > m_FlowCookie;

      OuterMessage m_OuterMsg;
    };
  }  // namespace iwp
}  // namespace llarp

#endif
