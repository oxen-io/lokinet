#include <llarp/messages/transfer_traffic.hpp>
#include <llarp/routing/handler.hpp>

namespace llarp
{
  namespace routing
  {
    TransferTrafficMessage&
    TransferTrafficMessage::operator=(const TransferTrafficMessage& other)
    {
      S       = other.S;
      version = other.version;
      X       = other.X;
      return *this;
    }

    bool
    TransferTrafficMessage::PutBuffer(llarp_buffer_t buf)
    {
      if(buf.sz > MaxExitMTU)
        return false;
      X.emplace_back(buf.sz);
      memcpy(X.back().data(), buf.base, buf.sz);
      // 8 bytes encoding overhead
      _size += buf.sz + 8;
      return true;
    }

    bool
    TransferTrafficMessage::BEncode(llarp_buffer_t* buf) const
    {
      if(!bencode_start_dict(buf))
        return false;
      if(!BEncodeWriteDictMsgType(buf, "A", "I"))
        return false;
      if(!BEncodeWriteDictInt("S", S, buf))
        return false;
      if(!BEncodeWriteDictInt("V", version, buf))
        return false;
      if(!BEncodeWriteDictList("X", X, buf))
        return false;
      return bencode_end(buf);
    }

    bool
    TransferTrafficMessage::DecodeKey(llarp_buffer_t key, llarp_buffer_t* buf)
    {
      bool read = false;
      if(!BEncodeMaybeReadDictInt("S", S, read, key, buf))
        return false;
      if(!BEncodeMaybeReadDictInt("V", version, read, key, buf))
        return false;
      if(!BEncodeMaybeReadDictList("X", X, read, key, buf))
        return false;
      return read;
    }

    bool
    TransferTrafficMessage::HandleMessage(IMessageHandler* h,
                                          llarp_router* r) const
    {
      return h->HandleTransferTrafficMessage(this, r);
    }

  }  // namespace routing
}  // namespace llarp