#ifndef LLARP_MESSAGES_DHT_IMMEDIATE_HPP
#define LLARP_MESSAGES_DHT_IMMEDIATE_HPP
#include <llarp/dht.hpp>
#include <llarp/link_message.hpp>
#include <vector>

namespace llarp
{
  struct DHTImmeidateMessage : public ILinkMessage
  {
    DHTImmeidateMessage(const RouterID& from) : ILinkMessage(from)
    {
    }

    ~DHTImmeidateMessage();

    std::vector< llarp::dht::IMessage* > msgs;

    bool
    DecodeKey(llarp_buffer_t key, llarp_buffer_t* buf);

    bool
    BEncode(llarp_buffer_t* buf) const;

    bool
    HandleMessage(llarp_router* router) const;
  };
}

#endif
