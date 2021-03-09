#pragma once

#include <llarp/link/session.hpp>
#include <llarp/router_id.hpp>
#include <llarp/util/bencode.hpp>
#include <llarp/path/path_types.hpp>

#include <vector>

namespace llarp
{
  struct ILinkSession;
  struct AbstractRouter;

  /// parsed link layer message
  struct ILinkMessage
  {
    /// who did this message come from or is going to
    ILinkSession* session = nullptr;
    uint64_t version = LLARP_PROTO_VERSION;

    PathID_t pathid;

    ILinkMessage() = default;

    virtual ~ILinkMessage() = default;

    virtual bool
    DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* val) = 0;

    bool
    BDecode(llarp_buffer_t* buf)
    {
      // default version if not specified is 0
      uint64_t v = 0;
      // seek for version and set it if we got it
      if (BEncodeSeekDictVersion(v, buf, 'v'))
      {
        version = v;
      }
      // when we hit the code path version is set and we can tell how to decode
      return bencode_decode_dict(*this, buf);
    }

    virtual bool
    BEncode(llarp_buffer_t* buf) const = 0;

    virtual bool
    HandleMessage(AbstractRouter* router) const = 0;

    virtual void
    Clear() = 0;

    // the name of this kind of message
    virtual const char*
    Name() const = 0;

    /// get message prority, higher value means more important
    virtual uint16_t
    Priority() const
    {
      return 1;
    }
  };

}  // namespace llarp
