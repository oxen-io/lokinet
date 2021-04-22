#pragma once

#include <type_traits>
#include "common.hpp"
#include "mem.h"
#include "types.hpp"

#include <cassert>
#include <iterator>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>
#include <algorithm>
#include <memory>
#include <vector>

/**
 * buffer.h
 *
 * generic memory buffer
 *
 * TODO: replace usage of these with std::span (via a backport until we move to C++20).  That's a
 * fairly big job, though, as llarp_buffer_t is currently used a bit differently (i.e. maintains
 * both start and current position, plus has some value reading/writing methods).
 */

/**
  llarp_buffer_t represents a region of memory that is ONLY
  valid in the current scope.

  make sure to follow the rules:

  ALWAYS copy the contents of the buffer if that data is to be used outside the
  current scope.

  ALWAYS pass a llarp_buffer_t * if you plan on modifying the data associated
  with the buffer

  ALWAYS pass a llarp_buffer_t * if you plan on advancing the stream position

  ALWAYS pass a const llarp_buffer_t & if you are doing a read only operation
  that does not modify the buffer

  ALWAYS pass a const llarp_buffer_t & if you don't want to advance the stream
  position

  ALWAYS bail out of the current operation if you run out of space in a buffer

  ALWAYS assume the pointers in the buffer are stack allocated memory
  (yes even if you know they are not)

  NEVER malloc() the pointers in the buffer when using it

  NEVER realloc() the pointers in the buffer when using it

  NEVER free() the pointers in the buffer when using it

  NEVER use llarp_buffer_t ** (double pointers)

  NEVER use llarp_buffer_t ** (double pointers)

  ABSOLUTELY NEVER USE DOUBLE POINTERS.

 */

struct ManagedBuffer;

struct llarp_buffer_t
{
  /// starting memory address
  byte_t* base{nullptr};
  /// memory address of stream position
  byte_t* cur{nullptr};
  /// max size of buffer
  size_t sz{0};

  byte_t
  operator[](size_t x)
  {
    return *(this->base + x);
  }

  llarp_buffer_t() = default;

  llarp_buffer_t(byte_t* b, byte_t* c, size_t s) : base(b), cur(c), sz(s)
  {}

  llarp_buffer_t(const ManagedBuffer&) = delete;
  llarp_buffer_t(ManagedBuffer&&) = delete;

  /// Construct referencing some 1-byte, trivially copyable (e.g. char, unsigned char, byte_t)
  /// pointer type and a buffer size.
  template <
      typename T,
      typename = std::enable_if_t<sizeof(T) == 1 and std::is_trivially_copyable_v<T>>>
  llarp_buffer_t(T* buf, size_t _sz)
      : base(reinterpret_cast<byte_t*>(const_cast<std::remove_const_t<T>*>(buf)))
      , cur(base)
      , sz(_sz)
  {}

  /// initialize llarp_buffer_t from containers supporting .data() and .size()
  template <
      typename T,
      typename = std::void_t<decltype(std::declval<T>().data() + std::declval<T>().size())>>
  llarp_buffer_t(T&& t) : llarp_buffer_t{t.data(), t.size()}
  {}

  byte_t*
  begin()
  {
    return base;
  }
  byte_t*
  begin() const
  {
    return base;
  }
  byte_t*
  end()
  {
    return base + sz;
  }
  byte_t*
  end() const
  {
    return base + sz;
  }

  size_t
  size_left() const;

  template <typename OutputIt>
  bool
  read_into(OutputIt begin, OutputIt end);

  template <typename InputIt>
  bool
  write(InputIt begin, InputIt end);

#ifndef _WIN32
  bool
  writef(const char* fmt, ...) __attribute__((format(printf, 2, 3)));

#elif defined(__MINGW64__) || defined(__MINGW32__)
  bool
  writef(const char* fmt, ...) __attribute__((__format__(__MINGW_PRINTF_FORMAT, 2, 3)));
#else
  bool
  writef(const char* fmt, ...);
#endif

  bool
  put_uint16(uint16_t i);
  bool
  put_uint32(uint32_t i);

  bool
  put_uint64(uint64_t i);

  bool
  read_uint16(uint16_t& i);
  bool
  read_uint32(uint32_t& i);

  bool
  read_uint64(uint64_t& i);

  size_t
  read_until(char delim, byte_t* result, size_t resultlen);

  /// make a copy of this buffer
  std::vector<byte_t>
  copy() const;

 private:
  friend struct ManagedBuffer;
  llarp_buffer_t(const llarp_buffer_t&) = default;
  llarp_buffer_t(llarp_buffer_t&&) = default;
};

bool
operator==(const llarp_buffer_t& buff, const char* data);

template <typename OutputIt>
bool
llarp_buffer_t::read_into(OutputIt begin, OutputIt end)
{
  auto dist = std::distance(begin, end);
  if (static_cast<decltype(dist)>(size_left()) >= dist)
  {
    std::copy_n(cur, dist, begin);
    cur += dist;
    return true;
  }
  return false;
}

template <typename InputIt>
bool
llarp_buffer_t::write(InputIt begin, InputIt end)
{
  auto dist = std::distance(begin, end);
  if (static_cast<decltype(dist)>(size_left()) >= dist)
  {
    cur = std::copy(begin, end, cur);
    return true;
  }
  return false;
}

/**
 Provide a copyable/moveable wrapper around `llarp_buffer_t`.
 */
struct ManagedBuffer
{
  llarp_buffer_t underlying;

  ManagedBuffer() = delete;

  explicit ManagedBuffer(const llarp_buffer_t& b) : underlying(b)
  {}

  ManagedBuffer(ManagedBuffer&&) = default;
  ManagedBuffer(const ManagedBuffer&) = default;

  operator const llarp_buffer_t&() const
  {
    return underlying;
  }
};

namespace llarp
{
  // Wrapper around a std::unique_ptr<byte_t[]> that owns its own memory and is also implicitly
  // convertible to a llarp_buffer_t.
  struct OwnedBuffer
  {
    std::unique_ptr<byte_t[]> buf;
    size_t sz;

    template <typename T, typename = std::enable_if_t<sizeof(T) == 1>>
    OwnedBuffer(std::unique_ptr<T[]> buf, size_t sz)
        : buf{reinterpret_cast<byte_t*>(buf.release())}, sz{sz}
    {}

    // Create a new, uninitialized owned buffer of the given size.
    explicit OwnedBuffer(size_t sz) : OwnedBuffer{std::make_unique<byte_t[]>(sz), sz}
    {}

    OwnedBuffer(const OwnedBuffer&) = delete;
    OwnedBuffer&
    operator=(const OwnedBuffer&) = delete;
    OwnedBuffer(OwnedBuffer&&) = default;
    OwnedBuffer&
    operator=(OwnedBuffer&&) = delete;

    // Implicit conversion so that this OwnedBuffer can be passed to anything taking a
    // llarp_buffer_t
    operator llarp_buffer_t()
    {
      return {buf.get(), sz};
    }

    // Creates an owned buffer by copying from a llarp_buffer_t.  (Can also be used to copy from
    // another OwnedBuffer via the implicit conversion operator above).
    static OwnedBuffer
    copy_from(const llarp_buffer_t& b);

    // Creates an owned buffer by copying the used portion of a llarp_buffer_t (i.e. from base to
    // cur), for when a llarp_buffer_t is used in write mode.
    static OwnedBuffer
    copy_used(const llarp_buffer_t& b);
  };

}  // namespace llarp
