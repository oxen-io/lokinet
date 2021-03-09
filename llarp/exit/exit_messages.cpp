#include "exit_messages.hpp"

#include <llarp/crypto/crypto.hpp>
#include <llarp/routing/handler.hpp>

namespace llarp
{
  namespace routing
  {
    bool
    ObtainExitMessage::Sign(const llarp::SecretKey& sk)
    {
      std::array<byte_t, 1024> tmp;
      llarp_buffer_t buf(tmp);
      I = seckey_topublic(sk);
      Z.Zero();
      if (!BEncode(&buf))
      {
        return false;
      }
      buf.sz = buf.cur - buf.base;
      return CryptoManager::instance()->sign(Z, sk, buf);
    }

    bool
    ObtainExitMessage::Verify() const
    {
      std::array<byte_t, 1024> tmp;
      llarp_buffer_t buf(tmp);
      ObtainExitMessage copy;
      copy = *this;
      copy.Z.Zero();
      if (!copy.BEncode(&buf))
      {
        return false;
      }
      // rewind buffer
      buf.sz = buf.cur - buf.base;
      return CryptoManager::instance()->verify(I, buf, Z);
    }

    bool
    ObtainExitMessage::BEncode(llarp_buffer_t* buf) const
    {
      if (!bencode_start_dict(buf))
        return false;
      if (!BEncodeWriteDictMsgType(buf, "A", "O"))
        return false;
      if (!BEncodeWriteDictArray("B", B, buf))
        return false;
      if (!BEncodeWriteDictInt("E", E, buf))
        return false;
      if (!BEncodeWriteDictEntry("I", I, buf))
        return false;
      if (!BEncodeWriteDictInt("S", S, buf))
        return false;
      if (!BEncodeWriteDictInt("T", T, buf))
        return false;
      if (!BEncodeWriteDictInt("V", version, buf))
        return false;
      if (!BEncodeWriteDictArray("W", W, buf))
        return false;
      if (!BEncodeWriteDictInt("X", X, buf))
        return false;
      if (!BEncodeWriteDictEntry("Z", Z, buf))
        return false;
      return bencode_end(buf);
    }

    bool
    ObtainExitMessage::DecodeKey(const llarp_buffer_t& k, llarp_buffer_t* buf)
    {
      bool read = false;
      if (!BEncodeMaybeReadDictList("B", B, read, k, buf))
        return false;
      if (!BEncodeMaybeReadDictInt("E", E, read, k, buf))
        return false;
      if (!BEncodeMaybeReadDictEntry("I", I, read, k, buf))
        return false;
      if (!BEncodeMaybeReadDictInt("S", S, read, k, buf))
        return false;
      if (!BEncodeMaybeReadDictInt("T", T, read, k, buf))
        return false;
      if (!BEncodeMaybeReadDictInt("V", version, read, k, buf))
        return false;
      if (!BEncodeMaybeReadDictList("W", W, read, k, buf))
        return false;
      if (!BEncodeMaybeReadDictInt("X", X, read, k, buf))
        return false;
      if (!BEncodeMaybeReadDictEntry("Z", Z, read, k, buf))
        return false;
      return read;
    }

    bool
    ObtainExitMessage::HandleMessage(IMessageHandler* h, AbstractRouter* r) const
    {
      return h->HandleObtainExitMessage(*this, r);
    }

    bool
    GrantExitMessage::BEncode(llarp_buffer_t* buf) const
    {
      if (!bencode_start_dict(buf))
        return false;
      if (!BEncodeWriteDictMsgType(buf, "A", "G"))
        return false;
      if (!BEncodeWriteDictInt("S", S, buf))
        return false;
      if (!BEncodeWriteDictInt("T", T, buf))
        return false;
      if (!BEncodeWriteDictInt("V", version, buf))
        return false;
      if (!BEncodeWriteDictEntry("Y", Y, buf))
        return false;
      if (!BEncodeWriteDictEntry("Z", Z, buf))
        return false;
      return bencode_end(buf);
    }

    bool
    GrantExitMessage::DecodeKey(const llarp_buffer_t& k, llarp_buffer_t* buf)
    {
      bool read = false;
      if (!BEncodeMaybeReadDictInt("S", S, read, k, buf))
        return false;
      if (!BEncodeMaybeReadDictInt("T", T, read, k, buf))
        return false;
      if (!BEncodeMaybeReadDictInt("V", version, read, k, buf))
        return false;
      if (!BEncodeMaybeReadDictEntry("Y", Y, read, k, buf))
        return false;
      if (!BEncodeMaybeReadDictEntry("Z", Z, read, k, buf))
        return false;
      return read;
    }

