#include <llarp/messages/dht.hpp>
#include "../router.hpp"

namespace llarp
{
  namespace routing
  {
    DHTMessage::~DHTMessage()
    {
      for(auto& msg : M)
        delete msg;
    }

    bool
    DHTMessage::DecodeKey(llarp_buffer_t key, llarp_buffer_t* val)
    {
      llarp::dht::Key_t from;
      from.Zero();
      if(llarp_buffer_eq(key, "M"))
      {
        return llarp::dht::DecodeMesssageList(from, val, M, true);
      }
      else if(llarp_buffer_eq(key, "V"))
      {
        return bencode_read_integer(val, &V);
      }
      return false;
    }

    bool
    DHTMessage::BEncode(llarp_buffer_t* buf) const
    {
      if(!bencode_start_dict(buf))
        return false;

      if(!BEncodeWriteDictMsgType(buf, "A", "M"))
        return false;
      if(!BEncodeWriteDictBEncodeList("M", M, buf))
        return false;
      if(!BEncodeWriteDictInt(buf, "V", LLARP_PROTO_VERSION))
        return false;

      return bencode_end(buf);
    }

    bool
    DHTMessage::HandleMessage(IMessageHandler* h, llarp_router* r) const
    {
      // set source as us
      llarp::dht::Key_t us = r->pubkey();
      for(auto& msg : M)
      {
        msg->From   = us;
        msg->pathID = from;
        if(!h->HandleDHTMessage(msg, r))
          return false;
      }
      return true;
    }

  }  // namespace routing
}  // namespace llarp