#include <llarp/messages/exit.hpp>
#include <llarp/routing/handler.hpp>

namespace llarp
{
  namespace routing
  {
    bool
    CloseExitMessage::BEncode(llarp_buffer_t* buf) const
    {
      if(!bencode_start_dict(buf))
        return false;
      if(!BEncodeWriteDictMsgType(buf, "A", "C"))
        return false;
      if(!BEncodeWriteDictInt("S", S, buf))
        return false;
      if(!BEncodeWriteDictInt("V", version, buf))
        return false;
      if(!BEncodeWriteDictEntry("Y", Y, buf))
        return false;
      if(!BEncodeWriteDictEntry("Z", Z, buf))
        return false;
      return bencode_end(buf);
    }

    bool
    CloseExitMessage::DecodeKey(llarp_buffer_t k, llarp_buffer_t* buf)
    {
      bool read = false;
      if(!BEncodeMaybeReadDictInt("S", S, read, k, buf))
        return false;
      if(!BEncodeMaybeReadDictInt("V", version, read, k, buf))
        return false;
      if(!BEncodeMaybeReadDictEntry("Y", Y, read, k, buf))
        return false;
      if(!BEncodeMaybeReadDictEntry("Z", Z, read, k, buf))
        return false;
      return read;
    }

    bool
    CloseExitMessage::Verify(llarp::Crypto* c, const llarp::PubKey& pk) const
    {
      byte_t tmp[512] = {0};
      auto buf        = llarp::StackBuffer< decltype(tmp) >(tmp);
      CloseExitMessage copy;
      copy = *this;
      copy.Z.Zero();
      if(!copy.BEncode(&buf))
        return false;
      buf.sz = buf.cur - buf.base;
      return c->verify(pk, buf, Z);
    }

    bool
    CloseExitMessage::Sign(llarp::Crypto* c, const llarp::SecretKey& sk)
    {
      byte_t tmp[512] = {0};
      auto buf        = llarp::StackBuffer< decltype(tmp) >(tmp);
      Z.Zero();
      Y.Randomize();
      if(!BEncode(&buf))
        return false;
      buf.sz = buf.cur - buf.base;
      return c->sign(Z, sk, buf);
    }

    CloseExitMessage&
    CloseExitMessage::operator=(const CloseExitMessage& other)
    {
      S       = other.S;
      version = other.version;
      Y       = other.Y;
      Z       = other.Z;
      return *this;
    }

    bool
    CloseExitMessage::HandleMessage(IMessageHandler* h, llarp::Router* r) const
    {
      return h->HandleCloseExitMessage(this, r);
    }

  }  // namespace routing
}  // namespace llarp
