#include "path_transfer_message.hpp"

#include "handler.hpp"
#include <llarp/util/buffer.hpp>

namespace llarp::routing
{
  bool
  PathTransferMessage::decode_key(const llarp_buffer_t& key, llarp_buffer_t* val)
  {
    bool read = false;
    if (!BEncodeMaybeReadDictEntry("P", path_id, read, key, val))
      return false;
    if (!BEncodeMaybeReadDictInt("S", sequence_number, read, key, val))
      return false;
    if (!BEncodeMaybeReadDictEntry("T", protocol_frame_msg, read, key, val))
      return false;
    if (!BEncodeMaybeReadDictInt("V", version, read, key, val))
      return false;
    if (!BEncodeMaybeReadDictEntry("Y", nonce, read, key, val))
      return false;
    return read;
  }

  std::string
  PathTransferMessage::bt_encode() const
  {
    oxenc::bt_dict_producer btdp;

    try
    {
      btdp.append("A", "T");
      btdp.append("P", path_id.ToView());
      btdp.append("T", protocol_frame_msg.bt_encode());
      btdp.append("V", version);
      btdp.append("Y", nonce.ToView());
    }
    catch (...)
    {
      log::critical(route_cat, "Error: PathTransferMessage failed to bt encode contents!");
    }

    return std::move(btdp).str();
  }

  bool
  PathTransferMessage::handle_message(AbstractRoutingMessageHandler* h, Router* r) const
  {
    return h->HandlePathTransferMessage(*this, r);
  }

}  // namespace llarp::routing
