#ifndef SARP_MEM_H_
#define SARP_MEM_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>

struct sarp_alloc
{
  void * (*malloc)(size_t sz);
  void * (*realloc)(void * ptr, size_t sz);
  void * (*calloc)(size_t n, size_t sz);
  void (*free)(void * ptr);
};
void sarp_mem_jemalloc(struct sarp_alloc * mem);
void sarp_mem_libc(struct sarp_alloc * mem);
void sarp_mem_dmalloc(struct sarp_alloc * mem);

#ifdef __cplusplus
}
#endif

  
#endif
