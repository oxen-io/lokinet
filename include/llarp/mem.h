#ifndef LLARP_MEM_H_
#define LLARP_MEM_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>

struct llarp_alloc {
  void *(*alloc)(size_t sz, size_t align);
  void (*free)(void *ptr);
};

/** global memory allocator */
extern struct llarp_alloc llarp_g_mem;
/** init llarp_g_mem with stdlib malloc */
void llarp_mem_stdlib();
/** init llarp_g_mem with jemalloc */
void llarp_mem_jemalloc();
/** init llarp_g_mem with dmalloc */
void llarp_mem_dmalloc();

#ifdef __cplusplus
}
#endif

#endif
