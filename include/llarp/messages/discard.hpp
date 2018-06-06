#ifndef LLARP_MESSAGES_DISCARD_HPP
#define LLARP_MESSAGES_DISCARD_HPP
#include <llarp/link_message.hpp>

namespace llarp
{
  const std::size_t MAX_DISCARD_SIZE = 10000;

  /// a dummy link message that is discarded
  struct DiscardMessage : public ILinkMessage
  {
    byte_t pad[MAX_DISCARD_SIZE];
    size_t sz = 0;

    DiscardMessage(const RouterID& id) : ILinkMessage(id)
    {
    }

    DiscardMessage(std::size_t padding) : ILinkMessage()
    {
      sz = padding;
      memset(pad, 'z', sz);
    }

    virtual bool
    DecodeKey(llarp_buffer_t key, llarp_buffer_t* buf)
    {
      llarp_buffer_t strbuf;
      if(llarp_buffer_eq(key, "v"))
      {
        if(!bencode_read_integer(buf, &version))
          return false;
        return version == LLARP_PROTO_VERSION;
      }
      if(llarp_buffer_eq(key, "z"))
      {
        if(!bencode_read_string(buf, &strbuf))
          return false;
        if(strbuf.sz > MAX_DISCARD_SIZE)
          return false;
        sz = strbuf.sz;
        memcpy(pad, strbuf.base, sz);
        return true;
      }
      return false;
    }

    virtual bool
    BEncode(llarp_buffer_t* buf) const
    {
      if(!bencode_start_dict(buf))
        return false;

      if(!bencode_write_bytestring(buf, "a", 1))
        return false;
      if(!bencode_write_bytestring(buf, "z", 1))
        return false;

      if(!bencode_write_version_entry(buf))
        return false;

      if(!bencode_write_bytestring(buf, "z", 1))
        return false;
      if(!bencode_write_bytestring(buf, pad, sz))
        return false;

      return bencode_end(buf);
    }

    virtual bool
    HandleMessage(llarp_router* router) const
    {
      (void)router;
      llarp::Info("got discard message of size ", sz, " bytes");
      return true;
    }
  };
}

#endif
