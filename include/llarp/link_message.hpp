#ifndef LLARP_LINK_MESSAGE_HPP
#define LLARP_LINK_MESSAGE_HPP

#include <llarp/bencode.hpp>
#include <llarp/router_id.hpp>
#include <llarp/link/session.hpp>

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
    HandleMessage(Router* router) const = 0;
  };

  struct InboundMessageParser
  {
    InboundMessageParser(Router* router);
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
    Router* router;
    ILinkSession* from = nullptr;
    std::unique_ptr< ILinkMessage > msg;
  };
}  // namespace llarp

#endif
