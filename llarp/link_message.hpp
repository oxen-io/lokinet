#ifndef LLARP_LINK_MESSAGE_HPP
#define LLARP_LINK_MESSAGE_HPP

#include <bencode.hpp>
#include <link/session.hpp>
#include <router_id.hpp>

#include <queue>
#include <vector>

namespace llarp
{
  struct ILinkSession;
  struct Router;

  using SendQueue = std::queue< ILinkMessage* >;

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
    HandleMessage(Router* router) const = 0;

    virtual void
    Clear() = 0;
  };

}  // namespace llarp

#endif
