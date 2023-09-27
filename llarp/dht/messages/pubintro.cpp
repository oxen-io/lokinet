#include "pubintro.hpp"

#include <llarp/dht/context.hpp>
#include "gotintro.hpp"
#include <llarp/router/router.hpp>
#include <llarp/routing/path_dht_message.hpp>
#include <llarp/nodedb.hpp>

#include <llarp/tooling/dht_event.hpp>

namespace llarp::dht
{
  const uint64_t PublishIntroMessage::MaxPropagationDepth = 5;
  PublishIntroMessage::~PublishIntroMessage() = default;

  bool
  PublishIntroMessage::decode_key(const llarp_buffer_t& key, llarp_buffer_t* val)
  {
    bool read = false;
    if (!BEncodeMaybeReadDictEntry("I", introset, read, key, val))
      return false;
    if (read)
      return true;

    if (!BEncodeMaybeReadDictInt("O", relayOrder, read, key, val))
      return false;
    if (read)
      return true;

    uint64_t relayedInt = (relayed ? 1 : 0);
    if (!BEncodeMaybeReadDictInt("R", relayedInt, read, key, val))
      return false;
    if (read)
    {
      relayed = relayedInt;
      return true;
    }

    if (!BEncodeMaybeReadDictInt("T", txID, read, key, val))
      return false;
    if (read)
      return true;

    if (!BEncodeMaybeReadDictInt("V", version, read, key, val))
      return false;
    if (read)
      return true;

    return false;
  }

  bool
  PublishIntroMessage::handle_message(
      AbstractDHTMessageHandler& dht,
      std::vector<std::unique_ptr<AbstractDHTMessage>>& replies) const
  {
    const auto now = dht.Now();
    const llarp::dht::Key_t addr{introset.derivedSigningKey.data()};

    auto router = dht.GetRouter();
    router->notify_router_event<tooling::PubIntroReceivedEvent>(
        router->pubkey(), Key_t(relayed ? router->pubkey() : From.data()), addr, txID, relayOrder);

    if (!introset.verify(now))
    {
      llarp::LogWarn("Received PublishIntroMessage with invalid introset: ", introset);
      // don't propogate or store
      replies.emplace_back(new GotIntroMessage({}, txID));
      return true;
    }

    if (introset.IsExpired(now + llarp::service::MAX_INTROSET_TIME_DELTA))
    {
      // don't propogate or store
      llarp::LogWarn("Received PublishIntroMessage with expired Introset: ", introset);
      replies.emplace_back(new GotIntroMessage({}, txID));
      return true;
    }

    // identify closest 4 routers
    auto closestRCs =
        dht.GetRouter()->node_db()->FindManyClosestTo(addr, INTROSET_STORAGE_REDUNDANCY);
    if (closestRCs.size() != INTROSET_STORAGE_REDUNDANCY)
    {
      llarp::LogWarn("Received PublishIntroMessage but only know ", closestRCs.size(), " nodes");
      replies.emplace_back(new GotIntroMessage({}, txID));
      return true;
    }

    const auto& us = dht.OurKey();

    // function to identify the closest 4 routers we know of for this introset
    auto propagateIfNotUs = [&](size_t index) {
      assert(index < INTROSET_STORAGE_REDUNDANCY);

      const auto& rc = closestRCs[index];
      const Key_t peer{rc.pubkey};

      if (peer == us)
      {
        llarp::LogInfo("we are peer ", index, " so storing instead of propagating");

        dht.services()->PutNode(introset);
        replies.emplace_back(new GotIntroMessage({introset}, txID));
      }
      else
      {
        llarp::LogInfo("propagating to peer ", index);
        if (relayed)
        {
          dht.PropagateLocalIntroSet(pathID, txID, introset, peer, 0);
        }
        else
        {
          dht.PropagateIntroSetTo(From, txID, introset, peer, 0);
        }
      }
    };

    if (relayed)
    {
      if (relayOrder >= INTROSET_STORAGE_REDUNDANCY)
      {
        llarp::LogWarn("Received PublishIntroMessage with invalid relayOrder: ", relayOrder);
        replies.emplace_back(new GotIntroMessage({}, txID));
        return true;
      }

      llarp::LogInfo("Relaying PublishIntroMessage for ", addr, ", txid=", txID);

      propagateIfNotUs(relayOrder);
    }
    else
    {
      int candidateNumber = -1;
      int index = 0;
      for (const auto& rc : closestRCs)
      {
        if (rc.pubkey == dht.OurKey())
        {
          candidateNumber = index;
          break;
        }
        ++index;
      }

      if (candidateNumber >= 0)
      {
        LogInfo(
            "Received PubIntro for ",
            addr,
            ", txid=",
            txID,
            " and we are candidate ",
            candidateNumber);
        dht.services()->PutNode(introset);
        replies.emplace_back(new GotIntroMessage({introset}, txID));
      }
      else
      {
        LogWarn(
            "!!! Received PubIntro with relayed==false but we aren't"
            " candidate, intro derived key: ",
            addr,
            ", txid=",
            txID,
            ", message from: ",
            From);
      }
    }

    return true;
  }

  void
  PublishIntroMessage::bt_encode(oxenc::bt_dict_producer& btdp) const
  {
    try
    {
      btdp.append("A", "I");
      btdp.append("I", introset.ToString());
      btdp.append("O", relayOrder);
      btdp.append("R", relayed ? 1 : 0);
      btdp.append("T", txID);
      btdp.append("V", llarp::constants::proto_version);
    }
    catch (...)
    {
      log::error(dht_cat, "Error: PublishIntroMessage failed to bt encode contents!");
    }
  }
}  // namespace llarp::dht
