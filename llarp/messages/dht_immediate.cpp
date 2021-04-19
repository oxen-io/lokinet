#include "dht_immediate.hpp"

#include <llarp/router/abstractrouter.hpp>

namespace llarp
{
  void
  DHTImmediateMessage::Clear()
  {
    msgs.clear();
    version = 0;
  }

  bool
  DHTImmediateMessage::DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* buf)
  {
    if (key == "m")
      return llarp::dht::DecodeMesssageList(dht::Key_t(session->GetPubKey()), buf, msgs);
    if (key == "v")
    {
      if (!bencode_read_integer(buf, &version))
        return false;
      return version == LLARP_PROTO_VERSION;
    }
    // bad key
    return false;
  }

  bool
  DHTImmediateMessage::BEncode(llarp_buffer_t* buf) const
  {
    if (!bencode_start_dict(buf))
      return false;

    // message type
    if (!bencode_write_bytestring(buf, "a", 1))
      return false;
    if (!bencode_write_bytestring(buf, "m", 1))
      return false;

    // dht messages
    if (!bencode_write_bytestring(buf, "m", 1))
      return false;
    // begin list
    if (!bencode_start_list(buf))
      return false;
    for (const auto& msg : msgs)
    {
      if (!msg->BEncode(buf))
        return false;
    }
    // end list
    if (!bencode_end(buf))
      return false;

    // protocol version
    if (!bencode_write_uint64_entry(buf, "v", 1, LLARP_PROTO_VERSION))
      return false;

    return bencode_end(buf);
  }

  bool
  DHTImmediateMessage::HandleMessage(AbstractRouter* router) const
  {
    DHTImmediateMessage reply;
    reply.session = session;
    bool result = true;
    for (auto& msg : msgs)
    {
      result &= msg->HandleMessage(router->dht(), reply.msgs);
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
