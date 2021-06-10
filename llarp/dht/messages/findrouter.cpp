#include "findrouter.hpp"

#include <llarp/dht/context.hpp>
#include "gotrouter.hpp"
#include <llarp/nodedb.hpp>
#include <llarp/path/path_context.hpp>
#include <llarp/router/abstractrouter.hpp>
#include <llarp/routing/dht_message.hpp>

#include <llarp/tooling/dht_event.hpp>

namespace llarp
{
  namespace dht
  {
    bool
    RelayedFindRouterMessage::HandleMessage(
        llarp_dht_context* ctx, std::vector<std::unique_ptr<IMessage>>& replies) const
    {
      auto& dht = *ctx->impl;
      /// lookup for us, send an immeidate reply
      const Key_t us = dht.OurKey();
      const Key_t k{targetKey};
      if (k == us)
      {
        auto path = dht.GetRouter()->pathContext().GetByUpstream(targetKey, pathID);
        if (path)
        {
          replies.emplace_back(new GotRouterMessage(k, txid, {dht.GetRouter()->rc()}, false));
          return true;
        }
        return false;
      }

      Key_t peer;
      // check if we know this in our nodedb first
      if (not dht.GetRouter()->SessionToRouterAllowed(targetKey))
      {
        // explicitly disallowed by network
        replies.emplace_back(new GotRouterMessage(k, txid, {}, false));
        return true;
      }
      // check netdb
      const auto rc = dht.GetRouter()->nodedb()->FindClosestTo(k);
      if (rc.pubkey == targetKey)
      {
        replies.emplace_back(new GotRouterMessage(k, txid, {rc}, false));
        return true;
      }
      peer = Key_t(rc.pubkey);
      // lookup if we don't have it in our nodedb
      dht.LookupRouterForPath(targetKey, txid, pathID, peer);
      return true;
    }

    FindRouterMessage::~FindRouterMessage() = default;

    bool
    FindRouterMessage::BEncode(llarp_buffer_t* buf) const
    {
      if (!bencode_start_dict(buf))
        return false;

      // message type
      if (!bencode_write_bytestring(buf, "A", 1))
        return false;
      if (!bencode_write_bytestring(buf, "R", 1))
        return false;

      // exploritory or not?
      if (!bencode_write_bytestring(buf, "E", 1))
        return false;
      if (!bencode_write_uint64(buf, exploritory ? 1 : 0))
        return false;

      // iterative or not?
      if (!bencode_write_bytestring(buf, "I", 1))
        return false;
      if (!bencode_write_uint64(buf, iterative ? 1 : 0))
        return false;

      // key
      if (!bencode_write_bytestring(buf, "K", 1))
        return false;
      if (!bencode_write_bytestring(buf, targetKey.data(), targetKey.size()))
        return false;

      // txid
      if (!bencode_write_bytestring(buf, "T", 1))
        return false;
      if (!bencode_write_uint64(buf, txid))
        return false;

      // version
      if (!bencode_write_bytestring(buf, "V", 1))
        return false;
      if (!bencode_write_uint64(buf, version))
        return false;

      return bencode_end(buf);
    }

    bool
    FindRouterMessage::DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* val)
    {
      llarp_buffer_t strbuf;

      if (key == "E")
      {
        uint64_t result;
        if (!bencode_read_integer(val, &result))
          return false;

        exploritory = result != 0;
        return true;
      }

      if (key == "I")
      {
        uint64_t result;
        if (!bencode_read_integer(val, &result))
          return false;

        iterative = result != 0;
        return true;
      }
      if (key == "K")
      {
        if (!bencode_read_string(val, &strbuf))
          return false;
        if (strbuf.sz != targetKey.size())
          return false;

        std::copy(strbuf.base, strbuf.base + targetKey.SIZE, targetKey.begin());
        return true;
      }
      if (key == "T")
      {
        return bencode_read_integer(val, &txid);
      }
      if (key == "V")
      {
        return bencode_read_integer(val, &version);
      }
      return false;
    }

    bool
    FindRouterMessage::HandleMessage(
        llarp_dht_context* ctx, std::vector<std::unique_ptr<IMessage>>& replies) const
    {
      auto& dht = *ctx->impl;

      auto router = dht.GetRouter();
      router->NotifyRouterEvent<tooling::FindRouterReceivedEvent>(router->pubkey(), *this);

      if (!dht.AllowTransit())
      {
        llarp::LogWarn("Got DHT lookup from ", From, " when we are not allowing dht transit");
        return false;
      }
      if (dht.pendingRouterLookups().HasPendingLookupFrom({From, txid}))
      {
        llarp::LogWarn("Duplicate FRM from ", From, " txid=", txid);
        return false;
      }
      RouterContact found;
      if (targetKey.IsZero())
      {
        llarp::LogError("invalid FRM from ", From, " key is zero");
        return false;
      }
      const Key_t k(targetKey);
      if (exploritory)
        return dht.HandleExploritoryRouterLookup(From, txid, targetKey, replies);
      dht.LookupRouterRelayed(From, txid, k, !iterative, replies);
      return true;
    }
  }  // namespace dht
}  // namespace llarp
