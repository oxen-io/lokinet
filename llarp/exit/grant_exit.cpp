#include <messages/exit.hpp>

#include <crypto/crypto.hpp>
#include <routing/handler.hpp>

namespace llarp
{
  namespace routing
  {
    bool
    GrantExitMessage::BEncode(llarp_buffer_t* buf) const
    {
      if(!bencode_start_dict(buf))
        return false;
      if(!BEncodeWriteDictMsgType(buf, "A", "G"))
        return false;
      if(!BEncodeWriteDictInt("S", S, buf))
        return false;
      if(!BEncodeWriteDictInt("T", T, buf))
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
    GrantExitMessage::DecodeKey(const llarp_buffer_t& k, llarp_buffer_t* buf)
    {
      bool read = false;
      if(!BEncodeMaybeReadDictInt("S", S, read, k, buf))
        return false;
      if(!BEncodeMaybeReadDictInt("T", T, read, k, buf))
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
    GrantExitMessage::Verify(const llarp::PubKey& pk) const
    {
      std::array< byte_t, 512 > tmp;
      llarp_buffer_t buf(tmp);
      GrantExitMessage copy;
      copy = *this;
      copy.Z.Zero();
      if(!copy.BEncode(&buf))
        return false;
      buf.sz = buf.cur - buf.base;
      return CryptoManager::instance()->verify(pk, buf, Z);
    }

    bool
    GrantExitMessage::Sign(const llarp::SecretKey& sk)
    {
      std::array< byte_t, 512 > tmp;
      llarp_buffer_t buf(tmp);
      Z.Zero();
      Y.Randomize();
      if(!BEncode(&buf))
        return false;
      buf.sz = buf.cur - buf.base;
      return CryptoManager::instance()->sign(Z, sk, buf);
    }

    bool
    GrantExitMessage::HandleMessage(IMessageHandler* h, AbstractRouter* r) const
    {
      return h->HandleGrantExitMessage(*this, r);
    }

  }  // namespace routing
}  // namespace llarp
