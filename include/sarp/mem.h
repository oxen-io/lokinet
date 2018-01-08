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
  
  /** global memory allocator */
  extern struct sarp_alloc sarp_g_mem;
  
  void sarp_mem_jemalloc();
  void sarp_mem_std();
  void sarp_mem_dmalloc();

#ifdef __cplusplus
}
#endif

  
#endif
