#pragma once

#include <llarp/crypto/encrypted.hpp>
#include "message.hpp"
#include <llarp/service/protocol_type.hpp>

#include <vector>

namespace llarp::routing
{
  constexpr size_t EXIT_PAD_SIZE = 512 - 48;
  constexpr size_t MAX_EXIT_MTU = 1500;
  constexpr size_t EXIT_OVERHEAD = sizeof(uint64_t);

  struct TransferTrafficMessage final : public AbstractRoutingMessage
  {
    std::vector<llarp::Encrypted<MAX_EXIT_MTU + EXIT_OVERHEAD>> enc_buf;
    service::ProtocolType protocol;
    size_t _size = 0;

    void
    clear() override
    {
      enc_buf.clear();
      _size = 0;
      version = 0;
      protocol = service::ProtocolType::TrafficV4;
    }

    size_t
    Size() const
    {
      return _size;
    }

    /// append buffer to X
    bool
    PutBuffer(const llarp_buffer_t& buf, uint64_t counter);

    std::string
    bt_encode() const override;

    bool
    decode_key(const llarp_buffer_t& k, llarp_buffer_t* val) override;

    bool
    handle_message(AbstractRoutingMessageHandler* h, Router* r) const override;
  };
}  // namespace llarp::routing
