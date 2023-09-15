#pragma once

#include <llarp/dht/message.hpp>
#include "link_message.hpp"

#include <vector>

namespace llarp
{
  struct DHTImmediateMessage final : public AbstractLinkMessage
  {
    DHTImmediateMessage() = default;
    ~DHTImmediateMessage() override = default;

    std::vector<std::unique_ptr<dht::AbstractDHTMessage>> msgs;

    std::string
    bt_encode() const override;

    bool
    decode_key(const llarp_buffer_t& key, llarp_buffer_t* buf) override;

    bool
    handle_message(Router* router) const override;

    void
    clear() override;

    const char*
    name() const override
    {
      return "DHTImmediate";
    }
  };
}  // namespace llarp
