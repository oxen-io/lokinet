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
      X.resize(buf.sz);
      memcpy(X.data(), buf.base, buf.sz);
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

      if(!bencode_write_bytestring(buf, "X", 1))
        return false;
      if(!bencode_write_bytestring(buf, X.data(), X.size()))
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
      if(llarp_buffer_eq(key, "X"))
      {
        llarp_buffer_t strbuf;
        if(!bencode_read_string(buf, &strbuf))
          return false;
        return PutBuffer(strbuf);
      }
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