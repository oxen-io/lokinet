#include <llarp/messages/relay_ack.hpp>

namespace llarp 
{
  LR_AckMessage::LR_AckMessage(const RouterID & from) : ILinkMessage(from)
  {

  }
  LR_AckMessage::~LR_AckMessage()
  {

  }

  bool LR_AckMessage::BEncode(llarp_buffer_t * buf) const
  {
    return false;
  }

  bool LR_AckMessage::DecodeKey(llarp_buffer_t key, llarp_buffer_t * buf)
  {
    return false;
  }

  bool LR_AckMessage::HandleMessage(llarp_router * router) const
  {
    return false;
  }
}