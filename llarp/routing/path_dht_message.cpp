#include "path_dht_message.hpp"

#include <llarp/router/router.hpp>
#include "handler.hpp"

namespace llarp::routing
{
  bool
  PathDHTMessage::decode_key(const llarp_buffer_t& key, llarp_buffer_t* val)
  {
    if (key.startswith("M"))
    {
      llarp::dht::Key_t fromKey;
      fromKey.Zero();
      return llarp::dht::DecodeMessageList(fromKey, val, dht_msgs, true);
    }
    if (key.startswith("S"))
    {
      return bencode_read_integer(val, &sequence_number);
    }
    if (key.startswith("V"))
    {
      return bencode_read_integer(val, &version);
    }
    return false;
  }

  std::string
  PathDHTMessage::bt_encode() const
  {
    oxenc::bt_dict_producer btdp;

    try
    {
      btdp.append("A", "M");

      {
        auto subdict = btdp.append_dict("M");

        for (auto& m : dht_msgs)
          m->bt_encode(subdict);
      }

      btdp.append("S", sequence_number);
      btdp.append("V", version);
    }
    catch (...)
    {
      log::critical(route_cat, "Error: DHTMessage failed to bt encode contents!");
    }

    return std::move(btdp).str();
  }

  /// 'h' here is either TransitHop or Path.
  /// TransitHop chains to dht::Context::RelayRequestForPath and is where the
  /// end of a path handles a client's DHT message Path handles the message
  /// (e.g. dht::IMessage::HandleMessage()) in-place and is the case where a
  /// client receives a DHT message
  bool
  PathDHTMessage::handle_message(AbstractRoutingMessageHandler* h, Router* r) const
  {
    // set source as us
    const llarp::dht::Key_t us(r->pubkey());
    for (const auto& msg : dht_msgs)
    {
      msg->From = us;
      msg->pathID = from;
      if (!h->HandleDHTMessage(*msg, r))
        return false;
    }
    return true;
  }
}  // namespace llarp::routing
