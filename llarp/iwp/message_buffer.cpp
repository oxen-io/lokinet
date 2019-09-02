#include <iwp/message_buffer.hpp>
#include <iwp/session.hpp>
#include <crypto/crypto.hpp>

namespace llarp
{
  namespace iwp
  {
    OutboundMessage::OutboundMessage(uint64_t msgid, const llarp_buffer_t &pkt,
                                     llarp_time_t now,
                                     ILinkSession::CompletionHandler handler)
        : m_Size{(uint16_t)std::min(pkt.sz, MAX_LINK_MSG_SIZE)}
        , m_MsgID{msgid}
        , m_Completed{handler}
        , m_StartedAt{now}
    {
      m_Data.Zero();
      std::copy_n(pkt.base, m_Size, m_Data.begin());
      const llarp_buffer_t buf(m_Data.data(), m_Size);
      CryptoManager::instance()->shorthash(digest, buf);
    }

    std::vector< byte_t >
    OutboundMessage::XMIT() const
    {
      std::vector< byte_t > xmit{
          LLARP_PROTO_VERSION, Command::eXMIT, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
      htobe16buf(xmit.data() + 2, m_Size);
      htobe64buf(xmit.data() + 4, m_MsgID);
      std::copy(digest.begin(), digest.end(), std::back_inserter(xmit));
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
        std::function< void(const llarp_buffer_t &) > sendpkt, llarp_time_t now)
    {
      uint16_t idx = 0;
      while(idx < m_Size)
      {
        if(not m_Acks[idx / FragmentSize])
        {
          std::vector< byte_t > frag{LLARP_PROTO_VERSION,
                                     Command::eDATA,
                                     0,
                                     0,
                                     0,
                                     0,
                                     0,
                                     0,
                                     0,
                                     0,
                                     0,
                                     0};
          htobe16buf(frag.data() + 2, idx);
          htobe64buf(frag.data() + 4, m_MsgID);
          const size_t fragsz =
              idx + FragmentSize < m_Size ? FragmentSize : m_Size - idx;
          const auto sz = frag.size();
          frag.resize(sz + fragsz);
          std::copy(m_Data.begin() + idx, m_Data.begin() + idx + fragsz,
                    frag.begin() + sz);
          const llarp_buffer_t pkt(frag);
          sendpkt(pkt);
        }
        idx += FragmentSize;
      }
      m_LastFlush = now;
    }

    bool
    OutboundMessage::IsTransmitted() const
    {
      for(uint16_t idx = 0; idx < m_Size; idx += FragmentSize)
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

    std::vector< byte_t >
    InboundMessage::ACKS() const
    {
      std::vector< byte_t > acks{LLARP_PROTO_VERSION,
                                 Command::eACKS,
                                 0,
                                 0,
                                 0,
                                 0,
                                 0,
                                 0,
                                 0,
                                 0,
                                 uint8_t{(uint8_t)m_Acks.to_ulong()}};

      htobe64buf(acks.data() + 2, m_MsgID);
      return acks;
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
        std::function< void(const llarp_buffer_t &) > sendpkt, llarp_time_t now)
    {
      auto acks = ACKS();
      AddRandomPadding(acks);
      const llarp_buffer_t pkt(acks);
      sendpkt(pkt);
      m_LastACKSent = now;
    }

    bool
    InboundMessage::Verify() const
    {
      ShortHash gotten;
      const llarp_buffer_t buf(m_Data.data(), m_Size);
      CryptoManager::instance()->shorthash(gotten, buf);
      LogDebug("gotten=", gotten.ToHex());
      if(gotten != m_Digset)
      {
        DumpBuffer(buf);
        return false;
      }
      return true;
    }

  }  // namespace iwp
}  // namespace llarp