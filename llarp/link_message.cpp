#include <llarp/router_contact.hpp>
#include <llarp/messages.hpp>
#include "buffer.hpp"
#include "logger.hpp"
#include "router.hpp"

namespace llarp
{
  InboundMessageParser::InboundMessageParser(llarp_router* _router)
      : router(_router)
  {
    reader.user   = this;
    reader.on_key = &OnKey;
  }

  bool
  InboundMessageParser::OnKey(dict_reader* r, llarp_buffer_t* key)
  {
    InboundMessageParser* handler =
        static_cast< InboundMessageParser* >(r->user);
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
        llarp::LogWarn("message has no message type");
        return false;
      }

      if(!bencode_read_string(r->buffer, &strbuf))
      {
        llarp::LogWarn("could not read value of message type");
        return false;
      }
      // bad key size
      if(strbuf.sz != 1)
      {
        llarp::LogWarn("bad mesage type size: ", strbuf.sz);
        return false;
      }
      // create the message to parse based off message type
      llarp::LogDebug("inbound message ", *strbuf.cur);
      switch(*strbuf.cur)
      {
        case 'i':
          handler->msg = new LinkIntroMessage(handler->from);
          break;
        case 'd':
          handler->msg = new RelayDownstreamMessage(handler->from);
          break;
        case 'u':
          handler->msg = new RelayUpstreamMessage(handler->from);
          break;
        case 'm':
          handler->msg = new DHTImmeidateMessage(handler->from);
          break;
        case 'c':
          handler->msg = new LR_CommitMessage(handler->from);
          break;
        default:
          return false;
      }
      handler->firstkey = false;
      return handler->msg != nullptr;
    }
    // check for last element
    if(!key)
      return handler->MessageDone();

    return handler->msg->DecodeKey(*key, r->buffer);
  }

  bool
  InboundMessageParser::MessageDone()
  {
    bool result = false;
    if(msg)
    {
      result = msg->HandleMessage(router);
      delete msg;
      msg = nullptr;
    }
    return result;
  }

  bool
  InboundMessageParser::ProcessFrom(ILinkSession* src, llarp_buffer_t buf)
  {
    from     = src;
    firstkey = true;
    return bencode_read_dict(&buf, &reader);
  }
}  // namespace llarp
