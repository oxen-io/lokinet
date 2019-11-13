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
    OutboundMessage::XMIT(Session *s) const
    {
      auto xmit = s->CreatePacket(Command::eXMIT, 10 + 32);
      if(xmit)
      {
        htobe16buf(xmit->Data() + CommandOverhead + PacketOverhead,
                   m_Data.size());
        htobe64buf(xmit->Data() + 2 + CommandOverhead + PacketOverhead,
                   m_MsgID);
        std::copy_n(m_Digest.begin(), m_Digest.size(),
                    xmit->Data() + 10 + CommandOverhead + PacketOverhead);
      }
      return xmit;
    }

    bool
    OutboundMessage::ShouldSendXMIT(llarp_time_t now) const
    {
      return now > m_LastXMIT + Session::TXFlushInterval;
    }

    void
    OutboundMessage::MaybeSendXMIT(Session *s, llarp_time_t now)
    {
      if(ShouldSendXMIT(now))
      {
        auto pkt = XMIT(s);
        if(pkt)
        {
          s->EncryptAndSend(pkt);
          m_LastXMIT = now;
        }
      }
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
      return now > m_LastFlush + Session::TXFlushInterval;
    }

    void
    OutboundMessage::Ack(byte_t bitmask)
    {
      m_Acks = std::bitset< 8 >(bitmask);
    }

    void
    OutboundMessage::FlushUnAcked(Session *s, llarp_time_t now)
    {
      /// overhead for a data packet in plaintext
      static constexpr size_t Overhead = 10;
      uint16_t idx                     = 0;
      const auto datasz                = m_Data.size();
      bool fail                        = false;
      while(idx < datasz)
      {
        if(not m_Acks[idx / FragmentSize])
        {
          const size_t fragsz =
              idx + FragmentSize < datasz ? FragmentSize : datasz - idx;
          auto frag = s->CreatePacket(Command::eDATA, fragsz + Overhead, 0, 0);
          if(frag)
          {
            htobe16buf(frag->Data() + 2 + PacketOverhead, idx);
            htobe64buf(frag->Data() + 4 + PacketOverhead, m_MsgID);
            std::copy(m_Data.begin() + idx, m_Data.begin() + idx + fragsz,
                      frag->Data() + PacketOverhead + Overhead + 2);
            s->EncryptAndSend(frag);
          }
          else
          {
            fail = false;
            break;
          }
        }
        idx += FragmentSize;
      }
      if(not fail)
        m_LastFlush = now;
    }

    bool
    OutboundMessage::IsTransmitted() const
    {
      const auto sz = m_Data.size();
      for(uint16_t idx = 0; idx < sz; idx += FragmentSize)
      {
        if(not m_Acks.test(idx / FragmentSize))
          return false;
      }
      return true;
    }

    bool
    OutboundMessage::IsTimedOut(const llarp_time_t now) const
    {
      return now > m_StartedAt + Session::DeliveryTimeout;
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
        : m_Data(size_t{sz})
        , m_Digset{std::move(h)}
        , m_MsgID{msgid}
        , m_LastActiveAt{now}
    {
    }

    void
    InboundMessage::HandleData(uint16_t idx, const llarp_buffer_t &buf,
                               llarp_time_t now)
    {
      if(idx + buf.sz > m_Data.size())
      {
        LogWarn("invalid fragment offset ", idx);
        return;
      }

      byte_t *dst = m_Data.data() + idx;
      std::copy_n(buf.base, buf.sz, dst);
      m_Acks.set(idx / FragmentSize);
      LogDebug("got fragment ", idx / FragmentSize);
      m_LastActiveAt = now;
    }

    ILinkSession::Packet_t
    InboundMessage::ACKS(Session *s) const
    {
      auto acks = s->CreatePacket(Command::eACKS, 9);
      if(acks)
      {
        htobe64buf(acks->Data() + CommandOverhead + PacketOverhead, m_MsgID);
        acks->Data()[PacketOverhead + 10] = AcksBitmask();
      }
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
      for(size_t idx = 0; idx < sz; idx += FragmentSize)
      {
        if(not m_Acks.test(idx / FragmentSize))
          return false;
      }
      return true;
    }

    bool
    InboundMessage::ShouldSendACKS(llarp_time_t now) const
    {
      return now > m_LastACKSent + Session::ACKResendInterval;
    }

    bool
    InboundMessage::IsTimedOut(const llarp_time_t now) const
    {
      return now > m_LastActiveAt
          && now - m_LastActiveAt > Session::RecievalTimeout;
    }

    void
    InboundMessage::SendACKS(Session *s, llarp_time_t now)
    {
      auto pkt = ACKS(s);
      if(pkt)
      {
        s->EncryptAndSend(pkt);
        m_LastACKSent = now;
      }
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