#include <llarp/router_contact.h>
#include <llarp/link_message.hpp>
#include "buffer.hpp"

namespace llarp
{
  InboundMessageHandler::InboundMessageHandler(llarp_router* _router)
      : router(_router)
  {
    reader.user   = this;
    reader.on_key = &OnKey;
  }

  bool
  InboundMessageHandler::OnKey(dict_reader* r, llarp_buffer_t* key)
  {
    InboundMessageHandler* handler =
        static_cast< InboundMessageHandler* >(r->user);
    llarp_buffer_t strbuf;

    // we are reading the first key
    if(handler->firstkey)
    {
      // check for empty dict
      if(!key)
        return false;
      // we are expecting the first key to be 'a'
      if(!llarp_buffer_eq(*key, "a"))
      {
        printf("message does not have message type\n");
        return false;
      }

      if(!bdecode_read_string(r->buffer, &strbuf))
      {
        printf("could not value of message type");
        return false;
      }
      // bad key size
      if(strbuf.sz != 1)
      {
        printf("bad mesage type size: %ld\n", strbuf.sz);
        return false;
      }
      handler->msgtype  = *strbuf.cur;
      handler->firstkey = false;
      return true;
    }
    // check for not the last element
    if(!key)
      return true;

    switch(handler->msgtype)
    {
        // LIM
      case 'i':
        if(llarp_buffer_eq(*key, "r"))
        {
          if(!llarp_rc_bdecode(handler->from->get_remote_router(handler->from),
                               r->buffer))
          {
            printf("failed to decode RC\n");
            return false;
          }
          printf("decoded rc\n");
          return true;
        }
        else if(llarp_buffer_eq(*key, "v"))
        {
          if(!bdecode_read_integer(r->buffer, &handler->proto))
            return false;
          if(handler->proto != LLARP_PROTO_VERSION)
          {
            printf("llarp protocol version missmatch\n");
            return false;
          }
          return true;
        }
        else
        {
          printf("invalid LIM key: %c\n", *key->cur);
          return false;
        }
      default:
        printf("unknown link message type: %c\n", handler->msgtype);
        return false;
    }
  }

  bool
  InboundMessageHandler::ProcessFrom(llarp_link_session* src,
                                     llarp_buffer_t buf)
  {
    from     = src;
    msgtype  = 0;
    firstkey = true;
    return bdecode_read_dict(&buf, &reader);
  }

  bool
  InboundMessageHandler::FlushReplies()
  {
    printf("sending replies\n");
    bool success = true;
    while(sendq.size())
    {
      auto& msg = sendq.front();
      auto buf  = llarp::Buffer< decltype(msg) >(msg);
      success &= from->sendto(from, buf);
      sendq.pop();
    }
    return success;
  }
}
