#include <llarp/messages/exit.hpp>
#include <llarp/routing/handler.hpp>

namespace llarp
{
  namespace routing
  {
    bool
    UpdateExitMessage::BEncode(llarp_buffer_t* buf) const
    {
      if(!bencode_start_dict(buf))
        return false;
      if(!BEncodeWriteDictMsgType(buf, "A", "V"))
        return false;
      if(!BEncodeWriteDictEntry("P", P, buf))
        return false;
      if(!BEncodeWriteDictInt("S", S, buf))
        return false;
      if(!BEncodeWriteDictInt("T", T, buf))
        return false;
      if(!BEncodeWriteDictInt("V", version, buf))
        return false;
      if(!BEncodeWriteDictEntry("Z", Z, buf))
        return false;
      return bencode_end(buf);
    }

    bool
    UpdateExitMessage::DecodeKey(llarp_buffer_t k, llarp_buffer_t* buf)
    {
      bool read = false;
      if(!BEncodeMaybeReadDictInt("S", S, read, k, buf))
        return false;
      if(!BEncodeMaybeReadDictInt("T", T, read, k, buf))
        return false;
      if(!BEncodeMaybeReadDictInt("V", version, read, k, buf))
        return false;
      if(!BEncodeMaybeReadDictEntry("P", P, read, k, buf))
        return false;
      if(!BEncodeMaybeReadDictEntry("Z", Z, read, k, buf))
        return false;
      return read;
    }

    bool
    UpdateExitMessage::Verify(llarp_crypto* c, const llarp::PubKey& pk) const

    {
      byte_t tmp[128] = {0};
      auto buf        = llarp::StackBuffer< decltype(tmp) >(tmp);
      UpdateExitMessage copy;
      copy = *this;
      copy.Z.Zero();
      if(!copy.BEncode(&buf))
        return false;
      buf.sz = buf.cur - buf.base;
      return c->verify(pk, buf, Z);
    }

    UpdateExitMessage&
    UpdateExitMessage::operator=(const UpdateExitMessage& other)
    {
      P       = other.P;
      S       = other.S;
      T       = other.T;
      version = other.version;
      Z       = other.Z;
      return *this;
    }

    bool
    UpdateExitMessage::Sign(llarp_crypto* c, const llarp::SecretKey& sk)
    {
      byte_t tmp[128] = {0};
      auto buf        = llarp::StackBuffer< decltype(tmp) >(tmp);
      if(!BEncode(&buf))
        return false;
      buf.sz = buf.cur - buf.base;
      return c->sign(Z, sk, buf);
    }

    bool
    UpdateExitMessage::HandleMessage(IMessageHandler* h, llarp_router* r) const
    {
      return h->HandleUpdateExitMessage(this, r);
    }

    bool
    UpdateExitVerifyMessage::BEncode(llarp_buffer_t* buf) const
    {
      if(!bencode_start_dict(buf))
        return false;
      if(!BEncodeWriteDictMsgType(buf, "A", "V"))
        return false;
      if(!BEncodeWriteDictInt("S", S, buf))
        return false;
      if(!BEncodeWriteDictInt("T", T, buf))
        return false;
      if(!BEncodeWriteDictInt("V", version, buf))
        return false;
      return bencode_end(buf);
    }

    bool
    UpdateExitVerifyMessage::DecodeKey(llarp_buffer_t k, llarp_buffer_t* buf)
    {
      bool read = false;
      if(!BEncodeMaybeReadDictInt("S", S, read, k, buf))
        return false;
      if(!BEncodeMaybeReadDictInt("T", T, read, k, buf))
        return false;
      if(!BEncodeMaybeReadDictInt("V", version, read, k, buf))
        return false;
      return read;
    }

    bool
    UpdateExitVerifyMessage::HandleMessage(IMessageHandler* h,
                                           llarp_router* r) const
    {
      return h->HandleUpdateExitVerifyMessage(this, r);
    }

  }  // namespace routing
}  // namespace llarp