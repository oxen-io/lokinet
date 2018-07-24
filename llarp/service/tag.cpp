#include <llarp/service/tag.hpp>

namespace llarp
{
  namespace service
  {
    std::string
    Tag::ToString() const
    {
      return std::string((const char *)data());
    }
  }  // namespace service
}  // namespace llarp