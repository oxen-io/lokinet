#include "path_confirm_message.hpp"

#include "handler.hpp"
#include <llarp/util/bencode.hpp>
#include <llarp/util/time.hpp>

namespace llarp::routing
{
  PathConfirmMessage::PathConfirmMessage(llarp_time_t lifetime)
      : path_lifetime(lifetime), path_created_time(time_now_ms())
  {}

  bool
  PathConfirmMessage::decode_key(const llarp_buffer_t& key, llarp_buffer_t* val)
  {
    bool read = false;
    if (!BEncodeMaybeReadDictInt("L", path_lifetime, read, key, val))
      return false;
    if (!BEncodeMaybeReadDictInt("S", sequence_number, read, key, val))
      return false;
    if (!BEncodeMaybeReadDictInt("T", path_created_time, read, key, val))
      return false;
    if (!BEncodeMaybeReadDictInt("V", version, read, key, val))
      return false;
    return read;
  }

  std::string
  PathConfirmMessage::bt_encode() const
  {
    oxenc::bt_dict_producer btdp;

    try
    {
      btdp.append("A", "P");
      btdp.append("L", path_lifetime.count());
      btdp.append("S", sequence_number);
      btdp.append("T", path_created_time.count());
      btdp.append("V", version);
    }
    catch (...)
    {
      log::critical(route_cat, "Error: PathConfirmMessage failed to bt encode contents!");
    }

    return std::move(btdp).str();
  }

  bool
  PathConfirmMessage::handle_message(AbstractRoutingMessageHandler* h, AbstractRouter* r) const
  {
    return h && h->HandlePathConfirmMessage(*this, r);
  }

}  // namespace llarp::routing
