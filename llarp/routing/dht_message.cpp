#include "dht_message.hpp"

#include <llarp/router/abstractrouter.hpp>
#include "handler.hpp"

namespace llarp
{
  namespace routing
  {
    bool
    DHTMessage::DecodeKey(const llarp_buffer_t& key, llarp_buffer_t* val)
    {
      if (key == "M")
      {
        llarp::dht::Key_t fromKey;
        fromKey.Zero();
        return llarp::dht::DecodeMesssageList(fromKey, val, M, true);
      }
      if (key == "S")
      {
        return bencode_read_integer(val, &S);
      }
      if (key == "V")
      {
        return bencode_read_integer(val, &V);
      }
      return false;
    }

    bool
    DHTMessage::BEncode(llarp_buffer_t* buf) const
    {
      if (!bencode_start_dict(buf))
        return false;

      if (!BEncodeWriteDictMsgType(buf, "A", "M"))
        return false;
      if (!BEncodeWriteDictBEncodeList("M", M, buf))
        return false;
      if (!BEncodeWriteDictInt("S", S, buf))
        return false;
      if (!BEncodeWriteDictInt("V", LLARP_PROTO_VERSION, buf))
        return false;

      return bencode_end(buf);
    }

    /// 'h' here is either TransitHop or Path.
    /// TransitHop chains to dht::Context::RelayRequestForPath and is where the
    /// end of a path handles a client's DHT message Path handles the message
    /// (e.g. dht::IMessage::HandleMessage()) in-place and is the case where a
    /// client receives a DHT message
    bool
    DHTMessage::HandleMessage(IMessageHandler* h, AbstractRouter* r) const
    {
      // set source as us
      const llarp::dht::Key_t us(r->pubkey());
      for (const auto& msg : M)
      {
        msg->From = us;
        msg->pathID = from;
        if (!h->HandleDHTMessage(*msg, r))
          return false;
      }
      return true;
    }
  }  // namespace routing
}  // namespace llarp
