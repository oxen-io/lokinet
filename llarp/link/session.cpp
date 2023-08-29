#include "session.hpp"

namespace llarp
{
  bool
  AbstractLinkSession::IsRelay() const
  {
    return GetRemoteRC().IsPublicRouter();
  }

}  // namespace llarp
