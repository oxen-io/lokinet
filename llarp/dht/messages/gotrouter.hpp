#pragma once
#include <llarp/constants/proto.hpp>
#include <llarp/dht/message.hpp>
#include <llarp/router_contact.hpp>
#include <llarp/util/copy_or_nullptr.hpp>
#include <utility>
#include <vector>

namespace llarp
{
  namespace dht
  {
    struct GotRouterMessage final : public IMessage
    {
      GotRouterMessage(const Key_t& from, bool tunneled) : IMessage(from), relayed(tunneled)
      {}
      GotRouterMessage(
          const Key_t& from, uint64_t id, const std::vector<RouterContact>& results, bool tunneled)
          : IMessage(from), foundRCs(results), txid(id), relayed(tunneled)
      {}

      GotRouterMessage(const Key_t& from, const Key_t& closer, uint64_t id, bool tunneled)
          : IMessage(from), closerTarget(new Key_t(closer)), txid(id), relayed(tunneled)
      {}

      GotRouterMessage(uint64_t id, std::vector<RouterID> _near, bool tunneled)
          : IMessage({}), nearKeys(std::move(_near)), txid(id), relayed(tunneled)
      {}

      /// gossip message
      GotRouterMessage(const RouterContact rc) : IMessage({}), foundRCs({rc}), txid(0)
      {
        version = LLARP_PROTO_VERSION;
      }

      GotRouterMessage(const GotRouterMessage& other)
          : IMessage(other.From)
          , foundRCs(other.foundRCs)
          , nearKeys(other.nearKeys)
          , closerTarget(copy_or_nullptr(other.closerTarget))
          , txid(other.txid)
          , relayed(other.relayed)
      {
        version = other.version;
      }

      ~GotRouterMessage() override;

      bool
      BEncode(llarp_buffer_t* buf) const override;

      bool
      DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* val) override;

      bool
      HandleMessage(
          llarp_dht_context* ctx, std::vector<std::unique_ptr<IMessage>>& replies) const override;

      std::vector<RouterContact> foundRCs;
      std::vector<RouterID> nearKeys;
      std::unique_ptr<Key_t> closerTarget;
      uint64_t txid = 0;
      bool relayed = false;
    };

    using GotRouterMessage_constptr = std::shared_ptr<const GotRouterMessage>;
  }  // namespace dht
}  // namespace llarp
