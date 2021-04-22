#pragma once

#include <llarp/dht/message.hpp>
#include "link_message.hpp"

#include <vector>

namespace llarp
{
  struct DHTImmediateMessage final : public ILinkMessage
  {
    DHTImmediateMessage() = default;
    ~DHTImmediateMessage() override = default;

    std::vector<std::unique_ptr<dht::IMessage>> msgs;

    bool
    DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* buf) override;

    bool
    BEncode(llarp_buffer_t* buf) const override;

    bool
    HandleMessage(AbstractRouter* router) const override;

    void
    Clear() override;

    const char*
    Name() const override
    {
      return "DHTImmediate";
    }
  };
}  // namespace llarp
