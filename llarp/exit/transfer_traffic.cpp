#include <llarp/messages/transfer_traffic.hpp>
#include <llarp/routing/handler.hpp>

namespace llarp
{
  namespace routing
  {
    bool
    TransferTrafficMessage::Sign(llarp_crypto* c, const llarp::SecretKey& k)
    {
      byte_t tmp[MaxExitMTU + 512] = {0};
      auto buf                     = llarp::StackBuffer< decltype(tmp) >(tmp);
      // zero out sig
      Z.Zero();
      // randomize nonce
      Y.Randomize();
      if(!BEncode(&buf))
        return false;
      // rewind buffer
      buf.sz = buf.cur - buf.base;
      return c->sign(Z, k, buf);
    }

    TransferTrafficMessage&
    TransferTrafficMessage::operator=(const TransferTrafficMessage& other)
    {
      Z       = other.Z;
      Y       = other.Y;
      S       = other.S;
      version = other.version;
      X       = other.X;
      return *this;
    }

    bool
    TransferTrafficMessage::Verify(llarp_crypto* c,
                                   const llarp::PubKey& pk) const
    {
      byte_t tmp[MaxExitMTU + 512] = {0};
      auto buf                     = llarp::StackBuffer< decltype(tmp) >(tmp);
      // make copy
      TransferTrafficMessage copy;
      copy = *this;
      // zero copy's sig
      copy.Z.Zero();
      // encode
      if(!copy.BEncode(&buf))
        return false;
      // rewind buffer
      buf.sz = buf.cur - buf.base;
      // verify signature
      return c->verify(pk, buf, Z);
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
      if(!BEncodeWriteDictEntry("Y", Y, buf))
        return false;
      if(!BEncodeWriteDictEntry("Z", Z, buf))
        return false;
      return bencode_end(buf);
    }

    bool
    TransferTrafficMessage::DecodeKey(llarp_buffer_t key, llarp_buffer_t* buf)
    {
      bool read = false;
      if(!BEncodeMaybeReadDictEntry("Z", Z, read, key, buf))
        return false;
      if(!BEncodeMaybeReadDictEntry("Y", Y, read, key, buf))
        return false;
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