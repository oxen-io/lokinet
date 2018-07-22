#include <llarp/service/protocol.hpp>
#include "buffer.hpp"

namespace llarp
{
  namespace service
  {
    ProtocolMessage::ProtocolMessage()
    {
    }

    ProtocolMessage::~ProtocolMessage()
    {
    }

    bool
    ProtocolMessage::BEncode(llarp_buffer_t* buf) const
    {
      // TODO: implement me
      return false;
    }

    bool
    ProtocolMessage::DecodeKey(llarp_buffer_t key, llarp_buffer_t* val)
    {
      // TODO: implement me
      return false;
    }

    void
    ProtocolMessage::PutBuffer(llarp_buffer_t buf)
    {
      payload.resize(buf.sz);
      memcpy(payload.data(), buf.base, buf.sz);
      payload.shrink_to_fit();
    }

    ProtocolFrame::~ProtocolFrame()
    {
    }

    bool
    ProtocolFrame::BEncode(llarp_buffer_t* buf) const
    {
      if(!bencode_start_dict(buf))
        return false;
      if(!BEncodeWriteDictMsgType(buf, "A", "H"))
        return false;
      if(!BEncodeWriteDictEntry("D", D, buf))
        return false;
      if(S == 0)
      {
        if(!BEncodeWriteDictEntry("H", H, buf))
          return false;
      }
      if(!BEncodeWriteDictEntry("N", N, buf))
        return false;
      if(!BEncodeWriteDictInt(buf, "S", S))
        return false;
      if(!BEncodeWriteDictInt(buf, "V", version))
        return false;
      if(!BEncodeWriteDictEntry("Z", Z, buf))
        return false;
      return bencode_end(buf);
    }

    bool
    ProtocolFrame::DecodeKey(llarp_buffer_t key, llarp_buffer_t* val)
    {
      bool read = false;
      if(!BEncodeMaybeReadDictEntry("D", D, read, key, val))
        return false;
      if(!BEncodeMaybeReadDictEntry("H", H, read, key, val))
        return false;
      if(!BEncodeMaybeReadDictEntry("N", N, read, key, val))
        return false;
      if(!BEncodeMaybeReadDictInt("S", S, read, key, val))
        return false;
      if(!BEncodeMaybeReadVersion("V", version, LLARP_PROTO_VERSION, read, key,
                                  val))
        return false;
      if(!BEncodeMaybeReadDictEntry("Z", Z, read, key, val))
        return false;
      return read;
    }

    bool
    ProtocolFrame::EncryptAndSign(llarp_crypto* crypto,
                                  const ProtocolMessage* msg,
                                  byte_t* sessionKey, byte_t* signingkey)
    {
      // put payload and encrypt
      D = llarp::ConstBuffer(msg->payload);
      memcpy(D.data(), msg->payload.data(), D.size());
      auto dbuf = D.Buffer();
      crypto->xchacha20(*dbuf, sessionKey, N);
      // zero out signature
      Z.Zero();
      // encode
      byte_t tmp[MAX_PROTOCOL_MESSAGE_SIZE];
      auto buf = llarp::StackBuffer< decltype(tmp) >(tmp);
      if(!BEncode(&buf))
        return false;
      // rewind
      buf.sz  = buf.cur - buf.base;
      buf.cur = buf.base;
      // sign
      return crypto->sign(Z, signingkey, buf);
    }

    bool
    ProtocolFrame::Verify(llarp_crypto* crypto, byte_t* signkey)
    {
      // save signature
      llarp::Signature sig = Z;
      // zero out signature for verify
      Z.Zero();
      bool result = false;
      // serialize
      byte_t tmp[MAX_PROTOCOL_MESSAGE_SIZE];
      auto buf = llarp::StackBuffer< decltype(tmp) >(tmp);
      if(BEncode(&buf))
      {
        // rewind buffer
        buf.sz  = buf.cur - buf.base;
        buf.cur = buf.base;
        // verify
        result = crypto->verify(sig, buf, signkey);
      }
      // restore signature
      Z = sig;
      return result;
    }

  }  // namespace service
}  // namespace llarp