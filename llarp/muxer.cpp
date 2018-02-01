#include <llarp/msg_handler.h>
#include "router.hpp"
#include "link_handlers.hpp"

namespace llarp
{
  struct llarp_frame_handler introduce_handler = {
    .paths = nullptr,
    .parent = nullptr,
    .process = &llarp::frame::process_intro
  };

  struct llarp_frame_handler lrdm_handler = {
    .paths = nullptr,
    .parent = nullptr,
    .process = &llarp::frame::process_relay_down
  };

  struct llarp_frame_handler lrum_handler = {
    .paths = nullptr,
    .parent = nullptr,
    .process = &llarp::frame::process_relay_up
  };

  static struct llarp_frame_handler * find_frame_handler(struct llarp_router * r, const char ch)
  {
    struct llarp_frame_handler * handler = nullptr;
    switch(ch)
    {
    case 'i':
      handler = &introduce_handler;
    }
    if(handler)
    {
      handler->paths = r->paths;
      handler->parent = &r->muxer;
    }
    return handler;
  }

  static struct llarp_msg_handler * find_msg_handler(struct llarp_router * r, const char ch)
  {
    return nullptr;
  }
}

extern "C" {
  void llarp_msg_muxer_init(struct llarp_msg_muxer * muxer)
  {
    muxer->link_handler_for = &llarp::find_frame_handler;
    muxer->routing_handler_for = &llarp::find_msg_handler;
  }
}
