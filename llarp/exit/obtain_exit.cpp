#include <llarp/messages/exit.hpp>
#include <llarp/routing/handler.hpp>
#include <llarp/link_layer.hpp>

namespace llarp
{
  namespace routing
  {
    ObtainExitMessage&
    ObtainExitMessage::operator=(const ObtainExitMessage& other)
    {
      B       = other.B;
      E       = other.E;
      I       = other.I;
      T       = other.T;
      W       = other.W;
      X       = other.X;
      version = other.version;
      S       = other.S;
      Z       = other.Z;
      return *this;
    }

    bool
    ObtainExitMessage::Sign(llarp_crypto* c, const llarp::SecretKey& sk)
    {
      byte_t tmp[1024] = {0};
      auto buf         = llarp::StackBuffer< decltype(tmp) >(tmp);
      I                = llarp::seckey_topublic(sk);
      Z.Zero();
      if(!BEncode(&buf))
        return false;
      buf.sz = buf.cur - buf.base;
      return c->sign(Z, sk, buf);
    }

    bool
    ObtainExitMessage::Verify(llarp_crypto* c) const
    {
      byte_t tmp[1024] = {0};
      auto buf         = llarp::StackBuffer< decltype(tmp) >(tmp);
      ObtainExitMessage copy;
      copy = *this;
      copy.Z.Zero();
      if(!copy.BEncode(&buf))
        return false;
      // rewind buffer
      buf.sz = buf.cur - buf.base;
      return c->verify(I, buf, Z);
    }

    bool
    ObtainExitMessage::BEncode(llarp_buffer_t* buf) const
    {
      if(!bencode_start_dict(buf))
        return false;
      if(!BEncodeWriteDictMsgType(buf, "A", "X"))
        return false;
      if(!BEncodeWriteDictArray("B", B, buf))
        return false;
      if(!BEncodeWriteDictInt("E", E, buf))
        return false;
      if(!BEncodeWriteDictEntry("I", I, buf))
        return false;
      if(!BEncodeWriteDictInt("S", S, buf))
        return false;
      if(!BEncodeWriteDictInt("T", T, buf))
        return false;
      if(!BEncodeWriteDictInt("V", version, buf))
        return false;
      if(!BEncodeWriteDictArray("W", W, buf))
        return false;
      if(!BEncodeWriteDictInt("X", X, buf))
        return false;
      if(!BEncodeWriteDictEntry("Z", Z, buf))
        return false;
      return bencode_end(buf);
    }

    bool
    ObtainExitMessage::DecodeKey(llarp_buffer_t k, llarp_buffer_t* buf)
    {
      bool read = false;
      if(!BEncodeMaybeReadDictList("B", B, read, k, buf))
        return false;
      if(!BEncodeMaybeReadDictInt("E", E, read, k, buf))
        return false;
      if(!BEncodeMaybeReadDictEntry("I", I, read, k, buf))
        return false;
      if(!BEncodeMaybeReadDictInt("S", S, read, k, buf))
        return false;
      if(!BEncodeMaybeReadDictInt("T", T, read, k, buf))
        return false;
      if(!BEncodeMaybeReadDictInt("V", version, read, k, buf))
        return false;
      if(!BEncodeMaybeReadDictList("W", W, read, k, buf))
        return false;
      if(!BEncodeMaybeReadDictInt("X", X, read, k, buf))
        return false;
      if(!BEncodeMaybeReadDictEntry("Z", Z, read, k, buf))
        return false;
      return read;
    }

    bool
    ObtainExitMessage::HandleMessage(IMessageHandler* h, llarp_router* r) const
    {
      return h->HandleObtainExitMessage(this, r);
    }

  }  // namespace routing
}  // namespace llarp