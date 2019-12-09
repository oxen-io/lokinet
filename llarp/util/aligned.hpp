#ifndef LLARP_ALIGNED_HPP
#define LLARP_ALIGNED_HPP

#include <util/bencode.h>
#include <util/encode.hpp>
#include <util/logging/logger.hpp>
#include <util/meta/traits.hpp>
#include <util/printer.hpp>

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
      new(&m_val) Data;
      Zero();
    }

    explicit AlignedBuffer(const byte_t* data)
    {
      new(&m_val) Data;
      auto& b = as_array();
      for(size_t idx = 0; idx < sz; ++idx)
      {
        b[idx] = data[idx];
      }
    }

    explicit AlignedBuffer(const Data& buf)
    {
      new(&m_val) Data;
      std::copy(buf.begin(), buf.end(), begin());
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
      std::transform(begin(), end(), ret.begin(), [](byte_t a) { return ~a; });

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
      std::transform(begin(), end(), other.begin(), ret.begin(),
                     std::bit_xor< byte_t >());
      return ret;
    }

    AlignedBuffer&
    operator^=(const AlignedBuffer& other)
    {
      // Mutate in place instead.
      for(size_t i = 0; i < as_array().size(); ++i)
      {
        as_array()[i] ^= other.as_array()[i];
      }
      return *this;
    }

    byte_t& operator[](size_t idx)
    {
      assert(idx < SIZE);
      return as_array()[idx];
    }

    const byte_t& operator[](size_t idx) const
    {
      assert(idx < SIZE);
      return as_array()[idx];
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
      return reinterpret_cast< Data& >(m_val);
    }

    const Data&
    as_array() const
    {
      return reinterpret_cast< const Data& >(m_val);
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

    bool
    IsZero() const
    {
      auto notZero = [](byte_t b) { return b != 0; };

      return std::find_if(begin(), end(), notZero) == end();
    }

    void
    Zero()
    {
      as_array().fill(0);
    }

    void
    Randomize()
    {
      randombytes(data(), SIZE);
    }

    typename Data::iterator
    begin()
    {
      return as_array().begin();
    }

    typename Data::iterator
    end()
    {
      return as_array().end();
    }

    typename Data::const_iterator
    begin() const
    {
      return as_array().cbegin();
    }

    typename Data::const_iterator
    end() const
    {
      return as_array().cend();
    }

    bool
    BEncode(llarp_buffer_t* buf) const
    {
      return bencode_write_bytestring(buf, data(), sz);
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
      memcpy(data(), strbuf.base, sz);
      return true;
    }

    std::string
    ToHex() const
    {
      char strbuf[(1 + sz) * 2] = {0};
      return std::string(HexEncode(*this, strbuf));
    }

    std::ostream&
    print(std::ostream& stream, int level, int spaces) const
    {
      Printer printer(stream, level, spaces);
      printer.printValue(ToHex());

      return stream;
    }

    struct Hash
    {
      size_t
      operator()(const AlignedBuffer& buf) const
      {
        return *(reinterpret_cast< const size_t* >(buf.data()));
      }
    };

   private:
    using AlignedStorage = typename std::aligned_storage< sizeof(Data),
                                                          alignof(uint64_t) >::
        type;  // why did we align to the nearest double-precision float
    AlignedStorage m_val;
  };
}  // namespace llarp

#endif
