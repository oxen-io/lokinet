#ifndef LLARP_MESSAGES_DISCARD_HPP
#define LLARP_MESSAGES_DISCARD_HPP
#include <llarp/link_message.hpp>
#include <llarp/bencode.hpp>
namespace llarp
{
  struct DiscardMessage : public ILinkMessage
  {
    /// who did this message come from or is going to

    DiscardMessage() : ILinkMessage(nullptr)
    {
    }

    DiscardMessage(ILinkSession* from) : ILinkMessage(from)
    {
    }

    ~DiscardMessage()
    {
    }

    bool
    BEncode(llarp_buffer_t* buf) const
    {
      if(!bencode_start_dict(buf))
        return false;
      if(!bencode_write_bytestring(buf, "a", 1))
        return false;
      if(!bencode_write_bytestring(buf, "x", 1))
        return false;
      return bencode_end(buf);
    }

    bool
    DecodeKey(llarp_buffer_t key, llarp_buffer_t* buf)
    {
      return false;
    }

    bool
    HandleMessage(llarp_router* router) const
    {
      return true;
    }
  };
}  // namespace llarp

#endif
