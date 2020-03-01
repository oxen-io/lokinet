#pragma once

#include "router_event.hpp"
#include "dht/key.hpp"

namespace tooling
{
  struct FindIntroReceievedEvent : public RouterEvent
  {
    FindIntroReceievedEvent(const llarp::RouterID & ourRouter, const llarp::dht::Key_t & from, const llarp::dht::Key_t & location, uint64_t txid);
    
    llarp::dht::Key_t From;
    llarp::dht::Key_t IntrosetLocation;
    uint64_t TxID;
    
    std::string ToString() const override;
  };

  struct PubIntroReceivedEvent : public RouterEvent
  {
    PubIntroReceivedEvent(const llarp::RouterID & ourRouter, const llarp::dht::Key_t & from, const llarp::dht::Key_t & location, uint64_t txid, uint64_t relayOrder);
    
    llarp::dht::Key_t From;
    llarp::dht::Key_t IntrosetLocation;
    uint64_t RelayOrder;
    uint64_t TxID;
    std::string ToString() const override;
  };
}
