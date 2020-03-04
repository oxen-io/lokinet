#include "dht_event.hpp"
#include "service/intro_set.hpp"


namespace tooling
{
  PubIntroReceivedEvent::PubIntroReceivedEvent(const llarp::RouterID & ourRouter, const llarp::dht::Key_t & from, const llarp::dht::Key_t & location, uint64_t txid, uint64_t relayOrder) :
  RouterEvent("DHT: PubIntroReceivedEvent", ourRouter, true),
  From(from),
  IntrosetLocation(location),
  RelayOrder(relayOrder),
  TxID(txid)
  {}

  PubIntroSentEvent::PubIntroSentEvent(const llarp::RouterID & ourRouter, const llarp::dht::Key_t & introsetPubkey, const llarp::RouterID& relay, uint64_t relayIndex)
  : RouterEvent("DHT: PubIntroSentEvent", ourRouter, false)
  , introsetPubkey(introsetPubkey)
  , relay(relay)
  , relayIndex(relayIndex)
  {
  }

  std::string
  PubIntroSentEvent::ToString() const
  {
    return RouterEvent::ToString() + " ---- introset pubkey: " + introsetPubkey.ShortHex() + ", relay: " + relay.ShortString() + ", relayIndex: " + std::to_string(relayIndex);
  }

  std::string PubIntroReceivedEvent::ToString() const
  {
    return RouterEvent::ToString() + "from " + From.ShortHex() + " location=" + IntrosetLocation.ShortHex() + " order=" + std::to_string(RelayOrder) + " txid=" + std::to_string(TxID);
  }
  
  GotIntroReceivedEvent::GotIntroReceivedEvent(
      const llarp::RouterID& ourRouter_,
      const llarp::dht::Key_t& from_,
      const llarp::service::EncryptedIntroSet & introset_,
      uint64_t txid_)
      : RouterEvent("DHT:: GotIntroReceivedEvent", ourRouter_, true)
      , From(from_)
      , Introset(introset_)
      , TxID(txid_)
  {
  }

  std::string GotIntroReceivedEvent::ToString() const
  {
    return RouterEvent::ToString() + "from " + From.ShortHex() + " location=" + Introset.derivedSigningKey.ShortHex() + " order=" + std::to_string(RelayOrder) + " txid=" + std::to_string(TxID);
  }

}
