#ifndef LLARP_LINK_MESSAGE_HPP
#define LLARP_LINK_MESSAGE_HPP

#include <link/session.hpp>
#include <router_id.hpp>
#include <util/bencode.hpp>

#include <vector>

namespace llarp
{
  struct ILinkSession;
  struct AbstractRouter;

  /// parsed link layer message
  struct ILinkMessage : public IBEncodeMessage
  {
    /// who did this message come from or is going to
    ILinkSession* session = nullptr;
    uint64_t version      = 0;

    ILinkMessage() = default;

    virtual ~ILinkMessage()
    {
    }

    virtual bool
    HandleMessage(AbstractRouter* router) const = 0;

    virtual void
    Clear() = 0;
  };

}  // namespace llarp

#endif
