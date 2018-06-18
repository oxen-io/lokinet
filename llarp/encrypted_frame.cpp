#include <llarp/crypto.hpp>
#include <llarp/encrypted_frame.hpp>
#include "logger.hpp"

namespace llarp
{
  Encrypted::Encrypted(const byte_t* buf, size_t sz)
  {
    size = sz;
    data = new byte_t[sz];
    if(buf)
      memcpy(data, buf, sz);
    m_Buffer.base = data;
    m_Buffer.cur  = data;
    m_Buffer.sz   = size;
  }

  Encrypted::Encrypted(size_t sz) : Encrypted(nullptr, sz)
  {
  }

  Encrypted::~Encrypted()
  {
    if(data)
      delete[] data;
  }

  bool
  EncryptedFrame::EncryptInPlace(byte_t* ourSecretKey, byte_t* otherPubkey,
                                 llarp_crypto* crypto)
  {
    // format of frame is
    // <32 bytes keyed hash of following data>
    // <32 bytes nonce>
    // <32 bytes pubkey>
    // <N bytes encrypted payload>
    //
    byte_t* hash   = data;
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
    buf.sz   = size - EncryptedFrame::OverheadSize;

    // set our pubkey
    memcpy(pubkey, llarp::seckey_topublic(ourSecretKey), PUBKEYSIZE);
    // randomize nonce
    crypto->randbytes(nonce, TUNNONCESIZE);

    // derive shared key
    if(!DH(shared, otherPubkey, nonce, ourSecretKey))
    {
      llarp::Error("DH failed");
      return false;
    }
    // encrypt body
    if(!Encrypt(buf, shared, nonce))
    {
      llarp::Error("encrypt failed");
      return false;
    }

    // generate message auth
    buf.base = nonce;
    buf.cur  = buf.base;
    buf.sz   = size - SHORTHASHSIZE;

    if(!MDS(hash, buf, shared))
    {
      llarp::Error("Failed to generate messgae auth");
      return false;
    }
    return true;
  }

  bool
  EncryptedFrame::DecryptInPlace(byte_t* ourSecretKey, llarp_crypto* crypto)
  {
    if(size <= size_t(EncryptedFrame::OverheadSize))
    {
      llarp::Warn("encrypted frame too small, ", size, " <= ", size_t(EncryptedFrame::OverheadSize));
      return false;
    }
    // format of frame is
    // <32 bytes keyed hash of following data>
    // <32 bytes nonce>
    // <32 bytes pubkey>
    // <N bytes encrypted payload>
    //
    byte_t* hash        = data;
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
    buf.sz   = size - SHORTHASHSIZE;

    SharedSecret shared;
    ShortHash digest;

    if(!DH(shared, otherPubkey, nonce, ourSecretKey))
    {
      llarp::Error("DH failed");
      return false;
    }

    if(!MDS(digest, buf, shared))
    {
      llarp::Error("Digest failed");
      return false;
    }

    if(memcmp(digest, hash, digest.size()))
    {
      llarp::Error("message authentication failed");
      return false;
    }

    buf.base = body;
    buf.cur  = body;
    buf.sz   = size - EncryptedFrame::OverheadSize;

    if(!Decrypt(buf, shared, nonce))
    {
      llarp::Error("decrypt failed");
      return false;
    }
    return true;
  }
}
