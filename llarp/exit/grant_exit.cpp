#include <llarp/messages/exit.hpp>
#include <llarp/routing/handler.hpp>

namespace llarp
{
  namespace routing
  {
    bool
    GrantExitMessage::BEncode(llarp_buffer_t* buf) const
    {
      (void)buf;
      // TODO: implement me
      return false;
    }

    bool
    GrantExitMessage::DecodeKey(llarp_buffer_t k, llarp_buffer_t* buf)
    {
      (void)k;
      (void)buf;
      // TODO: implement me
      return false;
    }

    bool
    GrantExitMessage::HandleMessage(IMessageHandler* h, llarp_router* r) const
    {
      return h->HandleGrantExitMessage(this, r);
    }

  }  // namespace routing
}  // namespace llarp