    bool
    GrantExitMessage::Verify(const llarp::PubKey& pk) const
    {
      std::array<byte_t, 512> tmp;
      llarp_buffer_t buf(tmp);
      GrantExitMessage copy;
      copy = *this;
      copy.Z.Zero();
      if (!copy.BEncode(&buf))
        return false;
      buf.sz = buf.cur - buf.base;
      return CryptoManager::instance()->verify(pk, buf, Z);
    }

    bool
    GrantExitMessage::Sign(const llarp::SecretKey& sk)
    {
      std::array<byte_t, 512> tmp;
      llarp_buffer_t buf(tmp);
      Z.Zero();
      Y.Randomize();
      if (!BEncode(&buf))
        return false;
      buf.sz = buf.cur - buf.base;
      return CryptoManager::instance()->sign(Z, sk, buf);
    }

    bool
    GrantExitMessage::HandleMessage(IMessageHandler* h, AbstractRouter* r) const
    {
      return h->HandleGrantExitMessage(*this, r);
    }

    bool
    RejectExitMessage::BEncode(llarp_buffer_t* buf) const
    {
      if (!bencode_start_dict(buf))
        return false;
      if (!BEncodeWriteDictMsgType(buf, "A", "J"))
        return false;
      if (!BEncodeWriteDictInt("B", B, buf))
        return false;
      if (!BEncodeWriteDictList("R", R, buf))
        return false;
      if (!BEncodeWriteDictInt("S", S, buf))
        return false;
      if (!BEncodeWriteDictInt("T", T, buf))
        return false;
      if (!BEncodeWriteDictInt("V", version, buf))
        return false;
      if (!BEncodeWriteDictEntry("Y", Y, buf))
        return false;
      if (!BEncodeWriteDictEntry("Z", Z, buf))
        return false;
      return bencode_end(buf);
    }

    bool
    RejectExitMessage::DecodeKey(const llarp_buffer_t& k, llarp_buffer_t* buf)
    {
      bool read = false;
      if (!BEncodeMaybeReadDictInt("B", B, read, k, buf))
        return false;
      if (!BEncodeMaybeReadDictList("R", R, read, k, buf))
        return false;
      if (!BEncodeMaybeReadDictInt("S", S, read, k, buf))
        return false;
      if (!BEncodeMaybeReadDictInt("T", T, read, k, buf))
        return false;
      if (!BEncodeMaybeReadDictInt("V", version, read, k, buf))
        return false;
      if (!BEncodeMaybeReadDictEntry("Y", Y, read, k, buf))
        return false;
      if (!BEncodeMaybeReadDictEntry("Z", Z, read, k, buf))
        return false;
      return read;
    }

    bool
    RejectExitMessage::Sign(const llarp::SecretKey& sk)
    {
      std::array<byte_t, 512> tmp;
      llarp_buffer_t buf(tmp);
      Z.Zero();
      Y.Randomize();
      if (!BEncode(&buf))
        return false;
      buf.sz = buf.cur - buf.base;
      return CryptoManager::instance()->sign(Z, sk, buf);
    }

    bool
    RejectExitMessage::Verify(const llarp::PubKey& pk) const
    {
      std::array<byte_t, 512> tmp;
      llarp_buffer_t buf(tmp);
      RejectExitMessage copy;
      copy = *this;
      copy.Z.Zero();
      if (!copy.BEncode(&buf))
        return false;
      buf.sz = buf.cur - buf.base;
      return CryptoManager::instance()->verify(pk, buf, Z);
    }

    bool
    RejectExitMessage::HandleMessage(IMessageHandler* h, AbstractRouter* r) const
    {
      return h->HandleRejectExitMessage(*this, r);
    }

    bool
    UpdateExitMessage::BEncode(llarp_buffer_t* buf) const
    {
      if (!bencode_start_dict(buf))
        return false;
      if (!BEncodeWriteDictMsgType(buf, "A", "V"))
        return false;
      if (!BEncodeWriteDictEntry("P", P, buf))
        return false;
      if (!BEncodeWriteDictInt("S", S, buf))
        return false;
      if (!BEncodeWriteDictInt("T", T, buf))
        return false;
      if (!BEncodeWriteDictInt("V", version, buf))
        return false;
      if (!BEncodeWriteDictEntry("Z", Z, buf))
        return false;
      return bencode_end(buf);
    }

    bool
    UpdateExitMessage::DecodeKey(const llarp_buffer_t& k, llarp_buffer_t* buf)
    {
      bool read = false;
      if (!BEncodeMaybeReadDictInt("S", S, read, k, buf))
        return false;
      if (!BEncodeMaybeReadDictInt("T", T, read, k, buf))
        return false;
      if (!BEncodeMaybeReadDictInt("V", version, read, k, buf))
        return false;
      if (!BEncodeMaybeReadDictEntry("P", P, read, k, buf))
        return false;
      if (!BEncodeMaybeReadDictEntry("Z", Z, read, k, buf))
        return false;
      return read;
    }

