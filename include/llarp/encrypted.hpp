#ifndef LLARP_ENCCRYPTED_HPP
#define LLARP_ENCCRYPTED_HPP

#include <llarp/bencode.h>
#include <llarp/buffer.h>
#include <sodium.h>
#include <vector>

namespace llarp
{
  /// encrypted buffer base type
  struct Encrypted
  {
    Encrypted() = default;
    Encrypted(const byte_t* buf, size_t sz);
    Encrypted(size_t sz);

    bool
    BEncode(llarp_buffer_t* buf) const
    {
      return bencode_write_bytestring(buf, _data.data(), _data.size());
    }

    void
    Randomize()
    {
      if(_data.size())
        randombytes(_data.data(), _data.size());
    }

    bool
    BDecode(llarp_buffer_t* buf)
    {
      llarp_buffer_t strbuf;
      if(!bencode_read_string(buf, &strbuf))
        return false;
      if(strbuf.sz == 0)
        return false;
      _data.resize(strbuf.sz);
      memcpy(_data.data(), strbuf.base, _data.size());
      return true;
    }

    llarp_buffer_t*
    Buffer()
    {
      return &m_Buffer;
    }

    size_t
    size()
    {
      return _data.size();
    }

    size_t
    size() const
    {
      return _data.size();
    }

    byte_t*
    data()
    {
      return _data.data();
    }

    std::vector< byte_t > _data;

   private:
    llarp_buffer_t m_Buffer;
  };
}  // namespace llarp

#endif