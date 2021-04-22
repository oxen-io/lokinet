#pragma once

#include <llarp/dht/message.hpp>
#include "message.hpp"

#include <vector>

namespace llarp
{
  namespace routing
  {
    struct DHTMessage final : public IMessage
    {
      std::vector<llarp::dht::IMessage::Ptr_t> M;
      uint64_t V = 0;

      ~DHTMessage() override = default;

      bool
      DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* val) override;

      bool
      BEncode(llarp_buffer_t* buf) const override;

      bool
      HandleMessage(IMessageHandler* h, AbstractRouter* r) const override;

      void
      Clear() override
      {
        M.clear();
        V = 0;
      }
    };
  }  // namespace routing
}  // namespace llarp
