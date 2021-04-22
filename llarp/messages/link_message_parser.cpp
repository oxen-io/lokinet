#include "link_message_parser.hpp"

#include "dht_immediate.hpp"
#include "discard.hpp"
#include "link_intro.hpp"
#include "link_message.hpp"
#include "relay_commit.hpp"
#include "relay_status.hpp"
#include "relay.hpp"
#include <llarp/router_contact.hpp>
#include <llarp/util/buffer.hpp>
#include <llarp/util/logging/logger.hpp>

#include <memory>

namespace llarp
{
  struct LinkMessageParser::msg_holder_t
  {
    LinkIntroMessage i;
    RelayDownstreamMessage d;
    RelayUpstreamMessage u;
    DHTImmediateMessage m;
    LR_CommitMessage c;
    LR_StatusMessage s;
    DiscardMessage x;

    msg_holder_t() = default;
  };

  LinkMessageParser::LinkMessageParser(AbstractRouter* _router)
      : router(_router), from(nullptr), msg(nullptr), holder(std::make_unique<msg_holder_t>())
  {}

  LinkMessageParser::~LinkMessageParser() = default;

  bool
  LinkMessageParser::operator()(llarp_buffer_t* buffer, llarp_buffer_t* key)
  {
    // we are reading the first key
    if (firstkey)
    {
      llarp_buffer_t strbuf;
      // check for empty dict
      if (!key)
        return false;
      // we are expecting the first key to be 'a'
      if (!(*key == "a"))
      {
        llarp::LogWarn("message has no message type");
        return false;
      }

      if (!bencode_read_string(buffer, &strbuf))
      {
        llarp::LogWarn("could not read value of message type");
        return false;
      }
      // bad key size
      if (strbuf.sz != 1)
      {
        llarp::LogWarn("bad mesage type size: ", strbuf.sz);
        return false;
      }
      // create the message to parse based off message type
      llarp::LogDebug("inbound message ", *strbuf.cur);
      switch (*strbuf.cur)
      {
        case 'i':
          msg = &holder->i;
          break;
        case 'd':
          msg = &holder->d;
          break;
        case 'u':
          msg = &holder->u;
          break;
        case 'm':
          msg = &holder->m;
          break;
        case 'c':
          msg = &holder->c;
          break;
        case 's':
          msg = &holder->s;
          break;
        case 'x':
          msg = &holder->x;
          break;
        default:
          return false;
      }

      msg->session = from;
      firstkey = false;
      return true;
    }
    // check for last element
    if (!key)
      return MessageDone();

    return msg->DecodeKey(*key, buffer);
  }

  bool
  LinkMessageParser::MessageDone()
  {
    bool result = false;
    if (msg)
    {
      result = msg->HandleMessage(router);
    }
    Reset();
    return result;
  }

  bool
  LinkMessageParser::ProcessFrom(ILinkSession* src, const llarp_buffer_t& buf)
  {
    if (!src)
    {
      llarp::LogWarn("no link session");
      return false;
    }

    from = src;
    firstkey = true;
    ManagedBuffer copy(buf);
    return bencode_read_dict(*this, &copy.underlying);
  }

  void
  LinkMessageParser::Reset()
  {
    if (msg)
      msg->Clear();
    msg = nullptr;
  }
}  // namespace llarp
