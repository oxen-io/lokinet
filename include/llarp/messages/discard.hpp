#ifndef LLARP_MESSAGES_DISCARD_HPP
#define LLARP_MESSAGES_DISCARD_HPP
#include <llarp/link_message.hpp>
#include <llarp/routing/message.hpp>
#include <llarp/bencode.hpp>
#include <llarp/routing/handler.hpp>

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

  namespace routing
  {
    struct DataDiscardMessage : public IMessage
    {
      PathID_t P;

      DataDiscardMessage() : IMessage()
      {
      }

      DataDiscardMessage(const PathID_t& src, const PathID_t& dst, uint64_t s)
          : P(dst)
      {
        from    = src;
        S       = s;
        version = LLARP_PROTO_VERSION;
      }

      bool
      HandleMessage(IMessageHandler* h, llarp_router* r) const
      {
        return h->HandleDataDiscardMessage(this, r);
      }

      bool
      DecodeKey(llarp_buffer_t k, llarp_buffer_t* buf)
      {
        bool read = false;
        if(!BEncodeMaybeReadDictEntry("P", P, read, k, buf))
          return false;
        if(!BEncodeMaybeReadDictInt("S", S, read, k, buf))
          return false;
        if(!BEncodeMaybeReadDictInt("V", version, read, k, buf))
          return false;
        return read;
      }

      bool
      BEncode(llarp_buffer_t* buf) const
      {
        if(!bencode_start_dict(buf))
          return false;

        if(!BEncodeWriteDictMsgType(buf, "A", "D"))
          return false;
        if(!BEncodeWriteDictEntry("P", P, buf))
          return false;
        if(!BEncodeWriteDictInt("S", S, buf))
          return false;
        if(!BEncodeWriteDictInt("V", version, buf))
          return false;

        return bencode_end(buf);
      }
    };
  }  // namespace routing

}  // namespace llarp

#endif
