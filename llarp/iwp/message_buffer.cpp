#include "message_buffer.hpp"
#include "session.hpp"
#include <llarp/crypto/crypto.hpp>

namespace llarp
{
  namespace iwp
  {
    OutboundMessage::OutboundMessage(
        uint64_t msgid,
        ILinkSession::Message_t msg,
        llarp_time_t now,
        ILinkSession::CompletionHandler handler)
        : m_Data{std::move(msg)}
        , m_MsgID{msgid}
        , m_Completed{handler}
        , m_LastFlush{now}
        , m_StartedAt{now}
    {
      const llarp_buffer_t buf(m_Data);
      CryptoManager::instance()->shorthash(m_Digest, buf);
      m_Acks.set(0);
    }

    ILinkSession::Packet_t
    OutboundMessage::XMIT() const
    {
      size_t extra = std::min(m_Data.size(), FragmentSize);
      auto xmit = CreatePacket(Command::eXMIT, 10 + 32 + extra, 0, 0);
      htobe16buf(xmit.data() + CommandOverhead + PacketOverhead, m_Data.size());
      htobe64buf(xmit.data() + 2 + CommandOverhead + PacketOverhead, m_MsgID);
      std::copy_n(
          m_Digest.begin(), m_Digest.size(), xmit.data() + 10 + CommandOverhead + PacketOverhead);
      std::copy_n(m_Data.data(), extra, xmit.data() + 10 + CommandOverhead + PacketOverhead + 32);
      return xmit;
    }

    void
    OutboundMessage::Completed()
    {
      if (m_Completed)
      {
        m_Completed(ILinkSession::DeliveryStatus::eDeliverySuccess);
      }
      m_Completed = nullptr;
    }

    bool
    OutboundMessage::ShouldFlush(llarp_time_t now) const
    {
      return now - m_LastFlush >= TXFlushInterval;
    }

    void
    OutboundMessage::Ack(byte_t bitmask)
    {
      m_Acks = std::bitset<8>(bitmask);
    }

    void
    OutboundMessage::FlushUnAcked(
        std::function<void(ILinkSession::Packet_t)> sendpkt, llarp_time_t now)
    {
      /// overhead for a data packet in plaintext
      static constexpr size_t Overhead = 10;
      uint16_t idx = 0;
      const auto datasz = m_Data.size();
      while (idx < datasz)
      {
        if (not m_Acks[idx / FragmentSize])
        {
          const size_t fragsz = idx + FragmentSize < datasz ? FragmentSize : datasz - idx;
          auto frag = CreatePacket(Command::eDATA, fragsz + Overhead, 0, 0);
          htobe16buf(frag.data() + 2 + PacketOverhead, idx);
          htobe64buf(frag.data() + 4 + PacketOverhead, m_MsgID);
          std::copy(
              m_Data.begin() + idx,
              m_Data.begin() + idx + fragsz,
              frag.data() + PacketOverhead + Overhead + 2);
          sendpkt(std::move(frag));
        }
        idx += FragmentSize;
      }
      m_LastFlush = now;
    }

    bool
    OutboundMessage::IsTransmitted() const
    {
      const auto sz = m_Data.size();
      for (uint16_t idx = 0; idx < sz; idx += FragmentSize)
      {
        if (not m_Acks.test(idx / FragmentSize))
          return false;
      }
      return true;
    }

    bool
    OutboundMessage::IsTimedOut(const llarp_time_t now) const
    {
      // TODO: make configurable by outbound message deliverer
      return now > m_StartedAt && now - m_StartedAt > DeliveryTimeout;
    }

    void
    OutboundMessage::InformTimeout()
    {
      if (m_Completed)
      {
        m_Completed(ILinkSession::DeliveryStatus::eDeliveryDropped);
      }
      m_Completed = nullptr;
    }

    InboundMessage::InboundMessage(uint64_t msgid, uint16_t sz, ShortHash h, llarp_time_t now)
        : m_Data(size_t{sz}), m_Digset{std::move(h)}, m_MsgID(msgid), m_LastActiveAt{now}
    {}

    void
    InboundMessage::HandleData(uint16_t idx, const llarp_buffer_t& buf, llarp_time_t now)
    {
      if (idx + buf.sz > m_Data.size())
      {
        LogWarn("invalid fragment offset ", idx);
        return;
      }
      byte_t* dst = m_Data.data() + idx;
      std::copy_n(buf.base, buf.sz, dst);
      m_Acks.set(idx / FragmentSize);
      LogTrace("got fragment ", idx / FragmentSize);
      m_LastActiveAt = now;
    }

    ILinkSession::Packet_t
    InboundMessage::ACKS() const
    {
      auto acks = CreatePacket(Command::eACKS, 9);
      htobe64buf(acks.data() + CommandOverhead + PacketOverhead, m_MsgID);
      acks[PacketOverhead + 10] = AcksBitmask();
      return acks;
    }

    byte_t
    InboundMessage::AcksBitmask() const
    {
      return byte_t{(byte_t)m_Acks.to_ulong()};
    }

    bool
    InboundMessage::IsCompleted() const
    {
      const auto sz = m_Data.size();
      for (size_t idx = 0; idx < sz; idx += FragmentSize)
      {
        if (not m_Acks.test(idx / FragmentSize))
          return false;
      }
      return true;
    }

    bool
    InboundMessage::ShouldSendACKS(llarp_time_t now) const
    {
      return now > m_LastACKSent + ACKResendInterval;
    }

    bool
    InboundMessage::IsTimedOut(const llarp_time_t now) const
    {
      return now > m_LastActiveAt && now - m_LastActiveAt > DeliveryTimeout;
    }

    void
    InboundMessage::SendACKS(std::function<void(ILinkSession::Packet_t)> sendpkt, llarp_time_t now)
    {
      sendpkt(ACKS());
      m_LastACKSent = now;
    }

    bool
    InboundMessage::Verify() const
    {
      ShortHash gotten;
      const llarp_buffer_t buf(m_Data);
      CryptoManager::instance()->shorthash(gotten, buf);
      return gotten == m_Digset;
    }
  }  // namespace iwp
}  // namespace llarp