    bool
    UpdateExitMessage::Verify(const llarp::PubKey& pk) const

    {
      std::array<byte_t, 512> tmp;
      llarp_buffer_t buf(tmp);
      UpdateExitMessage copy;
      copy = *this;
      copy.Z.Zero();
      if (!copy.BEncode(&buf))
        return false;
      buf.sz = buf.cur - buf.base;
      return CryptoManager::instance()->verify(pk, buf, Z);
    }

    bool
    UpdateExitMessage::Sign(const llarp::SecretKey& sk)
    {
      std::array<byte_t, 512> tmp;
      llarp_buffer_t buf(tmp);
      Y.Randomize();
      if (!BEncode(&buf))
        return false;
      buf.sz = buf.cur - buf.base;
      return CryptoManager::instance()->sign(Z, sk, buf);
    }

    bool
    UpdateExitMessage::HandleMessage(IMessageHandler* h, AbstractRouter* r) const
    {
      return h->HandleUpdateExitMessage(*this, r);
    }

    bool
    UpdateExitVerifyMessage::BEncode(llarp_buffer_t* buf) const
    {
      if (!bencode_start_dict(buf))
        return false;
      if (!BEncodeWriteDictMsgType(buf, "A", "V"))
        return false;
      if (!BEncodeWriteDictInt("S", S, buf))
        return false;
      if (!BEncodeWriteDictInt("T", T, buf))
        return false;
      if (!BEncodeWriteDictInt("V", version, buf))
        return false;
      return bencode_end(buf);
    }

    bool
    UpdateExitVerifyMessage::DecodeKey(const llarp_buffer_t& k, llarp_buffer_t* buf)
    {
      bool read = false;
      if (!BEncodeMaybeReadDictInt("S", S, read, k, buf))
        return false;
      if (!BEncodeMaybeReadDictInt("T", T, read, k, buf))
        return false;
      if (!BEncodeMaybeReadDictInt("V", version, read, k, buf))
        return false;
      return read;
    }

    bool
    UpdateExitVerifyMessage::HandleMessage(IMessageHandler* h, AbstractRouter* r) const
    {
      return h->HandleUpdateExitVerifyMessage(*this, r);
    }

    bool
    CloseExitMessage::BEncode(llarp_buffer_t* buf) const
    {
      if (!bencode_start_dict(buf))
        return false;
      if (!BEncodeWriteDictMsgType(buf, "A", "C"))
        return false;
      if (!BEncodeWriteDictInt("S", S, buf))
        return false;
      if (!BEncodeWriteDictInt("V", version, buf))
        return false;
      if (!BEncodeWriteDictEntry("Y", Y, buf))
        return false;
      if (!BEncodeWriteDictEntry("Z", Z, buf))
        return false;
      return bencode_end(buf);
    }

    bool
    CloseExitMessage::DecodeKey(const llarp_buffer_t& k, llarp_buffer_t* buf)
    {
      bool read = false;
      if (!BEncodeMaybeReadDictInt("S", S, read, k, buf))
        return false;
      if (!BEncodeMaybeReadDictInt("V", version, read, k, buf))
        return false;
      if (!BEncodeMaybeReadDictEntry("Y", Y, read, k, buf))
        return false;
      if (!BEncodeMaybeReadDictEntry("Z", Z, read, k, buf))
        return false;
      return read;
    }

    bool
    CloseExitMessage::Verify(const llarp::PubKey& pk) const
    {
      std::array<byte_t, 512> tmp;
      llarp_buffer_t buf(tmp);
      CloseExitMessage copy;
      copy = *this;
      copy.Z.Zero();
      if (!copy.BEncode(&buf))
        return false;
      buf.sz = buf.cur - buf.base;
      return CryptoManager::instance()->verify(pk, buf, Z);
    }

    bool
    CloseExitMessage::Sign(const llarp::SecretKey& sk)
    {
      std::array<byte_t, 512> tmp;
      llarp_buffer_t buf(tmp);
      Z.Zero();
      Y.Randomize();
      if (!BEncode(&buf))
        return false;
      buf.sz = buf.cur - buf.base;
      return CryptoManager::instance()->sign(Z, sk, buf);
    }

    bool
    CloseExitMessage::HandleMessage(IMessageHandler* h, AbstractRouter* r) const
    {
      return h->HandleCloseExitMessage(*this, r);
    }
  }  // namespace routing
}  // namespace llarp
