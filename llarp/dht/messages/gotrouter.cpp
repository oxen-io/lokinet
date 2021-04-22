#include <llarp/dht/context.hpp>
#include "gotrouter.hpp"

#include <memory>
#include <llarp/path/path_context.hpp>
#include <llarp/router/abstractrouter.hpp>
#include <llarp/router/i_rc_lookup_handler.hpp>
#include <llarp/tooling/rc_event.hpp>

namespace llarp
{
  namespace dht
  {
    GotRouterMessage::~GotRouterMessage() = default;

    bool
    GotRouterMessage::BEncode(llarp_buffer_t* buf) const
    {
      if (not bencode_start_dict(buf))
        return false;

      // message type
      if (not BEncodeWriteDictMsgType(buf, "A", "S"))
        return false;

      if (closerTarget)
      {
        if (not BEncodeWriteDictEntry("K", *closerTarget, buf))
          return false;
      }

      // near
      if (not nearKeys.empty())
      {
        if (not BEncodeWriteDictList("N", nearKeys, buf))
          return false;
      }

      if (not BEncodeWriteDictList("R", foundRCs, buf))
        return false;

      // txid
      if (not BEncodeWriteDictInt("T", txid, buf))
        return false;

      // version
      if (not BEncodeWriteDictInt("V", version, buf))
        return false;

      return bencode_end(buf);
    }

    bool
    GotRouterMessage::DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* val)
    {
      if (key == "K")
      {
        if (closerTarget)  // duplicate key?
          return false;
        closerTarget = std::make_unique<dht::Key_t>();
        return closerTarget->BDecode(val);
      }
      if (key == "N")
      {
        return BEncodeReadList(nearKeys, val);
      }
      if (key == "R")
      {
        return BEncodeReadList(foundRCs, val);
      }
      if (key == "T")
      {
        return bencode_read_integer(val, &txid);
      }
      bool read = false;
      if (!BEncodeMaybeVerifyVersion("V", version, LLARP_PROTO_VERSION, read, key, val))
        return false;

      return read;
    }

    bool
    GotRouterMessage::HandleMessage(
        llarp_dht_context* ctx,
        [[maybe_unused]] std::vector<std::unique_ptr<IMessage>>& replies) const
    {
      auto& dht = *ctx->impl;
      if (relayed)
      {
        auto pathset = ctx->impl->GetRouter()->pathContext().GetLocalPathSet(pathID);
        auto copy = std::make_shared<const GotRouterMessage>(*this);
        return pathset && pathset->HandleGotRouterMessage(copy);
      }
      // not relayed
      const TXOwner owner(From, txid);

      if (dht.pendingExploreLookups().HasPendingLookupFrom(owner))
      {
        LogDebug("got ", nearKeys.size(), " results in GRM for explore");
        if (nearKeys.empty())
          dht.pendingExploreLookups().NotFound(owner, closerTarget);
        else
        {
          dht.pendingExploreLookups().Found(owner, From.as_array(), nearKeys);
        }
        return true;
      }
      // not explore lookup
      if (dht.pendingRouterLookups().HasPendingLookupFrom(owner))
      {
        LogDebug("got ", foundRCs.size(), " results in GRM for lookup");
        if (foundRCs.empty())
          dht.pendingRouterLookups().NotFound(owner, closerTarget);
        else if (foundRCs[0].pubkey.IsZero())
          return false;
        else
          dht.pendingRouterLookups().Found(owner, foundRCs[0].pubkey, foundRCs);
        return true;
      }
      // store if valid
      for (const auto& rc : foundRCs)
      {
        if (not dht.GetRouter()->rcLookupHandler().CheckRC(rc))
          return false;
        if (txid == 0)  // txid == 0 on gossip
        {
          auto* router = dht.GetRouter();
          router->NotifyRouterEvent<tooling::RCGossipReceivedEvent>(router->pubkey(), rc);
          router->GossipRCIfNeeded(rc);

          auto peerDb = router->peerDb();
          if (peerDb)
            peerDb->handleGossipedRC(rc);
        }
      }
      return true;
    }
  }  // namespace dht
}  // namespace llarp
