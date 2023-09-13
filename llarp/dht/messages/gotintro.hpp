#pragma once

#include <llarp/dht/message.hpp>
#include <llarp/service/intro_set.hpp>
#include <llarp/util/copy_or_nullptr.hpp>

#include <vector>
#include <optional>

namespace llarp::dht
{
  /// acknowledgement to PublishIntroMessage or reply to FindIntroMessage
  struct GotIntroMessage : public AbstractDHTMessage
  {
    /// the found introsets
    std::vector<service::EncryptedIntroSet> found;
    /// txid
    uint64_t txid = 0;
    /// the key of a router closer in keyspace if iterative lookup
    std::optional<Key_t> closer;

    GotIntroMessage(const Key_t& from) : AbstractDHTMessage(from)
    {}

    GotIntroMessage(const GotIntroMessage& other)
        : AbstractDHTMessage(other.From), found(other.found), txid(other.txid), closer(other.closer)
    {
      version = other.version;
    }

    /// for iterative reply
    GotIntroMessage(const Key_t& from, const Key_t& _closer, uint64_t _txid)
        : AbstractDHTMessage(from), txid(_txid), closer(_closer)
    {}

    /// for recursive reply
    GotIntroMessage(std::vector<service::EncryptedIntroSet> results, uint64_t txid);

    ~GotIntroMessage() override = default;

    void
    bt_encode(oxenc::bt_dict_producer& btdp) const override;

    bool
    decode_key(const llarp_buffer_t& key, llarp_buffer_t* val) override;

    bool
    handle_message(
        AbstractDHTMessageHandler& dht,
        std::vector<std::unique_ptr<AbstractDHTMessage>>& replies) const override;
  };

  struct RelayedGotIntroMessage final : public GotIntroMessage
  {
    RelayedGotIntroMessage() : GotIntroMessage({})
    {}

    bool
    handle_message(
        AbstractDHTMessageHandler& dht,
        std::vector<std::unique_ptr<AbstractDHTMessage>>& replies) const override;
  };

  using GotIntroMessage_constptr = std::shared_ptr<const GotIntroMessage>;
}  // namespace llarp::dht
