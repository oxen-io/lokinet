#ifndef LLARP_LINK_MESSAGE_HPP
#define LLARP_LINK_MESSAGE_HPP

#include <llarp/bencode.hpp>
#include <llarp/router_id.hpp>
#include <llarp/link/session.hpp>

#include <queue>
#include <vector>

struct llarp_router;

namespace llarp
{
  struct ILinkSession;

  using SendQueue = std::queue< ILinkMessage* >;

  /// parsed link layer message
  struct ILinkMessage : public IBEncodeMessage
  {
    /// who did this message come from or is going to
    ILinkSession* session;
    uint64_t version = 0;

    ILinkMessage() : ILinkMessage(nullptr)
    {
    }

    ILinkMessage(ILinkSession* from) : session(from)
    {
    }
    virtual ~ILinkMessage()
    {
    }

    virtual bool
    HandleMessage(llarp_router* router) const = 0;
  };

  struct InboundMessageParser
  {
    InboundMessageParser(llarp_router* router);
    dict_reader reader;

    static bool
    OnKey(dict_reader* r, llarp_buffer_t* buf);

    /// start processig message from a link session
    bool
    ProcessFrom(ILinkSession* from, llarp_buffer_t buf);

    /// called when the message is fully read
    /// return true when the message was accepted otherwise returns false
    bool
    MessageDone();

    /// resets internal state
    void
    Reset();

   private:
    RouterID
    GetCurrentFrom();

   private:
    bool firstkey;
    llarp_router* router;
    ILinkSession* from = nullptr;
    std::unique_ptr< ILinkMessage > msg;
  };
}  // namespace llarp

#endif
