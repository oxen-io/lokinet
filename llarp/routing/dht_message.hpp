#pragma once

#include <llarp/dht/message.hpp>
#include "message.hpp"

#include <vector>

namespace llarp::routing
{
  struct DHTMessage final : public AbstractRoutingMessage
  {
    std::vector<llarp::dht::AbstractDHTMessage::Ptr_t> M;
    uint64_t V = 0;

    ~DHTMessage() override = default;

    bool
    DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* val) override;

    bool
    BEncode(llarp_buffer_t* buf) const override;

    bool
    HandleMessage(AbstractRoutingMessageHandler* h, AbstractRouter* r) const override;

    void
    Clear() override
    {
      M.clear();
      V = 0;
    }
  };
}  // namespace llarp::routing
