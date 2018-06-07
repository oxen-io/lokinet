#include <llarp/messages/discard.hpp>

namespace llarp
{
  DiscardMessage::~DiscardMessage()
  {
    llarp::Debug("~DiscardMessage");
  }
}
