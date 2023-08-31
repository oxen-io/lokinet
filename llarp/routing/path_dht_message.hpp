#pragma once

#include <llarp/dht/message.hpp>
#include "message.hpp"

#include <vector>

namespace llarp::routing
{
  struct PathDHTMessage final : public AbstractRoutingMessage
  {
    std::vector<std::unique_ptr<dht::AbstractDHTMessage>> dht_msgs;

    ~PathDHTMessage() override = default;

    bool
    decode_key(const llarp_buffer_t& key, llarp_buffer_t* val) override;

    std::string
    bt_encode() const override;

    bool
    handle_message(AbstractRoutingMessageHandler* h, AbstractRouter* r) const override;

    void
    clear() override
    {
      dht_msgs.clear();
    }
  };
}  // namespace llarp::routing
