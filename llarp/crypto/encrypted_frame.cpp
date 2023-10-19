#include "encrypted_frame.hpp"

#include <llarp/util/logging.hpp>

#include "crypto.hpp"

namespace llarp
{
  bool
  EncryptedFrame::DoEncrypt(const SharedSecret& shared, bool noDH)
  {
    uint8_t* hash_ptr = data();
    uint8_t* nonce_ptr = hash_ptr + SHORTHASHSIZE;
    uint8_t* pubkey_ptr = nonce_ptr + TUNNONCESIZE;
    uint8_t* body_ptr = pubkey_ptr + PUBKEYSIZE;

    if (noDH)
    {
        crypto::randbytes(nonce_ptr, TUNNONCESIZE);
        crypto::randbytes(pubkey_ptr, PUBKEYSIZE);
    }

    TunnelNonce nonce(nonce_ptr);

    // encrypt body
    if (!crypto::xchacha20(body_ptr, size() - EncryptedFrameOverheadSize, shared, nonce))
    {
      llarp::LogError("encrypt failed");
      return false;
    }

    if (!crypto::hmac(hash_ptr, nonce_ptr, size() - SHORTHASHSIZE, shared))
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

    // set our pubkey
    memcpy(pubkey, ourSecretKey.toPublic().data(), PUBKEYSIZE);
    // randomize nonce
    crypto::randbytes(noncePtr, TUNNONCESIZE);
    TunnelNonce nonce(noncePtr);

    // derive shared key
    if (!crypto::dh_client(shared, otherPubkey, ourSecretKey, nonce))
    {
      llarp::LogError("DH failed");
      return false;
    }

    return DoEncrypt(shared, false);
  }

  bool
  EncryptedFrame::DoDecrypt(const SharedSecret& shared)
  {
    uint8_t* hash_ptr = data();
    uint8_t* nonce_ptr = hash_ptr + SHORTHASHSIZE;
    uint8_t* body_ptr = hash_ptr + EncryptedFrameOverheadSize;

    TunnelNonce nonce(nonce_ptr);

    ShortHash digest;
    if (!crypto::hmac(digest.data(), nonce_ptr, size() - SHORTHASHSIZE, shared))
    {
      llarp::LogError("Digest failed");
      return false;
    }

    if (!std::equal(digest.begin(), digest.end(), hash_ptr))
    {
      llarp::LogError("message authentication failed");
      return false;
    }

    if (!crypto::xchacha20(body_ptr, size() - EncryptedFrameOverheadSize, shared, nonce))
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

    // use dh_server because we are not the creator of this message
    if (!crypto::dh_server(shared, otherPubkey, ourSecretKey, nonce))
    {
      llarp::LogError("DH failed");
      return false;
    }

    return DoDecrypt(shared);
  }
}  // namespace llarp
