#include <llarp/messages/discard.hpp>

namespace llarp
{
  DiscardMessage::~DiscardMessage()
  {
    llarp::LogDebug("~DiscardMessage");
  }
}  // namespace llarp
