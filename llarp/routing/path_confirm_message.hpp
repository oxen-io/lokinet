#pragma once

#include "message.hpp"

namespace llarp::routing
{
  struct PathConfirmMessage final : public AbstractRoutingMessage
  {
    llarp_time_t path_lifetime = 0s;
    llarp_time_t path_created_time = 0s;

    PathConfirmMessage() = default;
    PathConfirmMessage(llarp_time_t lifetime);
    ~PathConfirmMessage() override = default;

    std::string
    bt_encode() const override;

    bool
    decode_key(const llarp_buffer_t& key, llarp_buffer_t* val) override;

    bool
    handle_message(AbstractRoutingMessageHandler* h, AbstractRouter* r) const override;

    void
    clear() override
    {
      path_lifetime = 0s;
      path_created_time = 0s;
      version = 0;
    }
  };
}  // namespace llarp::routing
