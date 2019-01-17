#include <crypto/encrypted_frame.hpp>

#include <crypto/crypto.hpp>
#include <util/logger.hpp>
#include <util/mem.hpp>

namespace llarp
{
  bool
  EncryptedFrame::EncryptInPlace(const SecretKey& ourSecretKey,
                                 const PubKey& otherPubkey,
                                 llarp::Crypto* crypto)
  {
    // format of frame is
    // <32 bytes keyed hash of following data>
    // <32 bytes nonce>
    // <32 bytes pubkey>
    // <N bytes encrypted payload>
    //
    byte_t* hash     = data();
    byte_t* noncePtr = hash + SHORTHASHSIZE;
    byte_t* pubkey   = noncePtr + TUNNONCESIZE;
    byte_t* body     = pubkey + PUBKEYSIZE;

    SharedSecret shared;

    auto DH      = crypto->dh_client;
    auto Encrypt = crypto->xchacha20;
    auto MDS     = crypto->hmac;

    llarp_buffer_t buf;
    buf.base = body;
    buf.cur  = buf.base;
    buf.sz   = size() - EncryptedFrameOverheadSize;

    // set our pubkey
    memcpy(pubkey, ourSecretKey.toPublic().data(), PUBKEYSIZE);
    // randomize nonce
    crypto->randbytes(noncePtr, TUNNONCESIZE);
    TunnelNonce nonce(noncePtr);

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
    buf.base = noncePtr;
    buf.cur  = buf.base;
    buf.sz   = size() - SHORTHASHSIZE;

    if(!MDS(hash, buf, shared))
    {
      llarp::LogError("Failed to generate message auth");
      return false;
    }
    return true;
  }

  bool
  EncryptedFrame::DecryptInPlace(const SecretKey& ourSecretKey,
                                 llarp::Crypto* crypto)
  {
    // format of frame is
    // <32 bytes keyed hash of following data>
    // <32 bytes nonce>
    // <32 bytes pubkey>
    // <N bytes encrypted payload>
    //
    ShortHash hash(data());
    byte_t* noncePtr = data() + SHORTHASHSIZE;
    byte_t* body     = data() + EncryptedFrameOverheadSize;
    TunnelNonce nonce(noncePtr);
    PubKey otherPubkey(noncePtr + TUNNONCESIZE);

    // use dh_server because we are not the creator of this message
    auto DH      = crypto->dh_server;
    auto Decrypt = crypto->xchacha20;
    auto MDS     = crypto->hmac;

    SharedSecret shared;

    if(!DH(shared, otherPubkey, ourSecretKey, nonce))
    {
      llarp::LogError("DH failed");
      return false;
    }

    llarp_buffer_t buf;
    buf.base = noncePtr;
    buf.cur  = buf.base;
    buf.sz   = size() - SHORTHASHSIZE;

    ShortHash digest;
    if(!MDS(digest.data(), buf, shared))
    {
      llarp::LogError("Digest failed");
      return false;
    }

    if(!std::equal(digest.begin(), digest.end(), hash.begin()))
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
