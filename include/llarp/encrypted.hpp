#ifndef LLARP_ENCCRYPTED_HPP
#define LLARP_ENCCRYPTED_HPP

#include <llarp/bencode.h>
#include <llarp/buffer.h>
#include <sodium.h>

namespace llarp
{
  /// encrypted buffer base type
  struct Encrypted
  {
    Encrypted() = default;
    Encrypted(const byte_t* buf, size_t sz);
    Encrypted(size_t sz);
    ~Encrypted();

    bool
    BEncode(llarp_buffer_t* buf) const
    {
      return bencode_write_bytestring(buf, data, size);
    }

    void
    Randomize()
    {
      if(data)
        randombytes(data, size);
    }

    bool
    BDecode(llarp_buffer_t* buf)
    {
      llarp_buffer_t strbuf;
      if(!bencode_read_string(buf, &strbuf))
        return false;
      if(strbuf.sz == 0)
        return false;
      if(data)
        delete[] data;
      size = strbuf.sz;
      data = new byte_t[size];
      memcpy(data, strbuf.base, size);
      return true;
    }

    llarp_buffer_t*
    Buffer()
    {
      return &m_Buffer;
    }

    byte_t* data = nullptr;
    size_t size  = 0;

   private:
    llarp_buffer_t m_Buffer;
  };
}

#endif