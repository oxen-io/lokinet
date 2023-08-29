#include "dht_immediate.hpp"

#include <llarp/router/abstractrouter.hpp>

namespace llarp
{
  void
  DHTImmediateMessage::clear()
  {
    msgs.clear();
    version = 0;
  }

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
  DHTImmediateMessage::handle_message(AbstractRouter* router) const
  {
    DHTImmediateMessage reply;
    reply.session = session;
    bool result = true;
    for (auto& msg : msgs)
    {
      result &= msg->handle_message(router->dht(), reply.msgs);
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
