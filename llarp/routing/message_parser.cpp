#include <llarp/messages/dht.hpp>
#include <llarp/messages/path_confirm.hpp>
#include <llarp/messages/path_latency.hpp>
#include <llarp/messages/path_transfer.hpp>
#include <llarp/messages/discard.hpp>
#include <llarp/routing/message.hpp>
#include "mem.hpp"

namespace llarp
{
  namespace routing
  {
    InboundMessageParser::InboundMessageParser()
    {
      reader.user   = this;
      reader.on_key = &OnKey;
      firstKey      = false;
    }

    bool
    InboundMessageParser::OnKey(dict_reader* r, llarp_buffer_t* key)
    {
      InboundMessageParser* self =
          static_cast< InboundMessageParser* >(r->user);

      if(key == nullptr && self->firstKey)
      {
        // empty dict
        return false;
      }
      if(!key)
        return true;
      if(self->firstKey)
      {
        llarp_buffer_t strbuf;
        if(!llarp_buffer_eq(*key, "A"))
          return false;
        if(!bencode_read_string(r->buffer, &strbuf))
          return false;
        if(strbuf.sz != 1)
          return false;
        self->key = *strbuf.cur;
        switch(self->key)
        {
          case 'D':
            self->msg.reset(new DataDiscardMessage());
            break;
          case 'L':
            self->msg.reset(new PathLatencyMessage());
            break;
          case 'M':
            self->msg.reset(new DHTMessage());
            break;
          case 'P':
            self->msg.reset(new PathConfirmMessage());
            break;
          case 'T':
            self->msg.reset(new PathTransferMessage());
            break;
          case 'H':
            self->msg.reset(new service::ProtocolFrame());
            break;
          default:
            llarp::LogError("invalid routing message id: ", *strbuf.cur);
        }
        self->firstKey = false;
        return self->msg != nullptr;
      }
      else
      {
        return self->msg->DecodeKey(*key, r->buffer);
      }
    }

    bool
    InboundMessageParser::ParseMessageBuffer(llarp_buffer_t buf,
                                             IMessageHandler* h,
                                             const PathID_t& from,
                                             llarp_router* r)
    {
      bool result = false;
      msg         = nullptr;
      firstKey    = true;
      if(bencode_read_dict(&buf, &reader))
      {
        msg->from = from;
        result    = msg->HandleMessage(h, r);
        if(!result)
          llarp::LogWarn("Failed to handle inbound routing message ", key);
      }
      else
      {
        llarp::LogError("read dict failed in routing layer");
        llarp::DumpBuffer< llarp_buffer_t, 128 >(buf);
      }
      msg.reset();
      return result;
    }
  }  // namespace routing
}  // namespace llarp
