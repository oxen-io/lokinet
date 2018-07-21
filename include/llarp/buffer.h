#ifndef LLARP_BUFFER_H_
#define LLARP_BUFFER_H_
#include <llarp/common.h>
#include <llarp/mem.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * buffer.h
 *
 * generic memory buffer
 */

typedef uint8_t byte_t;

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
typedef struct llarp_buffer_t
{
  /// starting memory address
  byte_t *base;
  /// memory address of stream position
  byte_t *cur;
  /// max size of buffer
  size_t sz;

  const byte_t
  operator[](size_t x)
  {
    return *(this->base + x);
  }
} llarp_buffer_t;

/// how much room is left in buffer
size_t
llarp_buffer_size_left(llarp_buffer_t buff);

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
llarp_buffer_eq(llarp_buffer_t buff, const char *data);

#endif
