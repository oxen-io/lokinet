#pragma once

#include "bencode.h"
#include <llarp/util/logging/logger.hpp>
#include <llarp/util/meta/traits.hpp>
#include "printer.hpp"

#include <oxenmq/hex.h>

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
  template <size_t sz>
  // Microsoft C malloc(3C) cannot return pointers aligned wider than 8 ffs
#ifdef _WIN32
  struct alignas(uint64_t) AlignedBuffer
#else
  struct alignas(std::max_align_t) AlignedBuffer
#endif
  {
    static_assert(alignof(std::max_align_t) <= 16, "insane alignment");
    static_assert(
        sz >= 8,
        "AlignedBuffer cannot be used with buffers smaller than 8 "
        "bytes");

    static constexpr size_t SIZE = sz;

    using Data = std::array<byte_t, SIZE>;

    virtual ~AlignedBuffer() = default;

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
      return out << oxenmq::to_hex(self.begin(), self.end());
    }

    /// bitwise NOT
    AlignedBuffer<sz>
    operator~() const
    {
      AlignedBuffer<sz> ret;
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
      AlignedBuffer<sz> ret;
      std::transform(begin(), end(), other.begin(), ret.begin(), std::bit_xor<>());
      return ret;
    }

    AlignedBuffer&
    operator^=(const AlignedBuffer& other)
    {
      // Mutate in place instead.
      for (size_t i = 0; i < sz; ++i)
      {
        m_data[i] ^= other.m_data[i];
      }
      return *this;
    }

    byte_t&
    operator[](size_t idx)
    {
      assert(idx < SIZE);
      return m_data[idx];
    }

    const byte_t&
    operator[](size_t idx) const
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
      const uint64_t* ptr = reinterpret_cast<const uint64_t*>(data());
      for (size_t idx = 0; idx < SIZE / sizeof(uint64_t); idx++)
      {
        if (ptr[idx])
          return false;
      }
      return true;
    }

    void
    Zero()
    {
      m_data.fill(0);
    }

    virtual void
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
    FromBytestring(llarp_buffer_t* buf)
    {
      if (buf->sz != sz)
      {
        llarp::LogError("bdecode buffer size mismatch ", buf->sz, "!=", sz);
        return false;
      }
      memcpy(data(), buf->base, sz);
      return true;
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
      if (!bencode_read_string(buf, &strbuf))
      {
        return false;
      }
      return FromBytestring(&strbuf);
    }

    std::string
    ToHex() const
    {
      return oxenmq::to_hex(begin(), end());
    }

    std::string
    ShortHex() const
    {
      return oxenmq::to_hex(begin(), begin() + 4);
    }

    bool
    FromHex(std::string_view str)
    {
      if (str.size() != 2 * size() || !oxenmq::is_hex(str))
        return false;
      oxenmq::from_hex(str.begin(), str.end(), begin());
      return true;
    }

    std::ostream&
    print(std::ostream& stream, int level, int spaces) const
    {
      Printer printer(stream, level, spaces);
      printer.printValue(ToHex());

      return stream;
    }

   private:
    Data m_data;
  };
}  // namespace llarp

namespace std
{
  template <size_t sz>
  struct hash<llarp::AlignedBuffer<sz>>
  {
    std::size_t
    operator()(const llarp::AlignedBuffer<sz>& buf) const noexcept
    {
      std::size_t h = 0;
      std::memcpy(&h, buf.data(), sizeof(std::size_t));
      return h;
    }
  };
}  // namespace std
