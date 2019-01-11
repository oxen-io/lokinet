#include <dns/serialize.hpp>
#include <net/net_int.hpp>

namespace llarp
{
  namespace dns
  {
    bool
    EncodeRData(llarp_buffer_t* buf, const std::vector< byte_t >& v)
    {
      if(v.size() > 65536)
        return false;
      uint16_t len = v.size();
      if(!llarp_buffer_put_uint16(buf, len))
        return false;
      if(llarp_buffer_size_left(*buf) < len)
        return false;
      memcpy(buf->cur, v.data(), len);
      buf->cur += len;
      return true;
    }

    bool
    DecodeRData(llarp_buffer_t* buf, std::vector< byte_t >& v)
    {
      uint16_t len;
      if(!llarp_buffer_read_uint16(buf, &len))
        return false;
      size_t left = llarp_buffer_size_left(*buf);
      if(left < len)
        return false;
      v.resize(size_t(len));
      if(len)
      {
        memcpy(v.data(), buf->cur, len);
        buf->cur += len;
      }
      return true;
    }

  }  // namespace dns
}  // namespace llarp
