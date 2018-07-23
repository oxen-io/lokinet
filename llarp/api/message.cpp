#include <llarp/api/messages.hpp>

namespace llarp
{
  namespace api
  {
    bool
    IMessage::BEncode(llarp_buffer_t* buf) const
    {
      if(!bencode_start_dict(buf))
        return false;
      if(!BEncodeWriteDictString("A", FunctionName(), buf))
        return false;
      if(!EncodeParams(buf))
        return false;
      if(!BEncodeWriteDictInt("Y", seqno, buf))
        return false;
      if(!BEncodeWriteDictEntry("Z", hash, buf))
        return false;
      return bencode_end(buf);
    }

    bool
    IMessage::DecodeKey(llarp_buffer_t key, llarp_buffer_t* val)
    {
      bool read = false;
      if(!BEncodeMaybeReadDictInt("Y", seqno, read, key, val))
        return false;
      if(!BEncodeMaybeReadDictEntry("Z", hash, read, key, val))
        return false;
      return read;
    }

    bool
    IMessage::IsWellFormed(llarp_crypto* crypto, const std::string& password)
    {
      // hash password
      llarp::ShortHash secret;
      llarp_buffer_t passbuf;
      passbuf.base = (byte_t*)password.c_str();
      passbuf.cur  = passbuf.base;
      passbuf.sz   = password.size();
      crypto->shorthash(secret, passbuf);

      llarp::ShortHash digest, tmpHash;
      // save hash
      tmpHash = hash;
      // zero hash
      hash.Zero();

      // bencode
      byte_t tmp[1500];
      llarp_buffer_t buf;
      buf.base = tmp;
      buf.cur  = buf.base;
      buf.sz   = sizeof(tmp);
      if(!BEncode(&buf))
        return false;

      // rewind buffer
      buf.sz  = buf.cur - buf.base;
      buf.cur = buf.base;
      // calculate message auth
      crypto->hmac(digest, buf, secret);
      // restore hash
      hash = tmpHash;
      return tmpHash == digest;
    }

    void
    IMessage::CalculateHash(llarp_crypto* crypto, const std::string& password)
    {
      // hash password
      llarp::ShortHash secret;
      llarp_buffer_t passbuf;
      passbuf.base = (byte_t*)password.c_str();
      passbuf.cur  = passbuf.base;
      passbuf.sz   = password.size();
      crypto->shorthash(secret, passbuf);

      // llarp::ShortHash digest;
      // zero hash
      hash.Zero();

      // bencode
      byte_t tmp[1500];
      llarp_buffer_t buf;
      buf.base = tmp;
      buf.cur  = buf.base;
      buf.sz   = sizeof(tmp);
      if(BEncode(&buf))
      {
        // rewind buffer
        buf.sz  = buf.cur - buf.base;
        buf.cur = buf.base;
        // calculate message auth
        crypto->hmac(hash, buf, secret);
      }
    }
  }  // namespace api
}  // namespace llarp
