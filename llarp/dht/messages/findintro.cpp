#include <llarp/dht/context.hpp>
#include "findintro.hpp"
#include "gotintro.hpp"
#include <llarp/routing/message.hpp>
#include <llarp/router/abstractrouter.hpp>
#include <llarp/nodedb.hpp>

namespace llarp::dht
{
  FindIntroMessage::~FindIntroMessage() = default;

  bool
  FindIntroMessage::decode_key(const llarp_buffer_t& k, llarp_buffer_t* val)
  {
    bool read = false;

    if (!BEncodeMaybeReadDictEntry("N", tagName, read, k, val))
      return false;

    if (!BEncodeMaybeReadDictInt("O", relayOrder, read, k, val))
      return false;

    if (!BEncodeMaybeReadDictEntry("S", location, read, k, val))
      return false;

    if (!BEncodeMaybeReadDictInt("T", txID, read, k, val))
      return false;

    if (!BEncodeMaybeVerifyVersion("V", version, llarp::constants::proto_version, read, k, val))
      return false;

    return read;
  }

  void
  FindIntroMessage::bt_encode(oxenc::bt_dict_producer& btdp) const
  {
    try
    {
      btdp.append("A", "F");
      if (tagName.Empty())
      {
        btdp.append("O", relayOrder);
        btdp.append("S", location.ToView());
      }
      else
      {
        btdp.append("N", tagName.ToView());
        btdp.append("O", relayOrder);
      }

      btdp.append("T", txID);
      btdp.append("V", llarp::constants::proto_version);
    }
    catch (...)
    {
      log::critical(dht_cat, "FindIntroMessage failed to bt encode contents!");
    }
  }

  bool
  FindIntroMessage::handle_message(
      AbstractDHTMessageHandler& dht,
      std::vector<std::unique_ptr<AbstractDHTMessage>>& replies) const
  {
    if (dht.pendingIntrosetLookups().HasPendingLookupFrom(TXOwner{From, txID}))
    {
      llarp::LogWarn("duplicate FIM from ", From, " txid=", txID);
      return false;
    }

    if (not tagName.Empty())
    {
      return false;
    }
    // bad request (request for zero-key)
    if (location.IsZero())
    {
      // we dont got it
      replies.emplace_back(new GotIntroMessage({}, txID));
      return true;
    }

    // we are relaying this message for e.g. a client
    if (relayed)
    {
      if (relayOrder >= IntroSetStorageRedundancy)
      {
        llarp::LogWarn("Invalid relayOrder received: ", relayOrder);
        replies.emplace_back(new GotIntroMessage({}, txID));
        return true;
      }

      auto closestRCs =
          dht.GetRouter()->nodedb()->FindManyClosestTo(location, IntroSetStorageRedundancy);

      if (closestRCs.size() <= relayOrder)
      {
        llarp::LogWarn("Can't fulfill FindIntro for relayOrder: ", relayOrder);
        replies.emplace_back(new GotIntroMessage({}, txID));
        return true;
      }

      const auto& entry = closestRCs[relayOrder];
      Key_t peer = Key_t(entry.pubkey);
      dht.LookupIntroSetForPath(location, txID, pathID, peer, 0);
    }
    else
    {
      // we should have this value if introset was propagated properly
      const auto maybe = dht.GetIntroSetByLocation(location);
      if (maybe)
      {
        replies.emplace_back(new GotIntroMessage({*maybe}, txID));
      }
      else
      {
        LogWarn("Got FIM with relayed == false and we don't have entry");
        replies.emplace_back(new GotIntroMessage({}, txID));
      }
    }
    return true;
  }
}  // namespace llarp::dht
