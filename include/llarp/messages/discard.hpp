#ifndef LLARP_MESSAGES_DISCARD_HPP
#define LLARP_MESSAGES_DISCARD_HPP

#include <link_message.hpp>
#include <llarp/bencode.hpp>
#include <routing/handler.hpp>
#include <routing/message.hpp>

namespace llarp
{
  struct DiscardMessage final : public ILinkMessage
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
    BEncode(llarp_buffer_t* buf) const override
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
    DecodeKey(__attribute__((unused)) llarp_buffer_t key,
              __attribute__((unused)) llarp_buffer_t* buf) override
    {
      return false;
    }

    bool
    HandleMessage(__attribute__((unused)) llarp::Router* router) const override
    {
      return true;
    }
  };

  namespace routing
  {
    struct DataDiscardMessage final : public IMessage
    {
      PathID_t P;

      DataDiscardMessage() : IMessage()
      {
      }

      DataDiscardMessage(const PathID_t& dst, uint64_t s) : P(dst)
      {
        S       = s;
        version = LLARP_PROTO_VERSION;
      }

      bool
      HandleMessage(IMessageHandler* h, llarp::Router* r) const override
      {
        return h->HandleDataDiscardMessage(this, r);
      }

      bool
      DecodeKey(llarp_buffer_t k, llarp_buffer_t* buf) override
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
      BEncode(llarp_buffer_t* buf) const override
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
