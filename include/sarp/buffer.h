#ifndef SARP_BUFFER_H_
#define SARP_BUFFER_H_
#ifdef __cplusplus
extern "C" {
#endif
#include <stdlib.h>
#include <stdint.h>

typedef struct {
  uint8_t * ptr;
  size_t sz;
} sarp_buffer_t;

#ifdef __cplusplus
}
#endif
  
#endif
