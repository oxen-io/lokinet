#pragma once

#include "router_event.hpp"
#include "dht/key.hpp"
#include "service/intro_set.hpp"

namespace tooling
{
  struct PubIntroSentEvent : public RouterEvent
  {
    PubIntroSentEvent(const llarp::RouterID& ourRouter,
                      const llarp::dht::Key_t& introsetPubkey,
                      const llarp::RouterID& relay, uint64_t relayIndex);

    llarp::dht::Key_t introsetPubkey;

    llarp::RouterID relay;

    uint64_t relayIndex;

    std::string
    ToString() const override;
  };

  struct PubIntroReceivedEvent : public RouterEvent
  {
    PubIntroReceivedEvent(const llarp::RouterID& ourRouter,
                          const llarp::dht::Key_t& from,
                          const llarp::dht::Key_t& location, uint64_t txid,
                          uint64_t relayOrder);

    llarp::dht::Key_t From;
    llarp::dht::Key_t IntrosetLocation;
    uint64_t RelayOrder;
    uint64_t TxID;
    std::string
    ToString() const override;
  };

  struct GotIntroReceivedEvent : public RouterEvent
  {
    // TODO: thought: why not just use the original message object here?
    // TODO: question: what ties this to the actual logic that knows an event
    // occurred?
    GotIntroReceivedEvent(const llarp::RouterID& ourRouter,
                          const llarp::dht::Key_t& from,
                          const llarp::service::EncryptedIntroSet& introset,
                          uint64_t txid);

    llarp::dht::Key_t From;
    llarp::service::EncryptedIntroSet Introset;
    uint64_t RelayOrder;
    uint64_t TxID;
    std::string
    ToString() const override;
  };

  struct FindRouterSentEvent : public RouterEvent
  {
    llarp::dht::Key_t from;
    llarp::RouterID targetKey;
    bool iterative = false;
    bool exploritory = false;
    uint64_t txid = 0;
    uint64_t version = 0;

    FindRouterSentEvent(const llarp::RouterID& ourRouter)
      : RouterEvent("DHT: FindRouterSentEvent", ourRouter, true)
    {
    }

    std::string
    ToString() const override
    {
      return RouterEvent::ToString()
        +" from "+ from.ShortHex()
        +", targetKey: "+ targetKey.ToString()
        +", iterative: "+ std::to_string(iterative)
        +", exploritory "+ std::to_string(exploritory)
        +", txid "+ std::to_string(txid)
        +", version "+ std::to_string(version);
    }
  };

  struct FindRouterReceivedEvent : public RouterEvent
  {
    llarp::dht::Key_t from;
    llarp::RouterID targetKey;
    bool iterative = false;
    bool exploritory = false;
    uint64_t txid = 0;
    uint64_t version = 0;

    FindRouterReceivedEvent(const llarp::RouterID& ourRouter)
      : RouterEvent("DHT: FindRouterReceivedEvent", ourRouter, false)
    {
    }

    std::string
    ToString() const override
    {
      return RouterEvent::ToString()
        +" from "+ from.ShortHex()
        +", targetKey: "+ targetKey.ToString()
        +", iterative: "+ std::to_string(iterative)
        +", exploritory "+ std::to_string(exploritory)
        +", txid "+ std::to_string(txid)
        +", version "+ std::to_string(version);
    }
  };

}  // namespace tooling
