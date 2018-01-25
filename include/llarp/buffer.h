#ifndef LLARP_BUFFER_H_
#define LLARP_BUFFER_H_
#ifdef __cplusplus
extern "C" {
#endif
#include <stdlib.h>

  typedef struct llarp_buffer_t {
    char * base;
    size_t sz;
    char * cur;
  } llarp_buffer_t;

  size_t llarp_buffer_size_left(llarp_buffer_t * buff);
  
#ifdef __cplusplus
}
#endif
  
#endif
