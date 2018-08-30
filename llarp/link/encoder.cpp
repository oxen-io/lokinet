#include "encoder.hpp"

namespace llarp
{
  /// encode Link Introduce Message onto a buffer
  /// if router is nullptr then the LIM's r member is omitted.
  bool
  EncodeLIM(llarp_buffer_t* buff, const llarp::RouterContact* router)
  {
    if(!bencode_start_dict(buff))
      return false;

    // message type
    if(!bencode_write_bytestring(buff, "a", 1))
      return false;
    if(!bencode_write_bytestring(buff, "i", 1))
      return false;

    // router contact
    if(router)
    {
      if(!bencode_write_bytestring(buff, "r", 1))
        return false;
      if(!router->BEncode(buff))
        return false;
    }

    // version
    if(!bencode_write_version_entry(buff))
      return false;

    return bencode_end(buff);
  }
}  // namespace llarp
