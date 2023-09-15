#pragma once

#include <llarp/router_id.hpp>
#include <llarp/util/bencode.h>

#include <memory>

namespace llarp
{
  struct Router;
  struct AbstractLinkMessage;
  struct AbstractLinkSession;

  struct LinkMessageParser
  {
    LinkMessageParser(Router* router);
    ~LinkMessageParser();

    bool
    operator()(llarp_buffer_t* buffer, llarp_buffer_t* key);

    /// start processig message from a link session
    bool
    ProcessFrom(AbstractLinkSession* from, const llarp_buffer_t& buf);

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
    AbstractLinkSession* from;
    AbstractLinkMessage* msg;

    struct msg_holder_t;

    std::unique_ptr<msg_holder_t> holder;
  };
}  // namespace llarp
