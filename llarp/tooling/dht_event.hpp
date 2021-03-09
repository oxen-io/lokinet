#pragma once

#include "router_event.hpp"
#include <llarp/dht/key.hpp>
#include <llarp/service/intro_set.hpp>
#include <llarp/dht/messages/findrouter.hpp>

namespace tooling
{
  struct PubIntroSentEvent : public RouterEvent
  {
    llarp::dht::Key_t introsetPubkey;
    llarp::RouterID relay;
    uint64_t relayIndex;

    PubIntroSentEvent(
        const llarp::RouterID& ourRouter,
        const llarp::dht::Key_t& introsetPubkey_,
        const llarp::RouterID& relay_,
        uint64_t relayIndex_)
        : RouterEvent("DHT: PubIntroSentEvent", ourRouter, false)
        , introsetPubkey(introsetPubkey_)
        , relay(relay_)
        , relayIndex(relayIndex_)
    {}

    std::string
    ToString() const
    {
      return RouterEvent::ToString() + " ---- introset pubkey: " + introsetPubkey.ShortHex()
          + ", relay: " + relay.ShortString() + ", relayIndex: " + std::to_string(relayIndex);
    }
  };

  struct PubIntroReceivedEvent : public RouterEvent
  {
    llarp::dht::Key_t from;
    llarp::dht::Key_t location;
    uint64_t txid;
    uint64_t relayOrder;

    PubIntroReceivedEvent(
        const llarp::RouterID& ourRouter,
        const llarp::dht::Key_t& from_,
        const llarp::dht::Key_t& location_,
        uint64_t txid_,
        uint64_t relayOrder_)
        : RouterEvent("DHT: PubIntroReceivedEvent", ourRouter, true)
        , from(from_)
        , location(location_)
        , txid(txid_)
        , relayOrder(relayOrder_)
    {}

    std::string
    ToString() const override
    {
      return RouterEvent::ToString() + "from " + from.ShortHex()
          + " location=" + location.ShortHex() + " order=" + std::to_string(relayOrder)
          + " txid=" + std::to_string(txid);
    }
  };

  struct GotIntroReceivedEvent : public RouterEvent
  {
    llarp::dht::Key_t From;
    llarp::service::EncryptedIntroSet Introset;
    uint64_t RelayOrder;
    uint64_t TxID;

    GotIntroReceivedEvent(
        const llarp::RouterID& ourRouter_,
        const llarp::dht::Key_t& from_,
        const llarp::service::EncryptedIntroSet& introset_,
        uint64_t txid_)
        : RouterEvent("DHT:: GotIntroReceivedEvent", ourRouter_, true)
        , From(from_)
        , Introset(introset_)
        , TxID(txid_)
    {}

    std::string
    ToString() const override
    {
      return RouterEvent::ToString() + "from " + From.ShortHex()
          + " location=" + Introset.derivedSigningKey.ShortHex()
          + " order=" + std::to_string(RelayOrder) + " txid=" + std::to_string(TxID);
    }
  };

  struct FindRouterEvent : public RouterEvent
  {
    llarp::dht::Key_t from;
    llarp::RouterID targetKey;
    bool iterative;
    bool exploritory;
    uint64_t txid;
    uint64_t version;

    FindRouterEvent(const llarp::RouterID& ourRouter, const llarp::dht::FindRouterMessage& msg)
        : RouterEvent("DHT: FindRouterEvent", ourRouter, true)
        , from(msg.From)
        , targetKey(msg.targetKey)
        , iterative(msg.iterative)
        , exploritory(msg.exploritory)
        , txid(msg.txid)
        , version(msg.version)
    {}

    std::string
    ToString() const override
    {
      return RouterEvent::ToString() + " from " + from.ShortHex()
          + ", targetKey: " + targetKey.ToString() + ", iterative: " + std::to_string(iterative)
          + ", exploritory " + std::to_string(exploritory) + ", txid " + std::to_string(txid)
          + ", version " + std::to_string(version);
    }
  };

  struct FindRouterReceivedEvent : public FindRouterEvent
  {
    using FindRouterEvent::FindRouterEvent;
  };

  struct FindRouterSentEvent : public FindRouterEvent
  {
    using FindRouterEvent::FindRouterEvent;
  };

}  // namespace tooling
