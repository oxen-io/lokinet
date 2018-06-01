#include <llarp/messages/relay_commit.hpp>

namespace llarp
{
  LR_CommitMessage::~LR_CommitMessage()
  {
  }

  bool
  LR_CommitMessage::DecodeKey(llarp_buffer_t key, llarp_buffer_t* buf)
  {
    // TODO: implement
    return false;
  }

  bool
  LR_CommitMessage::BEncode(llarp_buffer_t* buf) const
  {
    // TODO: implement
    return false;
  }

  bool
  LR_CommitMessage::HandleMessage(llarp_router* router) const
  {
    // TODO: implement
    return false;
  }
}
