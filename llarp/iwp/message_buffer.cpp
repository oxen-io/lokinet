#include <iwp/message_buffer.hpp>
#include <iwp/session.hpp>
#include <crypto/crypto.hpp>

namespace llarp
{
  namespace iwp
  {
    OutboundMessage::OutboundMessage(uint64_t msgid,
                                     ILinkSession::Message_t msg,
                                     llarp_time_t now,
                                     ILinkSession::CompletionHandler handler)
        : m_Data{std::move(msg)}
        , m_MsgID{msgid}
        , m_Completed{handler}
        , m_StartedAt{now}
    {
      const llarp_buffer_t buf(m_Data);
      CryptoManager::instance()->shorthash(m_Digest, buf);
    }

    ILinkSession::Packet_t
    OutboundMessage::XMIT() const
    {
      ILinkSession::Packet_t xmit(12 + 32);
      xmit[0] = LLARP_PROTO_VERSION;
      xmit[1] = Command::eXMIT;
      htobe16buf(xmit.data() + 2, m_Data.size());
      htobe64buf(xmit.data() + 4, m_MsgID);
      std::copy_n(m_Digest.begin(), m_Digest.size(), xmit.begin() + 12);
      return xmit;
    }

    void
    OutboundMessage::Completed()
    {
      if(m_Completed)
      {
        m_Completed(ILinkSession::DeliveryStatus::eDeliverySuccess);
      }
      m_Completed = nullptr;
    }

    bool
    OutboundMessage::ShouldFlush(llarp_time_t now) const
    {
      return now - m_LastFlush >= Session::TXFlushInterval;
    }

    void
    OutboundMessage::Ack(byte_t bitmask)
    {
      m_Acks = std::bitset< 8 >(bitmask);
    }

    void
    OutboundMessage::FlushUnAcked(
        std::function< void(ILinkSession::Packet_t) > sendpkt, llarp_time_t now)
    {
      /// overhead for a data packet in plaintext
      static constexpr size_t Overhead = 12;
      uint16_t idx                     = 0;
      const auto datasz                = m_Data.size();
      while(idx < datasz)
      {
        if(not m_Acks[idx / FragmentSize])
        {
          const size_t fragsz =
              idx + FragmentSize < datasz ? FragmentSize : datasz - idx;
          ILinkSession::Packet_t frag(fragsz + Overhead);

          frag[0] = LLARP_PROTO_VERSION;
          frag[1] = Command::eDATA;
          htobe16buf(frag.data() + 2, idx);
          htobe64buf(frag.data() + 4, m_MsgID);
          std::copy(m_Data.begin() + idx, m_Data.begin() + idx + fragsz,
                    frag.begin() + Overhead);
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
      for(uint16_t idx = 0; idx < sz; idx += FragmentSize)
      {
        if(!m_Acks.test(idx / FragmentSize))
          return false;
      }
      return true;
    }

    bool
    OutboundMessage::IsTimedOut(const llarp_time_t now) const
    {
      // TODO: make configurable by outbound message deliverer
      return now > m_StartedAt && now - m_StartedAt > Session::DeliveryTimeout;
    }

    void
    OutboundMessage::InformTimeout()
    {
      if(m_Completed)
      {
        m_Completed(ILinkSession::DeliveryStatus::eDeliveryDropped);
      }
      m_Completed = nullptr;
    }

    InboundMessage::InboundMessage(uint64_t msgid, uint16_t sz, ShortHash h,
                                   llarp_time_t now)
        : m_Digset{std::move(h)}
        , m_Size{sz}
        , m_MsgID{msgid}
        , m_LastActiveAt{now}
    {
    }

    void
    InboundMessage::HandleData(uint16_t idx, const llarp_buffer_t &buf,
                               llarp_time_t now)
    {
      if(idx + buf.sz > m_Data.size())
        return;
      auto *dst = m_Data.data() + idx;
      std::copy_n(buf.base, buf.sz, dst);
      m_Acks.set(idx / FragmentSize);
      LogDebug("got fragment ", idx / FragmentSize, " of ", m_Size);
      m_LastActiveAt = now;
    }

    ILinkSession::Packet_t
    InboundMessage::ACKS() const
    {
      ILinkSession::Packet_t acks(9);
      acks[0] = LLARP_PROTO_VERSION;
      acks[1] = Command::eACKS;
      htobe64buf(acks.data() + 2, m_MsgID);
      acks[8] = AcksBitmask();
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
      for(uint16_t idx = 0; idx < m_Size; idx += FragmentSize)
      {
        if(!m_Acks.test(idx / FragmentSize))
          return false;
      }
      return true;
    }

    bool
    InboundMessage::ShouldSendACKS(llarp_time_t now) const
    {
      return now - m_LastACKSent > 1000 || IsCompleted();
    }

    bool
    InboundMessage::IsTimedOut(const llarp_time_t now) const
    {
      return now > m_LastActiveAt
          && now - m_LastActiveAt > Session::DeliveryTimeout;
    }

    void
    InboundMessage::SendACKS(
        std::function< void(ILinkSession::Packet_t) > sendpkt, llarp_time_t now)
    {
      auto acks = ACKS();
      AddRandomPadding(acks);
      sendpkt(std::move(acks));
      m_LastACKSent = now;
    }

    bool
    InboundMessage::Verify() const
    {
      ShortHash gotten;
      const llarp_buffer_t buf(m_Data.data(), m_Size);
      CryptoManager::instance()->shorthash(gotten, buf);
      return gotten == m_Digset;
    }

  }  // namespace iwp
}  // namespace llarp