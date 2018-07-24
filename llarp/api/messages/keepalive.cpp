#include <llarp/api/messages.hpp>

namespace llarp
{
  namespace api
  {
    KeepAliveMessage::~KeepAliveMessage()
    {
    }

    bool
    KeepAliveMessage::EncodeParams(llarp_buffer_t *buf) const
    {
      return true;
    }

  }  // namespace api
}  // namespace llarp