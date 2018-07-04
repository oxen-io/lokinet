#include <llarp/bencode.h>
#include <llarp/router_contact.h>
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
    if(llarp_buffer_eq(key, "r"))
    {
      if(!llarp_rc_bdecode(RC, buf))
      {
        llarp::Warn("failed to decode RC");
        return false;
      }
      remote = (byte_t*)RC->pubkey;
      llarp::Debug("decoded RC from ", remote);
      return true;
    }
    else if(llarp_buffer_eq(key, "v"))
    {
      if(!bencode_read_integer(buf, &version))
        return false;
      if(version != LLARP_PROTO_VERSION)
      {
        llarp::Warn("llarp protocol version missmatch ", version,
                    " != ", LLARP_PROTO_VERSION);
        return false;
      }
      llarp::Debug("LIM version ", version);
      return true;
    }
    else
    {
      llarp::Warn("invalid LIM key: ", *key.cur);
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
    router->async_verify_RC(RC, !llarp_rc_is_public_router(RC));
    return true;
  }
}  // namespace llarp
