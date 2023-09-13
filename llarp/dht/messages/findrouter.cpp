#include "findrouter.hpp"

#include <llarp/dht/context.hpp>
#include "gotrouter.hpp"
#include <llarp/nodedb.hpp>
#include <llarp/path/path_context.hpp>
#include <llarp/router/abstractrouter.hpp>
#include <llarp/routing/path_dht_message.hpp>

#include <llarp/tooling/dht_event.hpp>

namespace llarp::dht
{
  bool
  RelayedFindRouterMessage::handle_message(
      AbstractDHTMessageHandler& dht,
      std::vector<std::unique_ptr<AbstractDHTMessage>>& replies) const
  {
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

  void
  FindRouterMessage::bt_encode(oxenc::bt_dict_producer& btdp) const
  {
    try
    {
      btdp.append("A", "R");
      btdp.append("T", exploratory ? 1 : 0);
      btdp.append("I", iterative ? 1 : 0);
      btdp.append("K", targetKey.ToView());
      btdp.append("T", txid);
      btdp.append("V", version);
    }
    catch (...)
    {
      log::error(dht_cat, "Error: FindRouterMessage failed to bt encode contents!");
    }
  }

  bool
  FindRouterMessage::decode_key(const llarp_buffer_t& key, llarp_buffer_t* val)
  {
    llarp_buffer_t strbuf;

    if (key.startswith("E"))
    {
      uint64_t result;
      if (!bencode_read_integer(val, &result))
        return false;

      exploratory = result != 0;
      return true;
    }

    if (key.startswith("I"))
    {
      uint64_t result;
      if (!bencode_read_integer(val, &result))
        return false;

      iterative = result != 0;
      return true;
    }
    if (key.startswith("K"))
    {
      if (!bencode_read_string(val, &strbuf))
        return false;
      if (strbuf.sz != targetKey.size())
        return false;

      std::copy(strbuf.base, strbuf.base + targetKey.SIZE, targetKey.begin());
      return true;
    }
    if (key.startswith("T"))
    {
      return bencode_read_integer(val, &txid);
    }
    if (key.startswith("V"))
    {
      return bencode_read_integer(val, &version);
    }
    return false;
  }

  bool
  FindRouterMessage::handle_message(
      AbstractDHTMessageHandler& dht,
      std::vector<std::unique_ptr<AbstractDHTMessage>>& replies) const
  {
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
    if (exploratory)
      return dht.HandleExploritoryRouterLookup(From, txid, targetKey, replies);
    dht.LookupRouterRelayed(From, txid, k, !iterative, replies);
    return true;
  }
}  // namespace llarp::dht
