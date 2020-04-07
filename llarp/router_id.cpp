#include <router_id.hpp>

namespace llarp
{
  std::string
  RouterID::ToString() const
  {
    char stack[64] = {0};
    return std::string(llarp::Base32Encode(*this, stack)) + ".snode";
  }

  std::string
  RouterID::ShortString() const
  {
    return ToString().substr(0, 8);
  }

  util::StatusObject
  RouterID::ExtractStatus() const
  {
    util::StatusObject obj{{"snode", ToString()}, {"hex", ToHex()}};
    return obj;
  }

  bool
  RouterID::FromString(const std::string& str)
  {
    auto pos = str.find(".snode");
    if(pos == std::string::npos || pos == 0)
    {
      return false;
    }
    return Base32Decode(str.substr(0, pos), *this);
  }
}  // namespace llarp
