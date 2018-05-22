#ifndef LLARP_BUFFER_H_
#define LLARP_BUFFER_H_
#include <llarp/common.h>
#include <llarp/mem.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint8_t byte_t;

typedef struct llarp_buffer_t
{
  byte_t *base;
  byte_t *cur;
  size_t sz;
} llarp_buffer_t;

size_t
llarp_buffer_size_left(llarp_buffer_t *buff);

bool
llarp_buffer_write(llarp_buffer_t *buff, const void *data, size_t sz);

bool
llarp_buffer_writef(llarp_buffer_t *buff, const char *fmt, ...);

bool
llarp_buffer_readfile(llarp_buffer_t *buff, FILE *f, struct llarp_alloc *mem);

size_t
llarp_buffer_read_until(llarp_buffer_t *buff, char delim, byte_t *result,
                        size_t resultlen);

bool
llarp_buffer_eq(llarp_buffer_t buff, const char *data);

#ifdef __cplusplus
}
#endif

#endif
