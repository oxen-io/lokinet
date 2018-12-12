#ifndef LLARP_MESSAGES_DHT_IMMEDIATE_HPP
#define LLARP_MESSAGES_DHT_IMMEDIATE_HPP
#include <dht.hpp>
#include <link_message.hpp>

#include <vector>

namespace llarp
{
  struct DHTImmeidateMessage : public ILinkMessage
  {
    DHTImmeidateMessage(ILinkSession* parent) : ILinkMessage(parent)
    {
    }

    DHTImmeidateMessage() : ILinkMessage()
    {
    }

    ~DHTImmeidateMessage();

    std::vector< std::unique_ptr< llarp::dht::IMessage > > msgs;

    bool
    DecodeKey(llarp_buffer_t key, llarp_buffer_t* buf);

    bool
    BEncode(llarp_buffer_t* buf) const;

    bool
    HandleMessage(llarp::Router* router) const;
  };
}  // namespace llarp

#endif
