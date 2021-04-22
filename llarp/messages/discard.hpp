#pragma once

#include "link_message.hpp"
#include <llarp/routing/handler.hpp>
#include <llarp/routing/message.hpp>
#include <llarp/util/bencode.hpp>

namespace llarp
{
  struct DiscardMessage final : public ILinkMessage
  {
    DiscardMessage() : ILinkMessage()
    {}

    bool
    BEncode(llarp_buffer_t* buf) const override
    {
      if (!bencode_start_dict(buf))
        return false;
      if (!bencode_write_bytestring(buf, "a", 1))
        return false;
      if (!bencode_write_bytestring(buf, "x", 1))
        return false;
      return bencode_end(buf);
    }

    void
    Clear() override
    {
      version = 0;
    }

    const char*
    Name() const override
    {
      return "Discard";
    }

    bool
    DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* buf) override
    {
      if (key == "a")
      {
        llarp_buffer_t strbuf;
        if (!bencode_read_string(buf, &strbuf))
          return false;
        if (strbuf.sz != 1)
          return false;
        return *strbuf.cur == 'x';
      }
      return false;
    }

    bool
    HandleMessage(AbstractRouter* /*router*/) const override
    {
      return true;
    }
  };

  namespace routing
  {
    struct DataDiscardMessage final : public IMessage
    {
      PathID_t P;

      DataDiscardMessage() = default;

      DataDiscardMessage(const PathID_t& dst, uint64_t s) : P(dst)
      {
        S = s;
        version = LLARP_PROTO_VERSION;
      }

      void
      Clear() override
      {
        version = 0;
      }

      bool
      HandleMessage(IMessageHandler* h, AbstractRouter* r) const override
      {
        return h->HandleDataDiscardMessage(*this, r);
      }

      bool
      DecodeKey(const llarp_buffer_t& k, llarp_buffer_t* buf) override
      {
        bool read = false;
        if (!BEncodeMaybeReadDictEntry("P", P, read, k, buf))
          return false;
        if (!BEncodeMaybeReadDictInt("S", S, read, k, buf))
          return false;
        if (!BEncodeMaybeReadDictInt("V", version, read, k, buf))
          return false;
        return read;
      }

      bool
      BEncode(llarp_buffer_t* buf) const override
      {
        if (!bencode_start_dict(buf))
          return false;

        if (!BEncodeWriteDictMsgType(buf, "A", "D"))
          return false;
        if (!BEncodeWriteDictEntry("P", P, buf))
          return false;
        if (!BEncodeWriteDictInt("S", S, buf))
          return false;
        if (!BEncodeWriteDictInt("V", version, buf))
          return false;

        return bencode_end(buf);
      }
    };
  }  // namespace routing

}  // namespace llarp
