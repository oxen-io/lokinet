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
  SecretKey::LoadFromFile(const char* fname)
  {
    std::ifstream f(fname, std::ios::in | std::ios::binary);
    if(!f.is_open())
    {
      return false;
    }

    f.seekg(0, std::ios::end);
    const size_t sz = f.tellg();
    f.seekg(0, std::ios::beg);

    if(sz == size())
    {
      // is raw buffer
      std::copy_n(std::istreambuf_iterator< char >(f), sz, begin());
      return true;
    }
    std::array< byte_t, 128 > tmp;
    llarp_buffer_t buf(tmp);
    if(sz > sizeof(tmp))
    {
      return false;
    }
    f.read(reinterpret_cast< char* >(tmp.data()), sz);
    return BDecode(&buf);
  }

  bool
  SecretKey::Recalculate()
  {
    return crypto_scalarmult_ed25519_base(data() + 32, data()) != -1;
  }

  bool
  SecretKey::SaveToFile(const char* fname) const
  {
    std::array< byte_t, 128 > tmp;
    llarp_buffer_t buf(tmp);
    if(!BEncode(&buf))
    {
      return false;
    }
    const fs::path fpath = std::string(fname);
    auto optional_f =
        llarp::util::OpenFileStream< std::ofstream >(fpath, std::ios::binary);
    if(!optional_f)
      return false;
    auto& f = optional_f.value();
    if(!f.is_open())
      return false;
    f.write((char*)buf.base, buf.cur - buf.base);
    return f.good();
  }

  bool
  IdentitySecret::LoadFromFile(const char* fname)
  {
    const fs::path fpath = std::string(fname);
    auto optional        = util::OpenFileStream< std::ifstream >(
        fpath, std::ios::binary | std::ios::in);
    if(!optional)
      return false;
    auto& f = optional.value();
    f.seekg(0, std::ios::end);
    const size_t sz = f.tellg();
    f.seekg(0, std::ios::beg);
    if(sz != 32)
    {
      llarp::LogError("service node seed size invalid: ", sz, " != 32");
      return false;
    }
    std::copy_n(std::istreambuf_iterator< char >(f), sz, begin());
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
