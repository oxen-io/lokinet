#include <sarp/mem.h>

namespace sarp
{
  void * std_malloc(size_t sz)
  {
    void * ptr = malloc(sz);
    if(ptr) return ptr;
    abort();
  }

  void std_free(void * ptr)
  {
    if(ptr) free(ptr);
  }

  void * std_calloc(size_t n, size_t sz)
  {
    void * ptr = calloc(n, sz);
    if (ptr) return ptr;
    abort();
  }

  void * std_realloc(void * ptr, size_t sz)
  {
    ptr = realloc(ptr, sz);
    if (ptr) return ptr;
    abort();
  }
  
}

extern "C" {
  void sarp_mem_std(struct sarp_alloc * mem)
  {
    mem->malloc = sarp::std_malloc;
    mem->free = sarp::std_free;
    mem->calloc = sarp::std_calloc;
    mem->realloc = sarp::std_realloc;
  }
}
