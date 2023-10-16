#pragma once

#include <llarp/crypto/types.hpp>
#include "policy.hpp"

#include <vector>

namespace llarp::routing
{
  struct ObtainExitMessage final : public AbstractRoutingMessage
  {
    uint64_t flag{0};  // 0 for snode, 1 for internet access
    llarp::PubKey pubkey;
    uint64_t tx_id{0};
    llarp::Signature sig;

    ObtainExitMessage() : AbstractRoutingMessage()
    {}

    ~ObtainExitMessage() override = default;

    void
    clear() override
    {
      flag = 0;
      pubkey.Zero();
      tx_id = 0;
      sig.Zero();
    }

    /// populates I and signs
    bool
    Sign(const llarp::SecretKey& sk);

    bool
    Verify() const;

    std::string
    bt_encode() const override;

    bool
    decode_key(const llarp_buffer_t& key, llarp_buffer_t* buf) override;

    bool
    handle_message(AbstractRoutingMessageHandler* h, Router* r) const override;
  };

  struct GrantExitMessage final : public AbstractRoutingMessage
  {
    uint64_t tx_id;
    llarp::AlignedBuffer<16> nonce;
    llarp::Signature sig;

    std::string
    bt_encode() const override;

    bool
    Sign(const llarp::SecretKey& sk);

    bool
    Verify(const llarp::PubKey& pk) const;

    bool
    decode_key(const llarp_buffer_t& key, llarp_buffer_t* buf) override;

    bool
    handle_message(AbstractRoutingMessageHandler* h, Router* r) const override;

    void
    clear() override
    {
      tx_id = 0;
      nonce.Zero();
      sig.Zero();
    }
  };

  struct RejectExitMessage final : public AbstractRoutingMessage
  {
    uint64_t backoff_time;
    uint64_t tx_id;
    llarp::AlignedBuffer<16> nonce;
    llarp::Signature sig;

    void
    clear() override
    {
      backoff_time = 0;
      tx_id = 0;
      nonce.Zero();
      sig.Zero();
    }

    bool
    Sign(const llarp::SecretKey& sk);

    bool
    Verify(const llarp::PubKey& pk) const;

    std::string
    bt_encode() const override;

    bool
    decode_key(const llarp_buffer_t& key, llarp_buffer_t* buf) override;

    bool
    handle_message(AbstractRoutingMessageHandler* h, Router* r) const override;
  };

  struct UpdateExitVerifyMessage final : public AbstractRoutingMessage
  {
    uint64_t tx_id;
    llarp::AlignedBuffer<16> nonce;
    llarp::Signature sig;

    ~UpdateExitVerifyMessage() override = default;

    void
    clear() override
    {
      tx_id = 0;
      nonce.Zero();
      sig.Zero();
    }

    std::string
    bt_encode() const override;

    bool
    decode_key(const llarp_buffer_t& key, llarp_buffer_t* buf) override;

    bool
    handle_message(AbstractRoutingMessageHandler* h, Router* r) const override;
  };

  struct UpdateExitMessage final : public AbstractRoutingMessage
  {
    llarp::PathID_t path_id;
    uint64_t tx_id;
    llarp::AlignedBuffer<16> nonce;
    llarp::Signature sig;

    bool
    Sign(const llarp::SecretKey& sk);

    bool
    Verify(const llarp::PubKey& pk) const;

    std::string
    bt_encode() const override;

    bool
    decode_key(const llarp_buffer_t& key, llarp_buffer_t* buf) override;

    bool
    handle_message(AbstractRoutingMessageHandler* h, Router* r) const override;

    void
    clear() override
    {
      path_id.Zero();
      tx_id = 0;
      nonce.Zero();
      sig.Zero();
    }
  };

  struct CloseExitMessage final : public AbstractRoutingMessage
  {
    llarp::AlignedBuffer<16> nonce;
    llarp::Signature sig;

    std::string
    bt_encode() const override;

    bool
    decode_key(const llarp_buffer_t& key, llarp_buffer_t* buf) override;

    bool
    handle_message(AbstractRoutingMessageHandler* h, Router* r) const override;

    bool
    Sign(const llarp::SecretKey& sk);

    bool
    Verify(const llarp::PubKey& pk) const;

    void
    clear() override
    {
      nonce.Zero();
      sig.Zero();
    }
  };
}  // namespace llarp::routing
