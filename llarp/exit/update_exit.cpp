#include <messages/exit.hpp>

#include <crypto/crypto.hpp>
#include <routing/handler.hpp>

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
    UpdateExitMessage::DecodeKey(const llarp_buffer_t& k, llarp_buffer_t* buf)
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
    UpdateExitMessage::Verify(const llarp::PubKey& pk) const

    {
      std::array< byte_t, 512 > tmp;
      llarp_buffer_t buf(tmp);
      UpdateExitMessage copy;
      copy = *this;
      copy.Z.Zero();
      if(!copy.BEncode(&buf))
        return false;
      buf.sz = buf.cur - buf.base;
      return CryptoManager::instance()->verify(pk, buf, Z);
    }

    bool
    UpdateExitMessage::Sign(const llarp::SecretKey& sk)
    {
      std::array< byte_t, 512 > tmp;
      llarp_buffer_t buf(tmp);
      Y.Randomize();
      if(!BEncode(&buf))
        return false;
      buf.sz = buf.cur - buf.base;
      return CryptoManager::instance()->sign(Z, sk, buf);
    }

    bool
    UpdateExitMessage::HandleMessage(IMessageHandler* h,
                                     AbstractRouter* r) const
    {
      return h->HandleUpdateExitMessage(*this, r);
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
    UpdateExitVerifyMessage::DecodeKey(const llarp_buffer_t& k,
                                       llarp_buffer_t* buf)
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
                                           AbstractRouter* r) const
    {
      return h->HandleUpdateExitVerifyMessage(*this, r);
    }

  }  // namespace routing
}  // namespace llarp
