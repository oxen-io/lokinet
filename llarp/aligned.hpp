#ifndef LLARP_ALIGNED_HPP
#define LLARP_ALIGNED_HPP

#include <bencode.h>
#include <encode.hpp>
#include <logger.hpp>

#include <array>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <memory>
#include <numeric>
#include <type_traits>
#include <algorithm>

extern "C"
{
  extern void
  randombytes(unsigned char* const ptr, unsigned long long sz);
}
namespace llarp
{
  /// aligned buffer that is sz bytes long and aligns to the nearest Alignment
  template < size_t sz >
  struct AlignedBuffer
  {
    static constexpr size_t SIZE = sz;

    using Data = std::array< byte_t, SIZE >;

    AlignedBuffer()
    {
      new(&val) Data;
      Zero();
    }

    AlignedBuffer(const byte_t* data)
    {
      new(&val) Data;
      auto& b = as_array();
      for(size_t idx = 0; idx < sz; ++idx)
      {
        b[idx] = data[idx];
      }
    }

    AlignedBuffer(const Data& buf)
    {
      new(&val) Data;
      std::copy(buf.begin(), buf.end(), as_array().begin());
    }

    AlignedBuffer&
    operator=(const byte_t* data)
    {
      auto& b = as_array();
      for(size_t idx = 0; idx < sz; ++idx)
      {
        b[idx] = data[idx];
      }
      return *this;
    }

    friend std::ostream&
    operator<<(std::ostream& out, const AlignedBuffer& self)
    {
      char tmp[(sz * 2) + 1] = {0};
      return out << HexEncode(self, tmp);
    }

    /// bitwise NOT
    AlignedBuffer< sz >
    operator~() const
    {
      AlignedBuffer< sz > ret;
      std::transform(as_array().begin(), as_array().end(),
                     ret.as_array().begin(), [](byte_t a) { return ~a; });

      return ret;
    }

    bool
    operator==(const AlignedBuffer& other) const
    {
      return as_array() == other.as_array();
    }

    bool
    operator!=(const AlignedBuffer& other) const
    {
      return as_array() != other.as_array();
    }

    bool
    operator<(const AlignedBuffer& other) const
    {
      return as_array() < other.as_array();
    }

    bool
    operator>(const AlignedBuffer& other) const
    {
      return as_array() > other.as_array();
    }

    bool
    operator<=(const AlignedBuffer& other) const
    {
      return as_array() <= other.as_array();
    }

    bool
    operator>=(const AlignedBuffer& other) const
    {
      return as_array() >= other.as_array();
    }

    AlignedBuffer
    operator^(const AlignedBuffer& other) const
    {
      AlignedBuffer< sz > ret;
      std::transform(as_array().begin(), as_array().end(),
                     other.as_array().begin(), ret.as_array().begin(),
                     std::bit_xor< byte_t >());
      return ret;
    }

    AlignedBuffer&
    operator^=(const AlignedBuffer& other)
    {
      // Mutate in place instead.
      // Well defined for std::transform,

      for(size_t i = 0; i < as_array().size(); ++i)
      {
        as_array()[i] ^= other.as_array()[i];
      }
      return *this;
    }

    static constexpr size_t
    size()
    {
      return sz;
    }

    void
    Fill(byte_t f)
    {
      as_array().fill(f);
    }

    Data&
    as_array()
    {
      return reinterpret_cast< Data& >(val);
    }

    const Data&
    as_array() const
    {
      return reinterpret_cast< const Data& >(val);
    }

    bool
    IsZero() const
    {
      auto notZero = [](byte_t b) { return b != 0; };

      return std::find_if(as_array().begin(), as_array().end(), notZero)
          == as_array().end();
    }

    void
    Zero()
    {
      as_array().fill(0);
    }

    void
    Randomize()
    {
      randombytes(as_array().data(), SIZE);
    }

    byte_t*
    data()
    {
      return as_array().data();
    }

    const byte_t*
    data() const
    {
      return as_array().data();
    }

    operator const byte_t*() const
    {
      return as_array().data();
    }

    operator byte_t*()
    {
      return as_array().data();
    }

    operator const Data&() const
    {
      return as_array();
    }

    operator Data&()
    {
      return as_array();
    }

    bool
    BEncode(llarp_buffer_t* buf) const
    {
      return bencode_write_bytestring(buf, as_array().data(), sz);
    }

    bool
    BDecode(llarp_buffer_t* buf)
    {
      llarp_buffer_t strbuf;
      if(!bencode_read_string(buf, &strbuf))
      {
        return false;
      }
      if(strbuf.sz != sz)
      {
        llarp::LogError("bdecode buffer size missmatch ", strbuf.sz, "!=", sz);
        return false;
      }
      memcpy(as_array().data(), strbuf.base, sz);
      return true;
    }

    std::string
    ToHex() const
    {
      char strbuf[(1 + sz) * 2] = {0};
      return std::string(HexEncode(*this, strbuf));
    }

    struct Hash
    {
      size_t
      operator()(const AlignedBuffer& buf) const
      {
        return std::accumulate(buf.as_array().begin(), buf.as_array().end(), 0,
                               std::bit_xor< size_t >());
      }
    };

   private:
    using AlignedStorage =
        typename std::aligned_storage< sizeof(Data),
                                       alignof(uint64_t) >::type; // why did we align to the nearest double-precision float
    AlignedStorage val;
  };

}  // namespace llarp

#endif
