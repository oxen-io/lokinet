#include "transfer_traffic_message.hpp"

#include "handler.hpp"
#include <llarp/util/bencode.hpp>
#include <llarp/util/endian.hpp>

namespace llarp
{
  namespace routing
  {
    bool
    TransferTrafficMessage::PutBuffer(const llarp_buffer_t& buf, uint64_t counter)
    {
      if (buf.sz > MaxExitMTU)
        return false;
      X.emplace_back(buf.sz + 8);
      byte_t* ptr = X.back().data();
      htobe64buf(ptr, counter);
      ptr += 8;
      memcpy(ptr, buf.base, buf.sz);
      // 8 bytes encoding overhead and 8 bytes counter
      _size += buf.sz + 16;
      return true;
    }

    bool
    TransferTrafficMessage::BEncode(llarp_buffer_t* buf) const
    {
      if (!bencode_start_dict(buf))
        return false;
      if (!BEncodeWriteDictMsgType(buf, "A", "I"))
        return false;
      if (!BEncodeWriteDictInt("P", protocol, buf))
        return false;
      if (!BEncodeWriteDictInt("S", S, buf))
        return false;
      if (!BEncodeWriteDictInt("V", version, buf))
        return false;
      if (!BEncodeWriteDictList("X", X, buf))
        return false;
      return bencode_end(buf);
    }

    bool
    TransferTrafficMessage::DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* buf)
    {
      bool read = false;
      if (!BEncodeMaybeReadDictInt("S", S, read, key, buf))
        return false;
      if (!BEncodeMaybeReadDictInt("P", protocol, read, key, buf))
        return false;
      if (!BEncodeMaybeReadDictInt("V", version, read, key, buf))
        return false;
      if (!BEncodeMaybeReadDictList("X", X, read, key, buf))
        return false;
      return read or bencode_discard(buf);
    }

    bool
    TransferTrafficMessage::HandleMessage(IMessageHandler* h, AbstractRouter* r) const
    {
      return h->HandleTransferTrafficMessage(*this, r);
    }

  }  // namespace routing
}  // namespace llarp
