#include <service/address.hpp>

#include <algorithm>

namespace llarp
{
  namespace service
  {
    std::string
    Address::ToString(const char* tld) const
    {
      char tmp[(1 + 32) * 2] = {0};
      std::string str        = Base32Encode(*this, tmp);
      return str + tld;
    }

    bool
    Address::FromString(const std::string& str, const char* tld)
    {
      auto pos = str.find(tld);
      if(pos == std::string::npos)
        return false;
      auto sub = str.substr(0, pos);
      // make sure it's lowercase
      std::transform(sub.begin(), sub.end(), sub.begin(), ::tolower);

      return Base32Decode(sub, *this);
    }
  }  // namespace service
}  // namespace llarp
