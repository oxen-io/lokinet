#include "encrypted_frame.hpp"

#include "crypto.hpp"
#include <llarp/util/logging/logger.hpp>
#include <llarp/util/mem.hpp>

namespace llarp
{
  bool
  EncryptedFrame::DoEncrypt(const SharedSecret& shared, bool noDH)
  {
    byte_t* hash = data();
    byte_t* noncePtr = hash + SHORTHASHSIZE;
    byte_t* pubkey = noncePtr + TUNNONCESIZE;
    byte_t* body = pubkey + PUBKEYSIZE;

    auto crypto = CryptoManager::instance();

    // if noDH flag, means key exchange has already taken place
    // in this case, set pubkey to random noise and choose a
    // random nonce here
    if (noDH)
    {
      crypto->randbytes(noncePtr, TUNNONCESIZE);
      crypto->randbytes(pubkey, PUBKEYSIZE);
    }

    TunnelNonce nonce(noncePtr);

    llarp_buffer_t buf;
    buf.base = body;
    buf.cur = buf.base;
    buf.sz = size() - EncryptedFrameOverheadSize;

    // encrypt body
    if (!crypto->xchacha20(buf, shared, nonce))
    {
      llarp::LogError("encrypt failed");
      return false;
    }

    // generate message auth
    buf.base = noncePtr;
    buf.cur = buf.base;
    buf.sz = size() - SHORTHASHSIZE;

    if (!crypto->hmac(hash, buf, shared))
    {
      llarp::LogError("Failed to generate message auth");
      return false;
    }

    return true;
  }

  bool
  EncryptedFrame::EncryptInPlace(const SecretKey& ourSecretKey, const PubKey& otherPubkey)
  {
    // format of frame is
    // <32 bytes keyed hash of following data>
    // <32 bytes nonce>
    // <32 bytes pubkey>
    // <N bytes encrypted payload>
    //
    byte_t* hash = data();
    byte_t* noncePtr = hash + SHORTHASHSIZE;
    byte_t* pubkey = noncePtr + TUNNONCESIZE;

    SharedSecret shared;

    auto crypto = CryptoManager::instance();

    // set our pubkey
    memcpy(pubkey, ourSecretKey.toPublic().data(), PUBKEYSIZE);
    // randomize nonce
    crypto->randbytes(noncePtr, TUNNONCESIZE);
    TunnelNonce nonce(noncePtr);

    // derive shared key
    if (!crypto->dh_client(shared, otherPubkey, ourSecretKey, nonce))
    {
      llarp::LogError("DH failed");
      return false;
    }

    return DoEncrypt(shared, false);
  }

  bool
  EncryptedFrame::DoDecrypt(const SharedSecret& shared)
  {
    ShortHash hash(data());
    byte_t* noncePtr = data() + SHORTHASHSIZE;
    byte_t* body = data() + EncryptedFrameOverheadSize;
    TunnelNonce nonce(noncePtr);

    auto crypto = CryptoManager::instance();

    llarp_buffer_t buf;
    buf.base = noncePtr;
    buf.cur = buf.base;
    buf.sz = size() - SHORTHASHSIZE;

    ShortHash digest;
    if (!crypto->hmac(digest.data(), buf, shared))
    {
      llarp::LogError("Digest failed");
      return false;
    }

    if (!std::equal(digest.begin(), digest.end(), hash.begin()))
    {
      llarp::LogError("message authentication failed");
      return false;
    }

    buf.base = body;
    buf.cur = body;
    buf.sz = size() - EncryptedFrameOverheadSize;

    if (!crypto->xchacha20(buf, shared, nonce))
    {
      llarp::LogError("decrypt failed");
      return false;
    }

    return true;
  }

  bool
  EncryptedFrame::DecryptInPlace(const SecretKey& ourSecretKey)
  {
    // format of frame is
    // <32 bytes keyed hash of following data>
    // <32 bytes nonce>
    // <32 bytes pubkey>
    // <N bytes encrypted payload>
    //
    byte_t* noncePtr = data() + SHORTHASHSIZE;
    TunnelNonce nonce(noncePtr);
    PubKey otherPubkey(noncePtr + TUNNONCESIZE);

    SharedSecret shared;

    auto crypto = CryptoManager::instance();

    // use dh_server because we are not the creator of this message
    if (!crypto->dh_server(shared, otherPubkey, ourSecretKey, nonce))
    {
      llarp::LogError("DH failed");
      return false;
    }

    return DoDecrypt(shared);
  }
}  // namespace llarp
