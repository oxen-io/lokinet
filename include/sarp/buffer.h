#ifndef SARP_BUFFER_H_
#define SARP_BUFFER_H_
#ifdef __cplusplus
extern "C" {
#endif
#include <stdlib.h>

  typedef struct sarp_buffer_t {
    char * base;
    size_t sz;
    char * cur;
  } sarp_buffer_t;

  size_t sarp_buffer_size_left(sarp_buffer_t * buff);
  
#ifdef __cplusplus
}
#endif
  
#endif
