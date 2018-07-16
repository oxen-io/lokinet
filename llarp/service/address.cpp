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
  }  // namespace service
}  // namespace llarp