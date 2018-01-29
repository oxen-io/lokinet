#ifndef LLARP_BUFFER_H_
#define LLARP_BUFFER_H_

#include <stdbool.h>
#include <stdlib.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct llarp_buffer_t {
  char *base;
  size_t sz;
  char *cur;
} llarp_buffer_t;

size_t llarp_buffer_size_left(llarp_buffer_t *buff);
bool llarp_buffer_write(llarp_buffer_t *buff, const void *data, size_t sz);

#ifdef __cplusplus
}
#endif

#endif
