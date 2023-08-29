#pragma once
#include <llarp/constants/proto.hpp>
#include <llarp/dht/message.hpp>
#include <llarp/router_contact.hpp>
#include <llarp/util/copy_or_nullptr.hpp>
#include <utility>
#include <vector>

namespace llarp::dht
{
  struct GotRouterMessage final : public AbstractDHTMessage
  {
    GotRouterMessage(const Key_t& from, bool tunneled) : AbstractDHTMessage(from), relayed(tunneled)
    {}
    GotRouterMessage(
        const Key_t& from, uint64_t id, const std::vector<RouterContact>& results, bool tunneled)
        : AbstractDHTMessage(from), foundRCs(results), txid(id), relayed(tunneled)
    {}

    GotRouterMessage(const Key_t& from, const Key_t& closer, uint64_t id, bool tunneled)
        : AbstractDHTMessage(from), closerTarget(new Key_t(closer)), txid(id), relayed(tunneled)
    {}

    GotRouterMessage(uint64_t id, std::vector<RouterID> _near, bool tunneled)
        : AbstractDHTMessage({}), nearKeys(std::move(_near)), txid(id), relayed(tunneled)
    {}

    /// gossip message
    GotRouterMessage(const RouterContact rc) : AbstractDHTMessage({}), foundRCs({rc}), txid(0)
    {
      version = llarp::constants::proto_version;
    }

    GotRouterMessage(const GotRouterMessage& other)
        : AbstractDHTMessage(other.From)
        , foundRCs(other.foundRCs)
        , nearKeys(other.nearKeys)
        , closerTarget(copy_or_nullptr(other.closerTarget))
        , txid(other.txid)
        , relayed(other.relayed)
    {
      version = other.version;
    }

    ~GotRouterMessage() override;

    void
    bt_encode(oxenc::bt_dict_producer& btdp) const override;

    bool
    decode_key(const llarp_buffer_t& key, llarp_buffer_t* val) override;

    bool
    handle_message(
        llarp_dht_context* ctx,
        std::vector<std::unique_ptr<AbstractDHTMessage>>& replies) const override;

    std::vector<RouterContact> foundRCs;
    std::vector<RouterID> nearKeys;
    std::unique_ptr<Key_t> closerTarget;
    uint64_t txid = 0;
    bool relayed = false;
  };

  using GotRouterMessage_constptr = std::shared_ptr<const GotRouterMessage>;
}  // namespace llarp::dht
