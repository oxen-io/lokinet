#include <messages/link_message_parser.hpp>

#include <messages/dht_immediate.hpp>
#include <messages/discard.hpp>
#include <messages/link_intro.hpp>
#include <messages/link_message.hpp>
#include <messages/relay_commit.hpp>
#include <messages/relay.hpp>
#include <router_contact.hpp>
#include <util/buffer.hpp>
#include <util/logger.hpp>

namespace llarp
{
  struct InboundMessageParser::msg_holder_t
  {
    LinkIntroMessage i;
    RelayDownstreamMessage d;
    RelayUpstreamMessage u;
    DHTImmediateMessage m;
    LR_CommitMessage c;
    DiscardMessage x;
  };

  InboundMessageParser::InboundMessageParser(AbstractRouter* _router)
      : router(_router)
      , from(nullptr)
      , msg(nullptr)
      , holder(std::make_unique< msg_holder_t >())
  {
  }

  InboundMessageParser::~InboundMessageParser()
  {
  }

  bool
  InboundMessageParser::OnKey(dict_reader* r, llarp_buffer_t* key)
  {
    InboundMessageParser* handler =
        static_cast< InboundMessageParser* >(r->user);

    // we are reading the first key
    if(handler->firstkey)
    {
      llarp_buffer_t strbuf;
      // check for empty dict
      if(!key)
        return false;
      // we are expecting the first key to be 'a'
      if(!(*key == "a"))
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
          handler->msg = &handler->holder->i;
          break;
        case 'd':
          handler->msg = &handler->holder->d;
          break;
        case 'u':
          handler->msg = &handler->holder->u;
          break;
        case 'm':
          handler->msg = &handler->holder->m;
          break;
        case 'c':
          handler->msg = &handler->holder->c;
          break;
        case 'x':
          handler->msg = &handler->holder->x;
          break;
        default:
          return false;
      }
      handler->msg->session = handler->from;
      handler->firstkey     = false;
      return true;
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
    }
    Reset();
    return result;
  }

  bool
  InboundMessageParser::ProcessFrom(ILinkSession* src,
                                    const llarp_buffer_t& buf)
  {
    if(!src)
    {
      llarp::LogWarn("no link session");
      return false;
    }
    reader.user   = this;
    reader.on_key = &OnKey;
    from          = src;
    firstkey      = true;
    ManagedBuffer copy(buf);
    return bencode_read_dict(&copy.underlying, &reader);
  }

  void
  InboundMessageParser::Reset()
  {
    if(msg)
      msg->Clear();
    msg = nullptr;
  }
}  // namespace llarp
