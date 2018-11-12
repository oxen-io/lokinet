#include <llarp/messages/exit.hpp>
#include <llarp/routing/handler.hpp>

namespace llarp
{
  namespace routing
  {
    bool
    RejectExitMessage::BEncode(llarp_buffer_t* buf) const
    {
      (void)buf;

      // TODO: implement me
      return false;
    }

    bool
    RejectExitMessage::DecodeKey(llarp_buffer_t k, llarp_buffer_t* buf)
    {
      (void)k;
      (void)buf;
      // TODO: implement me
      return false;
    }

    bool
    RejectExitMessage::HandleMessage(IMessageHandler* h, llarp_router* r) const
    {
      return h->HandleRejectExitMessage(this, r);
    }

  }  // namespace routing
}  // namespace llarp