#include <llarp/mem.h>

namespace llarp
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
  void llarp_mem_std()
  {
    llarp_g_mem.malloc = llarp::std_malloc;
    llarp_g_mem.free = llarp::std_free;
    llarp_g_mem.calloc = llarp::std_calloc;
    llarp_g_mem.realloc = llarp::std_realloc;
  }
}
