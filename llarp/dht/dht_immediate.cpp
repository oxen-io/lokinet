#include <llarp/messages/dht_immediate.hpp>
#include "router.hpp"

namespace llarp
{
  DHTImmeidateMessage::~DHTImmeidateMessage()
  {
  }

  bool
  DHTImmeidateMessage::DecodeKey(llarp_buffer_t key, llarp_buffer_t *buf)
  {
    if(llarp_buffer_eq(key, "m"))
      return llarp::dht::DecodeMesssageList(session->GetPubKey().data(), buf,
                                            msgs);
    if(llarp_buffer_eq(key, "v"))
    {
      if(!bencode_read_integer(buf, &version))
        return false;
      return version == LLARP_PROTO_VERSION;
    }
    // bad key
    return false;
  }

  bool
  DHTImmeidateMessage::BEncode(llarp_buffer_t *buf) const
  {
    if(!bencode_start_dict(buf))
      return false;

    // message type
    if(!bencode_write_bytestring(buf, "a", 1))
      return false;
    if(!bencode_write_bytestring(buf, "m", 1))
      return false;

    // dht messages
    if(!bencode_write_bytestring(buf, "m", 1))
      return false;
    // begin list
    if(!bencode_start_list(buf))
      return false;
    for(const auto &msg : msgs)
    {
      if(!msg->BEncode(buf))
        return false;
    }
    // end list
    if(!bencode_end(buf))
      return false;

    // protocol version
    if(!bencode_write_version_entry(buf))
      return false;

    return bencode_end(buf);
  }

  bool
  DHTImmeidateMessage::HandleMessage(llarp_router *router) const
  {
    DHTImmeidateMessage reply(session);
    bool result = true;
    for(auto &msg : msgs)
    {
      result &= msg->HandleMessage(router->dht, reply.msgs);
    }
    if(reply.msgs.size())
    {
      if(result)
      {
        result = router->SendToOrQueue(session->GetPubKey(), &reply);
      }
    }
    return result;
  }
}  // namespace llarp
