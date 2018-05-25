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
 * buffer used for bencoding
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef uint8_t byte_t;

typedef struct llarp_buffer_t
{
  /// starting memory address
  byte_t *base;
  /// memory address of stream position
  byte_t *cur;
  /// max size of buffer
  size_t sz;
} llarp_buffer_t;

/// how much room is left in buffer
size_t
llarp_buffer_size_left(llarp_buffer_t *buff);

/// write a chunk of data size "sz"
bool
llarp_buffer_write(llarp_buffer_t *buff, const void *data, size_t sz);

/// write multiple strings
bool
llarp_buffer_writef(llarp_buffer_t *buff, const char *fmt, ...);

/// read from file into buff using allocator "mem"
bool
llarp_buffer_readfile(llarp_buffer_t *buff, FILE *f, struct llarp_alloc *mem);

/// read buffer upto character delimiter
size_t
llarp_buffer_read_until(llarp_buffer_t *buff, char delim, byte_t *result,
                        size_t resultlen);
/// compare buffers, true if equal else false
bool
llarp_buffer_eq(llarp_buffer_t buff, const char *data);

#ifdef __cplusplus
}
#endif

#endif
