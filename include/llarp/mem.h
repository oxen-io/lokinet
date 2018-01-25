#ifndef LLARP_MEM_H_
#define LLARP_MEM_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>

  struct llarp_alloc
  {
    void * (*malloc)(size_t sz);
    void * (*realloc)(void * ptr, size_t sz);
    void * (*calloc)(size_t n, size_t sz);
    void (*free)(void * ptr);
  };
  
  /** global memory allocator */
  extern struct llarp_alloc llarp_g_mem;
  
  void llarp_mem_jemalloc();
  void llarp_mem_std();
  void llarp_mem_dmalloc();

#ifdef __cplusplus
}
#endif

  
#endif
