#ifndef LLARP_LINK_MESSAGE_PARSER_HPP
#define LLARP_LINK_MESSAGE_PARSER_HPP

#include <messages/discard.hpp>
#include <messages/dht_immediate.hpp>
#include <messages/link_intro.hpp>
#include <messages/link_message.hpp>
#include <messages/relay.hpp>
#include <messages/relay_commit.hpp>

namespace llarp
{
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
    ILinkMessage* msg  = nullptr;

    struct msg_holder_t
    {
      LinkIntroMessage i;
      RelayDownstreamMessage d;
      RelayUpstreamMessage u;
      DHTImmediateMessage m;
      LR_CommitMessage c;
      DiscardMessage x;
    };

    msg_holder_t holder;
  };
}  // namespace llarp
#endif
