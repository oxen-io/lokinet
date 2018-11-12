#include <llarp/messages/exit.hpp>
#include <llarp/routing/handler.hpp>

namespace llarp
{
  namespace routing
  {
    bool
    UpdateExitMessage::BEncode(llarp_buffer_t* buf) const
    {
      (void)buf;
      // TODO: implement me
      return false;
    }

    bool
    UpdateExitMessage::DecodeKey(llarp_buffer_t k, llarp_buffer_t* buf)
    {
      (void)k;
      (void)buf;
      // TODO: implement me
      return false;
    }

    bool
    UpdateExitMessage::HandleMessage(IMessageHandler* h, llarp_router* r) const
    {
      return h->HandleUpdateExitMessage(this, r);
    }

  }  // namespace routing
}  // namespace llarp