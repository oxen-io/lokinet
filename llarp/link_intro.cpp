#include <llarp/bencode.h>
#include <llarp/router_contact.h>
#include <llarp/messages/link_intro.hpp>
#include "logger.hpp"

namespace llarp
{
  LinkIntroMessage::~LinkIntroMessage()
  {
  }

  bool
  LinkIntroMessage::DecodeKey(llarp_buffer_t key, llarp_buffer_t* buf)
  {
    if(llarp_buffer_eq(key, "r"))
    {
      if(!llarp_rc_bdecode(RC, buf))
      {
        llarp::Warn(__FILE__, "failed to decode RC");
        return false;
      }
      remote = RC->pubkey;
      return true;
    }
    else if(llarp_buffer_eq(key, "v"))
    {
      if(!bdecode_read_integer(buf, &version))
        return false;
      if(version != LLARP_PROTO_VERSION)
      {
        llarp::Warn(__FILE__, "llarp protocol version missmatch ", version,
                    " != ", LLARP_PROTO_VERSION);
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
  LinkIntroMessage::BEncode(llarp_buffer_t* buf) const
  {
    if(!bencode_start_dict(buf))
      return false;

    if(!bencode_write_bytestring(buf, "a", 1))
      return false;
    if(!bencode_write_bytestring(buf, "i", 1))
      return false;

    if(RC)
    {
      if(!bencode_write_bytestring(buf, "r", 1))
        return false;
      if(!llarp_rc_bencode(RC, buf))
        return false;
    }

    if(!bencode_write_version_entry(buf))
      return false;

    return bencode_end(buf);
  }

  bool
  LinkIntroMessage::HandleMessage(llarp_router* router) const
  {
    llarp::Info(__FILE__, "got LIM from ", remote.Hex());
    return true;
  }
}
