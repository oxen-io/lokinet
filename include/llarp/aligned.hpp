#ifndef LLARP_ALIGNED_HPP
#define LLARP_ALIGNED_HPP

#include <llarp/crypto.h>
#include <sodium.h>
#include <iostream>

namespace llarp
{
  /// aligned buffer, sz must be multiple of 8 bytes
  template < size_t sz >
  struct AlignedBuffer
  {
    AlignedBuffer()
    {
    }

    AlignedBuffer(const byte_t* data)
    {
      memcpy(buf.b, data, sz);
    }

    AlignedBuffer(const AlignedBuffer& other)
    {
      memcpy(buf.b, other.data(), sz);
    }

    AlignedBuffer&
    operator=(const AlignedBuffer& other)
    {
      memcpy(buf.b, other.data(), sz);
      return *this;
    }

    byte_t& operator[](size_t idx)
    {
      return buf.b[idx];
    }

    std::ostream&
    operator<<(std::ostream& out) const
    {
      char buf[(1 + sz) * 2] = {0};
      size_t idx             = 0;
      char* ptr              = buf;
      char* end              = ptr + (sz * 2);
      while(idx < sz)
      {
        auto wrote = snprintf(ptr, end - ptr, "%.2x", buf.b[idx]);
        if(wrote == -1)
          break;
        ++idx;
        ptr += wrote;
      }
      return out << std::string(buf);
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
      return buf.b;
    }

    const byte_t*
    data() const
    {
      return buf.b;
    }

    const uint64_t*
    data_l() const
    {
      return buf.l;
    }

    uint64_t*
    data_l()
    {
      return buf.l;
    }

    operator const byte_t*() const
    {
      return buf.b;
    }

   private:
    union {
      byte_t b[sz];
      uint64_t l[sz / 8];
    } buf = {0};
  };
}

#endif
