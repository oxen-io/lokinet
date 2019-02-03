#ifndef LLARP_BUFFER_HPP
#define LLARP_BUFFER_HPP

#include <util/common.hpp>
#include <util/mem.h>
#include <util/types.hpp>

#include <cassert>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <utility>

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

  ALWAYS pass a llarp_buffer_t if you are doing a read only operation that does
  not modify the buffer

  ALWAYS pass a llarp_buffer_t if you don't want to advance the stream position

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
  byte_t *base;
  /// memory address of stream position
  byte_t *cur;
  /// max size of buffer
  size_t sz;

  byte_t operator[](size_t x)
  {
    return *(this->base + x);
  }

  llarp_buffer_t() : base(nullptr), cur(nullptr), sz(0)
  {
  }

  llarp_buffer_t(byte_t *b, byte_t *c, size_t s) : base(b), cur(c), sz(s)
  {
  }

  llarp_buffer_t(const ManagedBuffer &) = delete;
  llarp_buffer_t(ManagedBuffer &&)      = delete;

  template < typename T >
  llarp_buffer_t(T *buf, size_t _sz)
      : base(reinterpret_cast< byte_t * >(buf)), cur(base), sz(_sz)
  {
  }

  template < typename T >
  llarp_buffer_t(const T *buf, size_t _sz)
      : base(reinterpret_cast< byte_t * >(const_cast< T * >(buf)))
      , cur(base)
      , sz(_sz)
  {
  }

  /** initialize llarp_buffer_t from container */
  template < typename T >
  llarp_buffer_t(T &t) : base(t.data()), cur(t.data()), sz(t.size())
  {
    // use data over the first element to "enforce" the container used has
    // contiguous memory. (Note this isn't required by the standard, but a
    // reasonable test on most standard library implementations).
  }

  template < typename T >
  llarp_buffer_t(const T &t) : llarp_buffer_t(t.data(), t.size())
  {
  }

 private:
  friend struct ManagedBuffer;
  llarp_buffer_t(const llarp_buffer_t &) = default;
  llarp_buffer_t(llarp_buffer_t &&)      = default;
};

struct ManagedBuffer
{
  llarp_buffer_t underlying;

  explicit ManagedBuffer(const llarp_buffer_t &b) : underlying(b)
  {
  }

  ManagedBuffer(ManagedBuffer &&)      = default;
  ManagedBuffer(const ManagedBuffer &) = default;
};

/// how much room is left in buffer
size_t
llarp_buffer_size_left(const llarp_buffer_t &buff);

/// write a chunk of data size "sz"
bool
llarp_buffer_write(llarp_buffer_t *buff, const void *data, size_t sz);

/// write multiple strings
bool
llarp_buffer_writef(llarp_buffer_t *buff, const char *fmt, ...);

/// read buffer upto character delimiter
size_t
llarp_buffer_read_until(llarp_buffer_t *buff, char delim, byte_t *result,
                        size_t resultlen);
/// compare buffers, true if equal else false
bool
llarp_buffer_eq(const llarp_buffer_t &buff, const char *data);

/// put big endian unsigned 16 bit integer
bool
llarp_buffer_put_uint16(llarp_buffer_t *buf, uint16_t i);

/// put big endian unsigned 32 bit integer
bool
llarp_buffer_put_uint32(llarp_buffer_t *buf, uint32_t i);

/// read big endian unsigned 16 bit integer
bool
llarp_buffer_read_uint16(llarp_buffer_t *buf, uint16_t *i);

/// read big endian unsigned 32 bit integer
bool
llarp_buffer_read_uint32(llarp_buffer_t *buf, uint32_t *i);

#endif
