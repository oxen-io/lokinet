#pragma once

#include <llarp/crypto/encrypted_frame.hpp>
#include <llarp/crypto/types.hpp>
#include <llarp/messages/link_message.hpp>
#include <llarp/path/path_types.hpp>
#include <llarp/pow.hpp>

#include <array>
#include <memory>
#include <utility>

namespace llarp
{
  // forward declare
  struct Router;
  namespace path
  {
    struct PathContext;
  }

  struct LR_CommitRecord
  {
    PubKey commkey;
    RouterID nextHop;
    TunnelNonce tunnelNonce;
    PathID_t txid, rxid;

    std::unique_ptr<RouterContact> nextRC;
    std::unique_ptr<PoW> work;
    uint64_t version = 0;
    llarp_time_t lifetime = 0s;

    bool
    BDecode(llarp_buffer_t* buf);

    bool
    BEncode(llarp_buffer_t* buf) const;

    bool
    operator==(const LR_CommitRecord& other) const;

   private:
    bool
    OnKey(llarp_buffer_t* buffer, llarp_buffer_t* key);
  };

  struct LR_CommitMessage final : public AbstractLinkMessage
  {
    std::array<EncryptedFrame, 8> frames;

    LR_CommitMessage(std::array<EncryptedFrame, 8> _frames)
        : AbstractLinkMessage(), frames(std::move(_frames))
    {}

    LR_CommitMessage() = default;

    ~LR_CommitMessage() override = default;

    void
    clear() override;

    bool
    decode_key(const llarp_buffer_t& key, llarp_buffer_t* buf) override;

    std::string
    bt_encode() const override;

    bool
    handle_message(Router* router) const override;

    bool
    AsyncDecrypt(llarp::path::PathContext* context) const;

    const char*
    name() const override
    {
      return "RelayCommit";
    }

    uint16_t
    priority() const override
    {
      return 5;
    }
  };
}  // namespace llarp
