#pragma once

#include "router_event.hpp"
#include "dht/key.hpp"
#include "service/intro_set.hpp"

namespace tooling
{

  struct PubIntroReceivedEvent : public RouterEvent
  {
    PubIntroReceivedEvent(const llarp::RouterID & ourRouter, const llarp::dht::Key_t & from, const llarp::dht::Key_t & location, uint64_t txid, uint64_t relayOrder);
    
    llarp::dht::Key_t From;
    llarp::dht::Key_t IntrosetLocation;
    uint64_t RelayOrder;
    uint64_t TxID;
    std::string ToString() const override;
  };

  struct GotIntroReceivedEvent : public RouterEvent
  {
    // TODO: thought: why not just use the original message object here?
    // TODO: question: what ties this to the actual logic that knows an event occurred?
    GotIntroReceivedEvent(
      const llarp::RouterID & ourRouter,
      const llarp::dht::Key_t & from,
      const llarp::service::EncryptedIntroSet & introset,
      uint64_t txid);
    
    llarp::dht::Key_t From;
    llarp::service::EncryptedIntroSet Introset;
    uint64_t RelayOrder;
    uint64_t TxID;
    std::string ToString() const override;
  };
}
