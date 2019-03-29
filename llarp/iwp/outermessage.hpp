#ifndef LLARP_IWP_OUTERMESSAGE_HPP
#define LLARP_IWP_OUTERMESSAGE_HPP

#include <crypto/types.hpp>
#include <router_contact.hpp>
#include <util/aligned.hpp>

#include <array>

namespace llarp
{
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
  }  // namespace iwp
}  // namespace llarp
#endif
