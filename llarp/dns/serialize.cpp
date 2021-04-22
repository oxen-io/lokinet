#include "serialize.hpp"
#include <llarp/net/net_int.hpp>

namespace llarp
{
  namespace dns
  {
    Serialize::~Serialize() = default;

    bool
    EncodeRData(llarp_buffer_t* buf, const std::vector<byte_t>& v)
    {
      if (v.size() > 65536)
        return false;
      uint16_t len = v.size();
      if (!buf->put_uint16(len))
        return false;
      if (buf->size_left() < len)
        return false;
      memcpy(buf->cur, v.data(), len);
      buf->cur += len;
      return true;
    }

    bool
    DecodeRData(llarp_buffer_t* buf, std::vector<byte_t>& v)
    {
      uint16_t len;
      if (!buf->read_uint16(len))
        return false;
      size_t left = buf->size_left();
      if (left < len)
        return false;
      v.resize(size_t(len));
      if (len)
      {
        memcpy(v.data(), buf->cur, len);
        buf->cur += len;
      }
      return true;
    }

  }  // namespace dns
}  // namespace llarp
