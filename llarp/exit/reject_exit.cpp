#include <messages/exit.hpp>

#include <crypto/crypto.hpp>
#include <routing/handler.hpp>

namespace llarp
{
  namespace routing
  {
    bool
    RejectExitMessage::BEncode(llarp_buffer_t* buf) const
    {
      if(!bencode_start_dict(buf))
        return false;
      if(!BEncodeWriteDictMsgType(buf, "A", "J"))
        return false;
      if(!BEncodeWriteDictInt("B", B, buf))
        return false;
      if(!BEncodeWriteDictList("R", R, buf))
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
    RejectExitMessage::DecodeKey(const llarp_buffer_t& k, llarp_buffer_t* buf)
    {
      bool read = false;
      if(!BEncodeMaybeReadDictInt("B", B, read, k, buf))
        return false;
      if(!BEncodeMaybeReadDictList("R", R, read, k, buf))
        return false;
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
    RejectExitMessage::Sign(const llarp::SecretKey& sk)
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
    RejectExitMessage::Verify(const llarp::PubKey& pk) const
    {
      std::array< byte_t, 512 > tmp;
      llarp_buffer_t buf(tmp);
      RejectExitMessage copy;
      copy = *this;
      copy.Z.Zero();
      if(!copy.BEncode(&buf))
        return false;
      buf.sz = buf.cur - buf.base;
      return CryptoManager::instance()->verify(pk, buf, Z);
    }

    bool
    RejectExitMessage::HandleMessage(IMessageHandler* h,
                                     AbstractRouter* r) const
    {
      return h->HandleRejectExitMessage(*this, r);
    }

  }  // namespace routing
}  // namespace llarp
