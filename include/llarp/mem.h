#ifndef LLARP_MEM_H_
#define LLARP_MEM_H_

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct llarp_alloc {
  void * impl;
  void *(*alloc)(struct llarp_alloc * mem, size_t sz, size_t align);
  void (*free)(struct llarp_alloc * mem, void *ptr);
};
  
void llarp_mem_stdlib(struct llarp_alloc * mem);
void llarp_mem_jemalloc(struct llarp_alloc * mem);
void llarp_mem_dmalloc(struct llarp_alloc * mem);
  
void llarp_mem_slab(struct llarp_alloc * mem, uint32_t * buf, size_t sz);


/** constant time memcmp */
bool llarp_eq(const void * a, const void * b, size_t sz);
  
#ifdef __cplusplus
}
#endif

#endif
