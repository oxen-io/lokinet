#ifndef LLARP_LINK_MESSAGE_HPP
#define LLARP_LINK_MESSAGE_HPP

#include <llarp/bencode.h>
#include <llarp/link.h>
#include <llarp/aligned.hpp>

#include <queue>
#include <vector>

namespace llarp
{
  typedef AlignedBuffer< 32 > RouterID;

  struct ILinkMessage;

  typedef std::queue< ILinkMessage* > SendQueue;

  /// parsed link layer message
  struct ILinkMessage
  {
    /// who did this message come from (rc.k)
    RouterID remote  = {};
    uint64_t version = 0;

    ILinkMessage() = default;
    ILinkMessage(const RouterID& id);

    virtual ~ILinkMessage(){};

    virtual bool
    DecodeKey(llarp_buffer_t key, llarp_buffer_t* buf) = 0;

    virtual bool
    BEncode(llarp_buffer_t* buf) const = 0;

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
    ProcessFrom(llarp_link_session* from, llarp_buffer_t buf);

    /// called when the message is fully read
    /// return true when the message was accepted otherwise returns false
    bool
    MessageDone();

   private:
    RouterID
    GetCurrentFrom();

   private:
    bool firstkey;
    llarp_router* router;
    llarp_link_session* from;
    ILinkMessage* msg = nullptr;
  };
}

#endif
