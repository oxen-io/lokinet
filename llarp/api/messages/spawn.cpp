#include <llarp/api/messages.hpp>

namespace llarp
{
  namespace api
  {
    SpawnMessage::~SpawnMessage()
    {
    }

    bool
    SpawnMessage::EncodeParams(llarp_buffer_t *buf) const
    {
      return true;
    }

    bool
    SpawnMessage::DecodeKey(llarp_buffer_t key, llarp_buffer_t *val)
    {
      if(llarp_buffer_eq(key, "N"))
      {
        llarp_buffer_t strbuf;
        if(!bencode_read_string(val, &strbuf))
          return false;
        SessionName = std::string((char *)strbuf.cur, strbuf.sz);
        return true;
      }
      if(llarp_buffer_eq(key, "S"))
      {
        return Info.BDecode(val);
      }
      return IMessage::DecodeKey(key, val);
    }

  }  // namespace api
}  // namespace llarp