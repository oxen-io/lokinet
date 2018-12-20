#include <crypto.hpp>
#include <encrypted_frame.hpp>
#include <logger.hpp>
#include <mem.hpp>

namespace llarp
{
  bool
  EncryptedFrame::EncryptInPlace(const byte_t* ourSecretKey,
                                 const byte_t* otherPubkey,
                                 llarp::Crypto* crypto)
  {
    // format of frame is
    // <32 bytes keyed hash of following data>
    // <32 bytes nonce>
    // <32 bytes pubkey>
    // <N bytes encrypted payload>
    //
    byte_t* hash   = data();
    byte_t* nonce  = hash + SHORTHASHSIZE;
    byte_t* pubkey = nonce + TUNNONCESIZE;
    byte_t* body   = pubkey + PUBKEYSIZE;

    SharedSecret shared;

    auto DH      = crypto->dh_client;
    auto Encrypt = crypto->xchacha20;
    auto MDS     = crypto->hmac;

    llarp_buffer_t buf;
    buf.base = body;
    buf.cur  = buf.base;
    buf.sz   = size() - EncryptedFrameOverheadSize;

    // set our pubkey
    memcpy(pubkey, llarp::seckey_topublic(ourSecretKey), PUBKEYSIZE);
    // randomize nonce
    crypto->randbytes(nonce, TUNNONCESIZE);

    // derive shared key
    if(!DH(shared, otherPubkey, ourSecretKey, nonce))
    {
      llarp::LogError("DH failed");
      return false;
    }

    // encrypt body
    if(!Encrypt(buf, shared, nonce))
    {
      llarp::LogError("encrypt failed");
      return false;
    }

    // generate message auth
    buf.base = nonce;
    buf.cur  = buf.base;
    buf.sz   = size() - SHORTHASHSIZE;

    if(!MDS(hash, buf, shared))
    {
      llarp::LogError("Failed to generate messgae auth");
      return false;
    }
    return true;
  }

  bool
  EncryptedFrame::DecryptInPlace(const byte_t* ourSecretKey,
                                 llarp::Crypto* crypto)
  {
    // format of frame is
    // <32 bytes keyed hash of following data>
    // <32 bytes nonce>
    // <32 bytes pubkey>
    // <N bytes encrypted payload>
    //
    byte_t* hash        = data();
    byte_t* nonce       = hash + SHORTHASHSIZE;
    byte_t* otherPubkey = nonce + TUNNONCESIZE;
    byte_t* body        = otherPubkey + PUBKEYSIZE;

    // use dh_server becuase we are not the creator of this message
    auto DH      = crypto->dh_server;
    auto Decrypt = crypto->xchacha20;
    auto MDS     = crypto->hmac;

    llarp_buffer_t buf;
    buf.base = nonce;
    buf.cur  = buf.base;
    buf.sz   = size() - SHORTHASHSIZE;

    SharedSecret shared;
    ShortHash digest;

    if(!DH(shared, otherPubkey, ourSecretKey, nonce))
    {
      llarp::LogError("DH failed");
      return false;
    }

    if(!MDS(digest, buf, shared))
    {
      llarp::LogError("Digest failed");
      return false;
    }

    if(memcmp(digest, hash, digest.size()))
    {
      llarp::LogError("message authentication failed");
      return false;
    }

    buf.base = body;
    buf.cur  = body;
    buf.sz   = size() - EncryptedFrameOverheadSize;

    if(!Decrypt(buf, shared, nonce))
    {
      llarp::LogError("decrypt failed");
      return false;
    }
    return true;
  }
}  // namespace llarp
