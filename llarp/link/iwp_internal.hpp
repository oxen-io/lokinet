#ifndef LLARP_LINK_IWP_INTERNAL_HPP
#define LLARP_LINK_IWP_INTERNAL_HPP

#include <constants/link_layer.hpp>
#include <crypto/crypto.hpp>
#include <crypto/encrypted.hpp>
#include <crypto/types.hpp>
#include <link/server.hpp>
#include <link/session.hpp>

#include <array>
#include <bitset>
#include <deque>

namespace llarp
{
  struct Crypto;
  namespace iwp
  {
    using FlowID_t = AlignedBuffer< 32 >;

    using OuterCommand_t = byte_t;

    constexpr OuterCommand_t eOCMD_ObtainFlowID     = 'O';
    constexpr OuterCommand_t eOCMD_GiveFlowID       = 'G';
    constexpr OuterCommand_t eOCMD_Reject           = 'R';
    constexpr OuterCommand_t eOCMD_SessionNegotiate = 'S';
    constexpr OuterCommand_t eOCMD_TransmitData     = 'D';

    using InnerCommand_t = byte_t;

    constexpr InnerCommand_t eICMD_KeepAlive       = 'k';
    constexpr InnerCommand_t eICMD_KeepAliveAck    = 'l';
    constexpr InnerCommand_t eICMD_Congestion      = 'c';
    constexpr InnerCommand_t eICMD_AntiCongestion  = 'd';
    constexpr InnerCommand_t eICMD_Transmit        = 't';
    constexpr InnerCommand_t eICMD_Ack             = 'a';
    constexpr InnerCommand_t eICMD_RotateKeys      = 'r';
    constexpr InnerCommand_t eICMD_UpgradeProtocol = 'u';
    constexpr InnerCommand_t eICMD_VersionUpgrade  = 'v';

    struct OuterMessage
    {
      // required members
      byte_t command;
      FlowID_t flow;

      OuterMessage();
      ~OuterMessage();

      // static members
      static std::array< byte_t, 6 > obtain_flow_id_magic;
      static std::array< byte_t, 6 > give_flow_id_magic;

      void
      CreateReject(const char *msg, llarp_time_t now, const PubKey &pk);

      // optional members follow
      std::array< byte_t, 6 > magic;
      NetID netid;
      // either timestamp or counter
      uint64_t uinteger;
      std::array< byte_t, 14 > reject;
      AlignedBuffer< 24 > N;
      PubKey pubkey;

      std::unique_ptr< AlignedBuffer< 32 > > A;

      static constexpr size_t ipv6_mtu      = 1280;
      static constexpr size_t overhead_size = 16 + 24 + 32;
      static constexpr size_t payload_size  = ipv6_mtu - overhead_size;

      AlignedBuffer< payload_size > X;
      size_t Xsize;
      ShortHash Zhash;
      Signature Zsig;

      /// encode to buffer
      bool
      Encode(llarp_buffer_t *buf) const;

      /// decode from buffer
      bool
      Decode(llarp_buffer_t *buf);

      /// clear members
      void
      Clear();
    };

    struct LinkLayer final : public ILinkLayer
    {
      LinkLayer(Crypto *crypto, const SecretKey &encryptionSecretKey,
                GetRCFunc getrc, LinkMessageHandler h,
                SessionEstablishedHandler established,
                SessionRenegotiateHandler reneg, SignBufferFunc sign,
                TimeoutHandler timeout, SessionClosedHandler closed);

      ~LinkLayer();
      Crypto *const crypto;

      bool
      Start(Logic *l) override;

      ILinkSession *
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
