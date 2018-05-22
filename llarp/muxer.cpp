#include <llarp/msg_handler.h>
#include "router.hpp"

namespace llarp
{
  static struct llarp_frame_handler*
  find_frame_handler(struct llarp_router* r, const char ch)
  {
    auto itr = r->frame_handlers.find(ch);
    if(itr != r->frame_handlers.end())
    {
      auto handler    = &itr->second;
      handler->paths  = r->paths;
      handler->parent = &r->muxer;
    }
    return nullptr;
  }

  static struct llarp_msg_handler*
  find_msg_handler(struct llarp_router* r, const char ch)
  {
    return nullptr;
  }
}  // namespace llarp

extern "C" {
void
llarp_msg_muxer_init(struct llarp_msg_muxer* muxer)
{
  muxer->link_handler_for    = &llarp::find_frame_handler;
  muxer->routing_handler_for = &llarp::find_msg_handler;
}
}
