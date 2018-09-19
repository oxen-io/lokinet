#include <llarp/bencode.h>
#include <llarp/router_contact.hpp>
#include <llarp/messages/link_intro.hpp>
#include "logger.hpp"
#include "router.hpp"

namespace llarp
{
  LinkIntroMessage::~LinkIntroMessage()
  {
  }

  bool
  LinkIntroMessage::DecodeKey(llarp_buffer_t key, llarp_buffer_t* buf)
  {
    if(llarp_buffer_eq(key, "a"))
    {
      llarp_buffer_t strbuf;
      if(!bencode_read_string(buf, &strbuf))
        return false;
      if(strbuf.sz != 1)
        return false;
      return *strbuf.cur == 'i';
    }
    if(llarp_buffer_eq(key, "n"))
    {
      if(N.BDecode(buf))
        return true;
      llarp::LogWarn("failed to decode nonce in LIM");
      return false;
    }
    if(llarp_buffer_eq(key, "r"))
    {
      if(rc.BDecode(buf))
        return true;
      llarp::LogWarn("failed to decode RC in LIM");
      llarp::DumpBuffer(*buf);
      return false;
    }
    else if(llarp_buffer_eq(key, "v"))
    {
      if(!bencode_read_integer(buf, &version))
        return false;
      if(version != LLARP_PROTO_VERSION)
      {
        llarp::LogWarn("llarp protocol version missmatch ", version,
                       " != ", LLARP_PROTO_VERSION);
        return false;
      }
      llarp::LogDebug("LIM version ", version);
      return true;
    }
    else
    {
      llarp::LogWarn("invalid LIM key: ", *key.cur);
      return false;
    }
  }

  bool
  LinkIntroMessage::BEncode(llarp_buffer_t* buf) const
  {
    if(!bencode_start_dict(buf))
      return false;

    if(!bencode_write_bytestring(buf, "a", 1))
      return false;
    if(!bencode_write_bytestring(buf, "i", 1))
      return false;

    if(!bencode_write_bytestring(buf, "n", 1))
      return false;
    if(!N.BEncode(buf))
      return false;

    if(!bencode_write_bytestring(buf, "r", 1))
      return false;
    if(!rc.BEncode(buf))
      return false;

    if(!bencode_write_version_entry(buf))
      return false;

    return bencode_end(buf);
  }

  bool
  LinkIntroMessage::HandleMessage(llarp_router* router) const
  {
    return rc.VerifySignature(&router->crypto);
  }
}  // namespace llarp
