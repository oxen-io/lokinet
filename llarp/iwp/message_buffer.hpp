#pragma once
#include <vector>
#include <llarp/constants/link_layer.hpp>
#include <llarp/link/session.hpp>
#include <llarp/util/aligned.hpp>
#include <llarp/util/buffer.hpp>
#include <llarp/util/types.hpp>

namespace llarp
{
  namespace iwp
  {
    enum Command
    {
      /// keep alive message
      ePING = 0,
      /// begin transission
      eXMIT = 1,
      /// fragment data
      eDATA = 2,
      /// acknolege fragments
      eACKS = 3,
      /// negative ack
      eNACK = 4,
      /// multiack
      eMACK = 5,
      /// close session
      eCLOS = 0xff,
    };

    /// max size of data fragments
    static constexpr size_t FragmentSize = 1024;
    /// plaintext header overhead size
    static constexpr size_t CommandOverhead = 2;

    struct OutboundMessage
    {
      OutboundMessage() = default;
      OutboundMessage(
          uint64_t msgid,
          ILinkSession::Message_t data,
          llarp_time_t now,
          ILinkSession::CompletionHandler handler);

      ILinkSession::Message_t m_Data;
      uint64_t m_MsgID = 0;
      std::bitset<MAX_LINK_MSG_SIZE / FragmentSize> m_Acks;
      ILinkSession::CompletionHandler m_Completed;
      llarp_time_t m_LastFlush = 0s;
      ShortHash m_Digest;
      llarp_time_t m_StartedAt = 0s;

      ILinkSession::Packet_t
      XMIT() const;

      void
      Ack(byte_t bitmask);

      void
      FlushUnAcked(std::function<void(ILinkSession::Packet_t)> sendpkt, llarp_time_t now);

      bool
      ShouldFlush(llarp_time_t now) const;

      void
      Completed();

      bool
      IsTransmitted() const;

      bool
      IsTimedOut(llarp_time_t now) const;

      void
      InformTimeout();
    };

    struct InboundMessage
    {
      InboundMessage() = default;
      InboundMessage(uint64_t msgid, uint16_t sz, ShortHash h, llarp_time_t now);

      ILinkSession::Message_t m_Data;
      ShortHash m_Digset;
      uint64_t m_MsgID = 0;
      llarp_time_t m_LastACKSent = 0s;
      llarp_time_t m_LastActiveAt = 0s;
      std::bitset<MAX_LINK_MSG_SIZE / FragmentSize> m_Acks;

      void
      HandleData(uint16_t idx, const llarp_buffer_t& buf, llarp_time_t now);

      bool
      IsCompleted() const;

      bool
      IsTimedOut(llarp_time_t now) const;

      bool
      Verify() const;

      byte_t
      AcksBitmask() const;

      bool
      ShouldSendACKS(llarp_time_t now) const;

      void
      SendACKS(std::function<void(ILinkSession::Packet_t)> sendpkt, llarp_time_t now);

      ILinkSession::Packet_t
      ACKS() const;
    };

  }  // namespace iwp
}  // namespace llarp
