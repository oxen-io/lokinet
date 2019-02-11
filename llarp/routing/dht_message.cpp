#include <messages/dht.hpp>

#include <router/abstractrouter.hpp>
#include <routing/handler.hpp>

namespace llarp
{
  namespace routing
  {
    DHTMessage::~DHTMessage()
    {
    }

    bool
    DHTMessage::DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* val)
    {
      llarp::dht::Key_t from;
      from.Zero();
      if(llarp_buffer_eq(key, "M"))
      {
        return llarp::dht::DecodeMesssageList(from, val, M, true);
      }
      else if(llarp_buffer_eq(key, "S"))
      {
        return bencode_read_integer(val, &S);
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
      if(!BEncodeWriteDictInt("S", S, buf))
        return false;
      if(!BEncodeWriteDictInt("V", LLARP_PROTO_VERSION, buf))
        return false;

      return bencode_end(buf);
    }

    bool
    DHTMessage::HandleMessage(IMessageHandler* h, AbstractRouter* r) const
    {
      // set source as us
      llarp::dht::Key_t us{r->pubkey()};
      for(const auto& msg : M)
      {
        msg->From   = us;
        msg->pathID = from;
        if(!h->HandleDHTMessage(msg.get(), r))
          return false;
      }
      return true;
    }

  }  // namespace routing
}  // namespace llarp
