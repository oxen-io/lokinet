#pragma once

#include <llarp/crypto/encrypted.hpp>
#include <llarp/crypto/types.hpp>
#include "message.hpp"
#include <llarp/service/protocol.hpp>

namespace llarp::routing
{
  struct PathTransferMessage final : public AbstractRoutingMessage
  {
    PathID_t path_id;
    service::ProtocolFrameMessage protocol_frame_msg;
    TunnelNonce nonce;

    PathTransferMessage() = default;
    PathTransferMessage(const service::ProtocolFrameMessage& f, const PathID_t& p)
        : path_id(p), protocol_frame_msg(f)
    {
      nonce.Randomize();
    }
    ~PathTransferMessage() override = default;

    bool
    decode_key(const llarp_buffer_t& key, llarp_buffer_t* val) override;

    std::string
    bt_encode() const override;

    bool
    handle_message(AbstractRoutingMessageHandler*, AbstractRouter* r) const override;

    void
    clear() override
    {
      path_id.Zero();
      protocol_frame_msg.clear();
      nonce.Zero();
      version = 0;
    }
  };

}  // namespace llarp::routing
