#ifndef LLARP_MESSAGES_DISCARD_HPP
#define LLARP_MESSAGES_DISCARD_HPP
#include <llarp/link_message.hpp>

namespace llarp
{
  /// a dummy link message that is discarded
  struct DiscardMessage : public ILinkMessage
  {
    std::vector< byte_t > Z;

    ~DiscardMessage()
    {
    }

    DiscardMessage(const RouterID& id) : ILinkMessage(id)
    {
    }

    DiscardMessage(const RouterID& other, std::size_t padding)
        : ILinkMessage(other)
    {
      Z.resize(padding);
      std::fill(Z.begin(), Z.end(), 'z');
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
        Z.resize(strbuf.sz);
        memcpy(Z.data(), strbuf.base, strbuf.sz);
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
      if(!bencode_write_bytestring(buf, Z.data(), Z.size()))
        return false;

      return bencode_end(buf);
    }

    virtual bool
    HandleMessage(llarp_router* router) const
    {
      (void)router;
      llarp::Info("got discard message of size ", Z.size(), " bytes");
      return true;
    }
  };
}

#endif
