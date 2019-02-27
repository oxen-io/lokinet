#include <crypto/types.hpp>

#include <util/buffer.hpp>

#include <fstream>
#include <iterator>

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
    return Base32Encode(*this, buf);
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
  SecretKey::SaveToFile(const char* fname) const
  {
    std::array< byte_t, 128 > tmp;
    llarp_buffer_t buf(tmp);
    if(!BEncode(&buf))
    {
      return false;
    }

    std::ofstream f;
    f.open(fname, std::ios::binary);
    if(!f.is_open())
      return false;
    f.write((char*)buf.base, buf.cur - buf.base);
    return true;
  }

  bool
  IdentitySecret::LoadFromFile(const char* fname)
  {
    std::ifstream f(fname, std::ios::binary | std::ios::in);
    if(!f.is_open())
      return false;
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

}  // namespace llarp
