#include <llarp/router_contact.h>
#include <llarp/link_message.hpp>
#include <llarp/messages/dht_immediate.hpp>
#include <llarp/messages/link_intro.hpp>
#include <llarp/messages/relay_commit.hpp>
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
        llarp::Warn(__FILE__, "message has no message type");
        return false;
      }

      if(!bdecode_read_string(r->buffer, &strbuf))
      {
        llarp::Warn(__FILE__, "could not read value of message type");
        return false;
      }
      // bad key size
      if(strbuf.sz != 1)
      {
        llarp::Warn(__FILE__, "bad mesage type size: ", strbuf.sz);
        return false;
      }
      switch(*strbuf.cur)
      {
        case 'i':
          handler->msg = new LinkIntroMessage(
              handler->from->get_remote_router(handler->from));
          break;
        case 'm':
          handler->msg = new DHTImmeidateMessage(handler->GetCurrentFrom());
          break;
        case 'c':
          handler->msg = new LR_CommitMessage(handler->GetCurrentFrom());
      }
      handler->firstkey = false;
      return handler->msg != nullptr;
    }
    // check for last element
    if(!key)
      return handler->MessageDone();

    return handler->msg->DecodeKey(*key, r->buffer);
  }

  /*
  bool
  InboundMessageHandler::DecodeLIM(llarp_buffer_t key, llarp_buffer_t* buff)
  {
    if(llarp_buffer_eq(key, "r"))
    {
      if(!llarp_rc_bdecode(from->get_remote_router(from), buff))
      {
        llarp::Warn(__FILE__, "failed to decode RC");
        return false;
      }
      return true;
    }
    else if(llarp_buffer_eq(key, "v"))
    {
      if(!bdecode_read_integer(buff, &proto))
        return false;
      if(proto != LLARP_PROTO_VERSION)
      {
        llarp::Warn(__FILE__, "llarp protocol version missmatch ", proto);
        return false;
      }
      return true;
    }
    else
    {
      llarp::Warn(__FILE__, "invalid LIM key: ", *key.cur);
      return false;
    }
  }

  bool
  InboundMessageHandler::DecodeDHT(llarp_buffer_t key, llarp_buffer_t* buf)
  {
    if(llarp_buffer_eq(key, "d"))
      return llarp::dht::DecodeMesssageList(buf, dhtmsgs);
    if(llarp_buffer_eq(key, "v"))
    {
      if(!bdecode_read_integer(buf, &proto))
        return false;
      return proto == LLARP_PROTO_VERSION;
    }
    // bad key
    return false;
  }

  bool
  InboundMessageHandler::DecodeLRCM(llarp_buffer_t key, llarp_buffer_t* buf)
  {
    return false;
  }

  */

  RouterID
  InboundMessageParser::GetCurrentFrom()
  {
    auto rc = from->get_remote_router(from);
    return rc->pubkey;
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
  InboundMessageParser::ProcessFrom(llarp_link_session* src, llarp_buffer_t buf)
  {
    from     = src;
    firstkey = true;
    return bdecode_read_dict(&buf, &reader);
  }
}
