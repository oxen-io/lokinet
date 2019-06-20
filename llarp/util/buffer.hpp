#ifndef LLARP_BUFFER_HPP
#define LLARP_BUFFER_HPP

#include <util/mem.hpp>
#include <util/types.hpp>

#include <cassert>
#include <iterator>
#include <cstdio>
#include <cstdlib>
#include <utility>
#include <algorithm>

/**
 * buffer.h
 *
 * generic memory buffer
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
  byte_t *base{nullptr};
  /// memory address of stream position
  byte_t *cur{nullptr};
  /// max size of buffer
  size_t sz{0};

  byte_t operator[](size_t x)
  {
    return *(this->base + x);  // NOLINT
  }

  constexpr llarp_buffer_t() = default;

  llarp_buffer_t(byte_t *b, byte_t *c, size_t s) : base(b), cur(c), sz(s)
  {
  }

  llarp_buffer_t(const ManagedBuffer &) = delete;
  llarp_buffer_t(ManagedBuffer &&)      = delete;

  llarp_buffer_t(byte_t *buf, size_t _sz) : base(buf), cur(base), sz(_sz)
  {
  }

  llarp_buffer_t(const byte_t *buf, size_t _sz)
      : llarp_buffer_t(const_cast< byte_t * >(buf), _sz)
  {
  }

  llarp_buffer_t(char *buf, size_t _sz)
      : llarp_buffer_t(reinterpret_cast< byte_t * >(buf), _sz)
  {
  }

  llarp_buffer_t(const char *buf, size_t _sz)
      : llarp_buffer_t(const_cast< char * >(buf), _sz)
  {
  }

  /** initialize llarp_buffer_t from container */
  template < typename T >
  llarp_buffer_t(T &t) : llarp_buffer_t(t.data(), t.size())
  {
    // use data over the first element to "enforce" the container used has
    // contiguous memory. (Note this isn't required by the standard, but a
    // reasonable test on most standard library implementations).
  }

  template < typename T >
  llarp_buffer_t(const T &t) : llarp_buffer_t(t.data(), t.size())
  {
  }

  // clang-format off
  byte_t * begin()       { return base; }
  byte_t * begin() const { return base; }
  byte_t * end()         { return base + sz; }
  byte_t * end()   const { return base + sz; }
  // clang-format on

  size_t
  size_left() const;

  template < typename OutputIt >
  bool
  read_into(OutputIt begin, OutputIt end);

  template < typename InputIt >
  bool
  write(InputIt begin, InputIt end);

#ifndef _WIN32
  bool
  writef(const char *fmt, ...) __attribute__((format(printf, 2, 3)));

#elif defined(__MINGW64__) || defined(__MINGW32__)
  bool
  writef(const char *fmt, ...)
      __attribute__((__format__(__MINGW_PRINTF_FORMAT, 2, 3)));
#else
  bool
  writef(const char *fmt, ...);
#endif

  bool
  put_uint16(uint16_t i);
  bool
  put_uint32(uint32_t i);

  bool
  put_uint64(uint64_t i);

  bool
  read_uint16(uint16_t &i);
  bool
  read_uint32(uint32_t &i);

  bool
  read_uint64(uint64_t &i);

  size_t
  read_until(char delim, byte_t *result, size_t resultsize);

 private:
  friend struct ManagedBuffer;
  llarp_buffer_t(const llarp_buffer_t &) = default;
  llarp_buffer_t(llarp_buffer_t &&)      = default;
};

bool
operator==(const llarp_buffer_t &buff, const char *str);

template < typename OutputIt >
bool
llarp_buffer_t::read_into(OutputIt begin, OutputIt end)
{
  auto dist = std::distance(begin, end);
  if(static_cast< decltype(dist) >(size_left()) >= dist)
  {
    std::copy_n(cur, dist, begin);
    cur += dist;
    return true;
  }
  return false;
}

template < typename InputIt >
bool
llarp_buffer_t::write(InputIt begin, InputIt end)
{
  auto dist = std::distance(begin, end);
  if(static_cast< decltype(dist) >(size_left()) >= dist)
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

  explicit ManagedBuffer(const llarp_buffer_t &b) : underlying(b)
  {
  }

  ManagedBuffer(ManagedBuffer &&)      = default;
  ManagedBuffer(const ManagedBuffer &) = default;

  operator const llarp_buffer_t &() const
  {
    return underlying;
  }
};

#endif
