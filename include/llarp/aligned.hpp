#ifndef LLARP_ALIGNED_HPP
#define LLARP_ALIGNED_HPP

#include <llarp/bencode.h>
#include <llarp/crypto.h>
#include <sodium.h>
#include <iomanip>
#include <iostream>
#include <llarp/encode.hpp>
#include <llarp/logger.hpp>

namespace llarp
{
  /// aligned buffer, sz must be multiple of 8 bytes
  template < size_t sz >
  struct AlignedBuffer
  {
    AlignedBuffer() = default;

    AlignedBuffer(const byte_t* data)
    {
      for(size_t idx = 0; idx < sz; ++idx)
        b[idx] = data[idx];
    }

    AlignedBuffer&
    operator=(const byte_t* data)
    {
      for(size_t idx = 0; idx < sz; ++idx)
        b[idx] = data[idx];
      return *this;
    }

    friend std::ostream&
    operator<<(std::ostream& out, const AlignedBuffer& self)
    {
      char tmp[(1 + sz) * 2] = {0};
      return out << HexEncode(self, tmp);
    }

    bool
    operator==(const AlignedBuffer& other) const
    {
      return memcmp(data(), other.data(), sz) == 0;
    }

    bool
    operator!=(const AlignedBuffer& other) const
    {
      return !(*this == other);
    }

    size_t
    size() const
    {
      return sz;
    }

    size_t
    size()
    {
      return sz;
    }

    void
    Zero()
    {
      for(size_t idx = 0; sz < idx / 8; ++idx)
        l[idx] = 0;
    }

    void
    Randomize()
    {
      randombytes(b, sz);
    }

    byte_t*
    data()
    {
      return &b[0];
    }

    const byte_t*
    data() const
    {
      return &b[0];
    }

    uint64_t*
    data_l()
    {
      return &l[0];
    }

    const uint64_t*
    data_l() const
    {
      return &l[0];
    }

    operator const byte_t*() const
    {
      return &b[0];
    }

    operator byte_t*()
    {
      return &b[0];
    }

    bool
    BEncode(llarp_buffer_t* buf) const
    {
      return bencode_write_bytestring(buf, b, sz);
    }

    bool
    BDecode(llarp_buffer_t* buf)
    {
      llarp_buffer_t strbuf;
      if(!bencode_read_string(buf, &strbuf))
        return false;
      if(strbuf.sz != sz)
        return false;
      memcpy(b, strbuf.base, sz);
      return true;
    }

   private:
    union {
      byte_t b[sz];
      uint64_t l[sz / 8];
    };
  };
}

#endif
