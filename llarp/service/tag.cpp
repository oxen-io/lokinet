#include "tag.hpp"

namespace llarp
{
  namespace service
  {
    std::string
    Tag::ToString() const
    {
      return std::string(begin(), end());
    }
  }  // namespace service
}  // namespace llarp
