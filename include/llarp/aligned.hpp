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
  /// aligned buffer, aligns to the nears Long_t
  template < size_t sz, bool randomize = false, typename Long_t = uint64_t >
  struct AlignedBuffer
  {
    AlignedBuffer()
    {
      if(randomize)
        Randomize();
      else
        Zero();
    }

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

    bool
    operator<(const AlignedBuffer& other) const
    {
      return memcmp(l, other.l, sz) < 0;
    }

    bool
    operator>(const AlignedBuffer& other) const
    {
      return memcmp(l, other.l, sz) > 0;
    }

    bool
    operator<=(const AlignedBuffer& other) const
    {
      return memcmp(l, other.l, sz) <= 0;
    }

    bool
    operator>=(const AlignedBuffer& other) const
    {
      return memcmp(l, other.l, sz) >= 0;
    }

    AlignedBuffer
    operator^(const AlignedBuffer& other) const
    {
      AlignedBuffer< sz > ret;
      for(size_t idx = 0; idx < sz / sizeof(Long_t); ++idx)
        ret.l[idx] = l[idx] ^ other.l[idx];
      return ret;
    }

    AlignedBuffer&
    operator^=(const AlignedBuffer& other)
    {
      for(size_t idx = 0; idx < sz / sizeof(Long_t); ++idx)
        l[idx] ^= other.l[idx];
      return *this;
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
    Fill(byte_t f)
    {
      for(size_t idx = 0; idx < sz; ++idx)
        b[idx] = f;
    }

    bool
    IsZero() const
    {
      return sodium_is_zero(b, sz) != 0;
    }

    void
    Zero()
    {
      sodium_memzero(l, sz);
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

    Long_t*
    data_l()
    {
      return &l[0];
    }

    const Long_t*
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
      {
        llarp::LogErrorTag("AlignedBuffer::BDecode",
                           "bdecode buffer size missmatch ", strbuf.sz,
                           "!=", sz);
        return false;
      }
      memcpy(b, strbuf.base, sz);
      return true;
    }

    struct Hash
    {
      size_t
      operator()(const AlignedBuffer& buf) const
      {
        return *buf.data_l();
      }
    };

   protected:
    union {
      byte_t b[sz];
      Long_t l[(sz / sizeof(Long_t)) + (sz % sizeof(Long_t))];
    };
  };

}  // namespace llarp

#endif
