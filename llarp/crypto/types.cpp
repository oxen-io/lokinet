#include "types.hpp"

#include <llarp/util/buffer.hpp>
#include <llarp/util/file.hpp>

#include <oxenc/hex.h>
#include <sodium/crypto_hash_sha512.h>
#include <sodium/crypto_scalarmult_ed25519.h>

namespace llarp
{
  bool
  PubKey::FromString(const std::string& str)
  {
    if (str.size() != 2 * size())
      return false;
    oxenc::from_hex(str.begin(), str.end(), begin());
    return true;
  }

  PubKey
  PubKey::from_string(const std::string& s)
  {
    PubKey p;
    oxenc::from_hex(s.begin(), s.end(), p.begin());
    return p;
  }

  std::string
  PubKey::ToString() const
  {
    return oxenc::to_hex(begin(), end());
  }

  bool
  SecretKey::LoadFromFile(const fs::path& fname)
  {
    size_t sz;
    std::array<byte_t, 128> tmp;
    try
    {
      sz = util::slurp_file(fname, tmp.data(), tmp.size());
    }
    catch (const std::exception&)
    {
      return false;
    }

    if (sz == size())
    {
      // is raw buffer
      std::copy_n(tmp.begin(), sz, begin());
      return true;
    }

    llarp_buffer_t buf(tmp);
    return BDecode(&buf);
  }

  bool
  SecretKey::Recalculate()
  {
    PrivateKey key;
    PubKey pubkey;
    if (!toPrivate(key) || !key.toPublic(pubkey))
      return false;
    std::memcpy(data() + 32, pubkey.data(), 32);
    return true;
  }

  bool
  SecretKey::toPrivate(PrivateKey& key) const
  {
    // Ed25519 calculates a 512-bit hash from the seed; the first half (clamped)
    // is the private key; the second half is the hash that gets used in
    // signing.
    unsigned char h[crypto_hash_sha512_BYTES];
    if (crypto_hash_sha512(h, data(), 32) < 0)
      return false;
    h[0] &= 248;
    h[31] &= 63;
    h[31] |= 64;
    std::memcpy(key.data(), h, 64);
    return true;
  }

  bool
  PrivateKey::toPublic(PubKey& pubkey) const
  {
    return crypto_scalarmult_ed25519_base_noclamp(pubkey.data(), data()) != -1;
  }

  bool
  SecretKey::SaveToFile(const fs::path& fname) const
  {
    std::string tmp(128, 0);
    llarp_buffer_t buf(tmp);
    if (!bt_encode(&buf))
      return false;

    tmp.resize(buf.cur - buf.base);
    try
    {
      util::dump_file(fname, tmp);
    }
    catch (const std::exception&)
    {
      return false;
    }
    return true;
  }

  bool
  IdentitySecret::LoadFromFile(const fs::path& fname)
  {
    std::array<byte_t, SIZE> buf;
    size_t sz;
    try
    {
      sz = util::slurp_file(fname, buf.data(), buf.size());
    }
    catch (const std::exception& e)
    {
      llarp::LogError("failed to load service node seed: ", e.what());
      return false;
    }
    if (sz != SIZE)
    {
      llarp::LogError("service node seed size invalid: ", sz, " != ", SIZE);
      return false;
    }
    std::copy(buf.begin(), buf.end(), begin());
    return true;
  }

  byte_t*
  Signature::Lo()
  {
    return data();
  }

  const byte_t*
  Signature::Lo() const
  {
    return data();
  }

  byte_t*
  Signature::Hi()
  {
    return data() + 32;
  }

  const byte_t*
  Signature::Hi() const
  {
    return data() + 32;
  }
}  // namespace llarp
