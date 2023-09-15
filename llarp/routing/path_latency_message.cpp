#include "path_latency_message.hpp"

#include "handler.hpp"
#include <llarp/util/bencode.hpp>

namespace llarp::routing
{
  PathLatencyMessage::PathLatencyMessage() = default;

  bool
  PathLatencyMessage::decode_key(const llarp_buffer_t& key, llarp_buffer_t* val)
  {
    bool read = false;
    if (!BEncodeMaybeReadDictInt("L", latency, read, key, val))
      return false;
    if (!BEncodeMaybeReadDictInt("S", sequence_number, read, key, val))
      return false;
    if (!BEncodeMaybeReadDictInt("T", sent_time, read, key, val))
      return false;
    return read;
  }

  std::string
  PathLatencyMessage::bt_encode() const
  {
    oxenc::bt_dict_producer btdp;

    try
    {
      btdp.append("A", "L");
      if (latency)
        btdp.append("L", latency);
      if (sent_time)
        btdp.append("T", sent_time);
      btdp.append("S", sequence_number);
    }
    catch (...)
    {
      log::critical(route_cat, "Error: PathLatencyMessage failed to bt encode contents!");
    }

    return std::move(btdp).str();
  }

  bool
  PathLatencyMessage::handle_message(AbstractRoutingMessageHandler* h, Router* r) const
  {
    return h && h->HandlePathLatencyMessage(*this, r);
  }

}  // namespace llarp::routing
