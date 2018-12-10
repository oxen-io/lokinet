#include <llarp/crypto.hpp>

namespace llarp
{
  bool
  PubKey::FromString(const std::string& str)
  {
    return HexDecode(str.c_str(), data(), size());
  }

  std::string
  PubKey::ToString() const
  {
    char buf[(PUBKEYSIZE + 1) * 2] = {0};
    return HexEncode(*this, buf);
  }
}  // namespace llarp
