#include "dht_immediate.hpp"

#include <llarp/router/router.hpp>
#include <llarp/dht/context.hpp>

namespace llarp
{
  void
  DHTImmediateMessage::clear()
  {
    msgs.clear();
    version = 0;
  }

  /** Note: this is where AbstractDHTMessage::bt_encode() is called. Contextually, this is a
      bit confusing as it is within the ::bt_encode() method of DHTImmediateMessage, which is
      not an AbstractDHTMessage, but an AbstractLinkMessage. To see why AbstractLinkMessage
      overrides the ::bt_encode() that returns an std::string, see the comment in llarp/router/
      outbound_message_handler.cpp above OutboundMessageHandler::EncodeBuffer(...).

      In this context, there is already a bt_dict_producer being used by DHTImmediateMessage's
      bt_encode() method. This allows us to easily choose the override of bt_encode() that returns
      nothing, but takes a bt_dict_producer as a reference.
  */
  std::string
  DHTImmediateMessage::bt_encode() const
  {
    oxenc::bt_dict_producer btdp;

    try
    {
      btdp.append("a", "");
      {
        auto subdict = btdp.append_dict("m");
        for (auto& m : msgs)
          m->bt_encode(subdict);
      }

      btdp.append("v", llarp::constants::proto_version);
    }
    catch (...)
    {
      log::critical(link_cat, "Error: DHTImmediateMessage failed to bt encode contents!");
    }

    return std::move(btdp).str();
  }

  bool
  DHTImmediateMessage::decode_key(const llarp_buffer_t& key, llarp_buffer_t* buf)
  {
    if (key.startswith("m"))
      return llarp::dht::DecodeMessageList(dht::Key_t(session->GetPubKey()), buf, msgs);
    if (key.startswith("v"))
    {
      if (!bencode_read_integer(buf, &version))
        return false;
      return version == llarp::constants::proto_version;
    }
    // bad key
    return false;
  }

  bool
  DHTImmediateMessage::handle_message(Router* router) const
  {
    DHTImmediateMessage reply;
    reply.session = session;
    bool result = true;
    auto dht = router->dht();
    for (const auto& msg : msgs)
    {
      result &= dht->handle_message(*msg, reply.msgs);
    }
    if (reply.msgs.size())
    {
      if (result)
      {
        result = router->SendToOrQueue(session->GetPubKey(), reply);
      }
    }
    return true;
  }
}  // namespace llarp
