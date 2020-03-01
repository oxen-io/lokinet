#include "dht_event.hpp"


namespace tooling
{
  PubIntroReceivedEvent::PubIntroReceivedEvent(const llarp::RouterID & ourRouter, const llarp::dht::Key_t & from, const llarp::dht::Key_t & location, uint64_t txid, uint64_t relayOrder) :
  RouterEvent("DHT: PubIntroReceivedEvent", ourRouter, false),
  From(from),
  IntrosetLocation(location),
  RelayOrder(relayOrder),
  TxID(txid)
  {}

  std::string PubIntroReceivedEvent::ToString() const
  {
    return RouterEvent::ToString() + "from " + From.ShortHex() + " location=" + IntrosetLocation.ShortHex() + " order=" + std::to_string(RelayOrder) + " txid=" + std::to_string(TxID);
  }
  
}
