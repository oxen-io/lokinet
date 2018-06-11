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
    byte_t* nonce  = hash + sizeof(llarp_shorthash_t);
    byte_t* pubkey = nonce + sizeof(llarp_tunnel_nonce_t);
    byte_t* body   = pubkey + sizeof(llarp_pubkey_t);

    llarp_sharedkey_t shared;

    auto DH      = crypto->dh_client;
    auto Encrypt = crypto->xchacha20;
    auto MDS     = crypto->hmac;

    llarp_buffer_t buf;
    buf.base = body;
    buf.cur  = buf.base;
    buf.sz   = size - OverheadSize;

    // set our pubkey
    memcpy(pubkey, llarp::seckey_topublic(ourSecretKey),
           sizeof(llarp_pubkey_t));
    // randomize nonce
    crypto->randbytes(nonce, sizeof(llarp_tunnel_nonce_t));

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
    buf.sz   = size - sizeof(llarp_shorthash_t);

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
    if(size <= OverheadSize)
    {
      llarp::Warn("encrypted frame too small, ", size, " <= ", OverheadSize);
      return false;
    }
    // format of frame is
    // <32 bytes keyed hash of following data>
    // <32 bytes nonce>
    // <32 bytes pubkey>
    // <N bytes encrypted payload>
    //
    byte_t* hash        = data;
    byte_t* nonce       = hash + sizeof(llarp_shorthash_t);
    byte_t* otherPubkey = nonce + sizeof(llarp_tunnel_nonce_t);
    byte_t* body        = otherPubkey + sizeof(llarp_pubkey_t);

    // use dh_server becuase we are not the creator of this message
    auto DH      = crypto->dh_server;
    auto Decrypt = crypto->xchacha20;
    auto MDS     = crypto->hmac;

    llarp_buffer_t buf;
    buf.base = nonce;
    buf.cur  = buf.base;
    buf.sz   = size - sizeof(llarp_shorthash_t);

    llarp_sharedkey_t shared;
    llarp_shorthash_t digest;

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

    if(memcmp(digest, hash, sizeof(llarp_shorthash_t)))
    {
      llarp::Error("message authentication failed");
      return false;
    }

    buf.base = body;
    buf.cur  = body;
    buf.sz   = size - OverheadSize;

    if(!Decrypt(buf, shared, nonce))
    {
      llarp::Error("decrypt failed");
      return false;
    }
    return true;
  }
}