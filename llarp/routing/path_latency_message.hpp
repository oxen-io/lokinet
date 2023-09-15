#pragma once

#include "message.hpp"

namespace llarp::routing
{
  struct PathLatencyMessage final : public AbstractRoutingMessage
  {
    uint64_t sent_time = 0;
    uint64_t latency = 0;
    PathLatencyMessage();

    std::string
    bt_encode() const override;

    bool
    decode_key(const llarp_buffer_t& key, llarp_buffer_t* val) override;

    void
    clear() override
    {
      sent_time = 0;
      latency = 0;
      version = 0;
    }

    bool
    handle_message(AbstractRoutingMessageHandler* h, Router* r) const override;
  };
}  // namespace llarp::routing
