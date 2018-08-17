#include <algorithm>
#include <llarp/service/address.hpp>

namespace llarp
{
  namespace service
  {
    std::string
    Address::ToString() const
    {
      char tmp[(1 + 32) * 2] = {0};
      std::string str        = Base32Encode(*this, tmp);
      return str + ".loki";
    }

    bool
    Address::FromString(const std::string& str)
    {
      auto pos = str.find(".loki");
      if(pos == std::string::npos)
        return false;
      auto sub = str.substr(0, pos);
      // make sure it's lowercase
      std::transform(sub.begin(), sub.end(), sub.begin(), ::tolower);
      ;
      return Base32Decode(sub, *this);
    }
  }  // namespace service
}  // namespace llarp
