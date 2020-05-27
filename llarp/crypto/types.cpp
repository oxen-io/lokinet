#include <crypto/types.hpp>

#include <util/buffer.hpp>

#include <fstream>
#include <util/fs.hpp>

#include <iterator>

#include <sodium/crypto_sign.h>
#include <sodium/crypto_sign_ed25519.h>
#include <sodium/crypto_scalarmult_ed25519.h>

namespace llarp
{
  bool
  PubKey::FromString(const std::string& str)
  {
    return HexDecode(str.c_str(), begin(), size());
  }

  std::string
  PubKey::ToString() const
  {
    char buf[(PUBKEYSIZE + 1) * 2] = {0};
    return HexEncode(*this, buf);
  }

  bool
  SecretKey::LoadFromFile(const fs::path& fname)
  {
    std::ifstream f(fname.string(), std::ios::in | std::ios::binary);
    if (!f.is_open())
    {
      return false;
    }

    f.seekg(0, std::ios::end);
    const size_t sz = f.tellg();
    f.seekg(0, std::ios::beg);

    if (sz == size())
    {
      // is raw buffer
      std::copy_n(std::istreambuf_iterator<char>(f), sz, begin());
      return true;
    }
    std::array<byte_t, 128> tmp;
    llarp_buffer_t buf(tmp);
    if (sz > sizeof(tmp))
    {
      return false;
    }
    f.read(reinterpret_cast<char*>(tmp.data()), sz);
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
    std::array<byte_t, 128> tmp;
    llarp_buffer_t buf(tmp);
    if (!BEncode(&buf))
    {
      return false;
    }
    auto optional_f = llarp::util::OpenFileStream<std::ofstream>(fname, std::ios::binary);
    if (!optional_f)
      return false;
    auto& f = *optional_f;
    if (!f.is_open())
      return false;
    f.write((char*)buf.base, buf.cur - buf.base);
    return f.good();
  }

  bool
  IdentitySecret::LoadFromFile(const fs::path& fname)
  {
    auto optional = util::OpenFileStream<std::ifstream>(fname, std::ios::binary | std::ios::in);
    if (!optional)
      return false;
    auto& f = *optional;
    f.seekg(0, std::ios::end);
    const size_t sz = f.tellg();
    f.seekg(0, std::ios::beg);
    if (sz != 32)
    {
      llarp::LogError("service node seed size invalid: ", sz, " != 32");
      return false;
    }
    std::copy_n(std::istreambuf_iterator<char>(f), sz, begin());
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
