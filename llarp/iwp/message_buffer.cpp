#include <iwp/message_buffer.hpp>
#include <crypto/crypto.hpp>

namespace llarp
{
  namespace iwp
  {
    OutboundMessage::OutboundMessage() : 
      m_Size{0} {}

    OutboundMessage::OutboundMessage(uint64_t msgid, const llarp_buffer_t& pkt,
                      ILinkSession::CompletionHandler handler) : 
                      m_Size{std::min(pkt.sz, MAX_LINK_MSG_SIZE)},
                      m_MsgID{msgid},
                      m_Completed{handler}
                      {
                        m_Data.Zero();
                        std::copy_n(pkt.base, m_Size, m_Data.begin());
                      }

    std::vector<byte_t> 
    OutboundMessage::XMIT() const 
    {
      std::vector<byte_t> xmit{LLARP_PROTO_VERSION, Command::eXMIT, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
      htobe16buf(xmit.data() + 2, m_Size);
      htobe64buf(xmit.data() + 4, m_MsgID);
      const llarp_buffer_t buf{m_Data.data(), m_Size};
      ShortHash H;
      CryptoManager::instance()->shorthash(H, buf);
      std::copy(H.begin(), H.end(), std::back_inserter(xmit));
      LogDebug("xmit H=", H.ToHex());
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
      static constexpr llarp_time_t FlushInterval = 250;
      return now - m_LastFlush >= FlushInterval;
    }

    void 
    OutboundMessage::Ack(byte_t bitmask)
    {
      m_Acks = std::bitset<8>(bitmask);
    }

    void 
    OutboundMessage::FlushUnAcked(std::function<void(const llarp_buffer_t &)> sendpkt, llarp_time_t now)
    {
      uint16_t idx = 0;
      while(idx < m_Size)
      {
        if(not m_Acks[idx / FragmentSize])
        {
          std::vector<byte_t> frag{LLARP_PROTO_VERSION, Command::eDATA, 0,0,0,0,0,0,0,0,0,0};
          htobe16buf(frag.data() + 2, idx);
          htobe64buf(frag.data() + 4, m_MsgID);
          std::copy(m_Data.begin() + idx, m_Data.begin() + idx + FragmentSize, std::back_inserter(frag));
          const llarp_buffer_t pkt{frag};
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

  InboundMessage::InboundMessage() : m_Size{0} {}

  InboundMessage::InboundMessage(uint64_t msgid, uint16_t sz, ShortHash h) : 
    m_Digset{std::move(h)},
    m_Size{sz},
    m_MsgID{msgid}
  {}

    void 
    InboundMessage::HandleData(uint16_t idx, const byte_t * ptr)
    {
      if(idx + FragmentSize > MAX_LINK_MSG_SIZE)
        return;
      auto * dst = m_Data.data() + idx;
      std::copy_n(ptr, FragmentSize, dst);
      m_Acks.set(idx / FragmentSize);
      LogDebug("got fragment ", idx / FragmentSize , " of ", m_Size);
    }


    std::vector<byte_t> 
    InboundMessage::ACKS() const 
    {
      std::vector<byte_t> acks{LLARP_PROTO_VERSION, Command::eACKS, 0, 0, 0, 0, 0, 0, 0, 0, uint8_t{m_Acks.to_ulong()}};
      
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

    void
    InboundMessage::SendACKS(std::function<void(const llarp_buffer_t &)> sendpkt, llarp_time_t now)
    {
      auto acks = ACKS();
      const llarp_buffer_t pkt{acks};
      sendpkt(pkt);
      m_LastACKSent = now;
    }

    bool 
    InboundMessage::Verify() const
    {
      ShortHash gotten;
      const llarp_buffer_t buf{m_Data.data(), m_Size};
      CryptoManager::instance()->shorthash(gotten, buf);
      LogDebug("gotten=",gotten.ToHex());
      if(gotten != m_Digset)
      {
        DumpBuffer(buf);
        return false;
      }
      return true;
    }

  }
}