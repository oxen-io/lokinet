#ifndef LLARP_LINK_MESSAGE_HPP
#define LLARP_LINK_MESSAGE_HPP

#include <link/session.hpp>
#include <router_id.hpp>
#include <util/bencode.hpp>
#include <path/path_types.hpp>

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
    uint64_t version      = LLARP_PROTO_VERSION;

    PathID_t pathid;

    ILinkMessage() = default;

    virtual ~ILinkMessage() = default;

    virtual bool
    DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* val) = 0;

    bool
    BDecode(llarp_buffer_t* buf)
    {
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
  };

}  // namespace llarp

#endif
