#include <crypto.hpp>
#include <fstream>
#include <iterator>
#include <buffer.hpp>

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
    std::ifstream f;
    f.open(fname, std::ios::binary);
    if(!f.is_open())
      return false;
    size_t sz = 0;
    f.seekg(0, std::ios::end);
    sz = f.tellg();
    f.seekg(0, std::ios::beg);
    if(sz == size())
    {
      // is raw buffer
      std::copy(std::istream_iterator< byte_t >(f),
                std::istream_iterator< byte_t >(), begin());
      return true;
    }
    byte_t tmp[128];
    auto buf = llarp::StackBuffer< decltype(tmp) >(tmp);
    if(sz > sizeof(tmp))
      return false;
    f.read((char*)tmp, sz);
    return BDecode(&buf);
  }

  bool
  SecretKey::SaveToFile(const char* fname) const
  {
    byte_t tmp[128];
    auto buf = llarp::StackBuffer< decltype(tmp) >(tmp);
    if(!BEncode(&buf))
      return false;

    std::ofstream f;
    f.open(fname, std::ios::binary);
    if(!f.is_open())
      return false;
    f.write((char*)buf.base, buf.cur - buf.base);
    return true;
  }

}  // namespace llarp
