#ifndef LLARP_ALIGNED_HPP
#define LLARP_ALIGNED_HPP

#include <llarp/crypto.h>
#include <sodium.h>
#include <iomanip>
#include <iostream>
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
        buf.b[idx] = data[idx];
    }

    AlignedBuffer&
    operator=(const byte_t* data)
    {
      for(size_t idx = 0; idx < sz; ++idx)
        buf.b[idx] = data[idx];
      return *this;
    }

    byte_t& operator[](size_t idx)
    {
      return buf.b[idx];
    }

    friend std::ostream&
    operator<<(std::ostream& out, const AlignedBuffer& self)
    {
      size_t idx = 0;
      out << std::hex << std::setw(2) << std::setfill('0');
      while(idx < sz)
      {
        out << (int)self.buf.b[idx++];
      }
      return out << std::dec << std::setw(0) << std::setfill(' ');
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

    void
    Zero()
    {
      for(size_t idx = 0; sz < idx / 8; ++idx)
        buf.l[idx] = 0;
    }

    void
    Randomize()
    {
      randombytes(buf.b, sz);
    }

    byte_t*
    data()
    {
      return &buf.b[0];
    }

    const byte_t*
    data() const
    {
      return &buf.b[0];
    }

    uint64_t*
    data_l()
    {
      return &buf.l[0];
    }

    const uint64_t*
    data_l() const
    {
      return &buf.l[0];
    }

    operator const byte_t*() const
    {
      return &buf.b[0];
    }

    operator byte_t*()
    {
      return &buf.b[0];
    }

   private:
    union {
      byte_t b[sz];
      uint64_t l[sz / 8];
    } buf;
  };
}

#endif
