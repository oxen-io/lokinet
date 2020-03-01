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

  extern int
  sodium_is_zero(const unsigned char* n, const size_t nlen);
}
namespace llarp
{
  /// aligned buffer that is sz bytes long and aligns to the nearest Alignment
  template < size_t sz >
#ifdef _WIN32
  // We CANNOT align on a 128-bit boundary, malloc(3C) on win32
  // only hands out 64-bit aligned pointers
  struct alignas(uint64_t) AlignedBuffer
#else
  struct alignas(std::max_align_t) AlignedBuffer
#endif
  {
    static_assert(sz >= 8,
                  "AlignedBuffer cannot be used with buffers smaller than 8 "
                  "bytes");

    static constexpr size_t SIZE = sz;

    using Data = std::array< byte_t, SIZE >;

    AlignedBuffer()
    {
      Zero();
    }

    explicit AlignedBuffer(const byte_t* data)
    {
      *this = data;
    }

    explicit AlignedBuffer(const Data& buf)
    {
      m_data = buf;
    }

    AlignedBuffer&
    operator=(const byte_t* data)
    {
      std::memcpy(m_data.data(), data, sz);
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
      return m_data == other.m_data;
    }

    bool
    operator!=(const AlignedBuffer& other) const
    {
      return m_data != other.m_data;
    }

    bool
    operator<(const AlignedBuffer& other) const
    {
      return m_data < other.m_data;
    }

    bool
    operator>(const AlignedBuffer& other) const
    {
      return m_data > other.m_data;
    }

    bool
    operator<=(const AlignedBuffer& other) const
    {
      return m_data <= other.m_data;
    }

    bool
    operator>=(const AlignedBuffer& other) const
    {
      return m_data >= other.m_data;
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
      for(size_t i = 0; i < sz; ++i)
      {
        m_data[i] ^= other.m_data[i];
      }
      return *this;
    }

    byte_t& operator[](size_t idx)
    {
      assert(idx < SIZE);
      return m_data[idx];
    }

    const byte_t& operator[](size_t idx) const
    {
      assert(idx < SIZE);
      return m_data[idx];
    }

    static constexpr size_t
    size()
    {
      return sz;
    }

    void
    Fill(byte_t f)
    {
      m_data.fill(f);
    }

    Data&
    as_array()
    {
      return m_data;
    }

    const Data&
    as_array() const
    {
      return m_data;
    }

    byte_t*
    data()
    {
      return m_data.data();
    }

    const byte_t*
    data() const
    {
      return m_data.data();
    }

    bool
    IsZero() const
    {
      return sodium_is_zero(data(), size());
    }

    void
    Zero()
    {
      m_data.fill(0);
    }

    void
    Randomize()
    {
      randombytes(data(), SIZE);
    }

    typename Data::iterator
    begin()
    {
      return m_data.begin();
    }

    typename Data::iterator
    end()
    {
      return m_data.end();
    }

    typename Data::const_iterator
    begin() const
    {
      return m_data.cbegin();
    }

    typename Data::const_iterator
    end() const
    {
      return m_data.cend();
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

    std::string
    ShortHex() const
    {
      return ToHex().substr(0, 8);
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
        size_t hash;
        std::memcpy(&hash, buf.data(), sizeof(hash));
        return hash;
      }
    };

   private:
    Data m_data;
  };
}  // namespace llarp

#endif
