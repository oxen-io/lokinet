#ifndef SARP_BUFFER_H_
#define SARP_BUFFER_H_
#ifdef __cplusplus
extern "C" {
#endif
#include <stdlib.h>
#include <stdint.h>

  typedef struct sarp_buffer_t {
    uint8_t * base;
    size_t sz;
    uint8_t * cur;
  } sarp_buffer_t;

  static inline size_t sarp_buffer_size_left(sarp_buffer_t * buff)
  {
    return buff->sz - (buff->cur - buff->base);
  }
  
#ifdef __cplusplus
}
#endif
  
#endif
