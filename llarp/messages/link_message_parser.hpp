#pragma once

#include <llarp/router_id.hpp>
#include <llarp/util/bencode.h>

#include <memory>

namespace llarp
{
  struct AbstractRouter;
  struct ILinkMessage;
  struct ILinkSession;

  struct LinkMessageParser
  {
    LinkMessageParser(AbstractRouter* router);
    ~LinkMessageParser();

    bool
    operator()(llarp_buffer_t* buffer, llarp_buffer_t* key);

    /// start processig message from a link session
    bool
    ProcessFrom(ILinkSession* from, const llarp_buffer_t& buf);

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
    AbstractRouter* router;
    ILinkSession* from;
    ILinkMessage* msg;

    struct msg_holder_t;

    std::unique_ptr<msg_holder_t> holder;
  };
}  // namespace llarp
