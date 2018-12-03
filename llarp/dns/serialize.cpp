#include <llarp/dns/serialize.hpp>
#include <llarp/net_int.hpp>

namespace llarp
{
  namespace dns
  {
    template <>
    bool
    DecodeInt(llarp_buffer_t* buf, llarp::huint32_t& i)
    {
      return llarp_buffer_read_uint32(buf, &i.h);
    }

    template <>
    bool
    EncodeInt(llarp_buffer_t* buf, const llarp::huint32_t& i)
    {
      return llarp_buffer_put_uint32(buf, i.h);
    }

    template <>
    bool
    DecodeInt(llarp_buffer_t* buf, llarp::huint16_t& i)
    {
      return llarp_buffer_read_uint16(buf, &i.h);
    }

    template <>
    bool
    EncodeInt(llarp_buffer_t* buf, const llarp::huint16_t& i)
    {
      return llarp_buffer_put_uint16(buf, i.h);
    }

    template <>
    bool
    DecodeInt(llarp_buffer_t* buf, uint16_t& i)
    {
      return llarp_buffer_read_uint16(buf, &i);
    }

    template <>
    bool
    EncodeInt(llarp_buffer_t* buf, const uint16_t& i)
    {
      return llarp_buffer_put_uint16(buf, i);
    }

    bool
    EncodeRData(llarp_buffer_t* buf, const std::vector< byte_t >& v)
    {
      if(v.size() > 65536)
        return false;
      llarp::huint16_t len;
      len.h = v.size();
      return EncodeInt(buf, len) && llarp_buffer_write(buf, v.data(), v.size());
    }

    bool
    DecodeRData(llarp_buffer_t* buf, std::vector< byte_t >& v)
    {
      llarp::huint16_t len = {0};
      if(!DecodeInt(buf, len))
        return false;
      size_t left = llarp_buffer_size_left(*buf);
      if(left < len.h)
        return false;
      v.resize(size_t(len.h));
      memcpy(v.data(), buf->cur, len.h);
      buf->cur += len.h;
      return true;
    }

  }  // namespace dns
}  // namespace llarp
