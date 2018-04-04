#ifndef LLARP_BUFFER_H_
#define LLARP_BUFFER_H_

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct llarp_buffer_t {
  char *base;
  size_t sz;
  char *cur;
} llarp_buffer_t;

inline size_t llarp_buffer_size_left(llarp_buffer_t *buff)
{
  size_t diff = buff->cur - buff->base;
  if ( diff > buff->sz) return 0;
  else return buff->sz - diff;
}
  
inline bool llarp_buffer_write(llarp_buffer_t *buff, const void *data, size_t sz)
{
  size_t left = llarp_buffer_size_left(buff);
  if (left >= sz)
  {
    memcpy(buff->cur, data, sz);
    buff->cur += sz;
    return true;
  }
  return false;
}

bool llarp_buffer_writef(llarp_buffer_t *buff, const char * fmt, ...);

  
#ifdef __cplusplus
}
#endif

#endif
