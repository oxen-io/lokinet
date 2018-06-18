#ifndef LLARP_MESSAGES_RELAY_ACK_HPP
#define LLARP_MESSAGES_RELAY_ACK_HPP
#include <llarp/crypto.hpp>
#include <llarp/encrypted_frame.hpp>
#include <llarp/link_message.hpp>
#include <llarp/path_types.hpp>

namespace llarp
{
  struct LR_AckRecord
  {
    uint64_t version = 0;

    bool
    BEncode(llarp_buffer_t* buf) const;

    bool
    BDecode(llarp_buffer_t* buf);
  };

  struct LR_AckMessage : public ILinkMessage
  {
    std::vector< EncryptedFrame > replies;
    uint64_t version = 0;

    LR_AckMessage(const RouterID& from);

    ~LR_AckMessage();

    bool
    DecodeKey(llarp_buffer_t key, llarp_buffer_t* buf);

    bool
    BEncode(llarp_buffer_t* buf) const;

    bool
    HandleMessage(llarp_router* router) const;
  };
}  // namespace llarp

#endif
