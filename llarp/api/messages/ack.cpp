#include <llarp/api/messages.hpp>

namespace llarp
{
  namespace api
  {
    AckMessage::~AckMessage()
    {
    }

    bool
    AckMessage::EncodeParams(llarp_buffer_t *buf) const
    {
      return true;
    }

  }  // namespace api
}  // namespace llarp