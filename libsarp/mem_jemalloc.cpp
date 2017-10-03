#include <sarp/mem.h>
#include <jemalloc/jemalloc.h>

namespace sarp
{
  static void * jem_malloc(size_t sz)
  {
    return mallocx(sz, MALLOCX_ZERO);
  }

  static void jem_free(void * ptr)
  {
    if(ptr) free(ptr);
  }

  static void * jem_calloc(size_t n, size_t sz)
  {
    return mallocx(n * sz, MALLOCX_ZERO);
  }

  static void * jem_realloc(void * ptr, size_t sz)
  {
    return rallocx(ptr, sz, MALLOCX_ZERO);
  }
}

extern "C" {
  void sarp_mem_jemalloc(struct sarp_alloc * mem)
  {
    mem->malloc = sarp::jem_malloc;
    mem->free = sarp::jem_free;
    mem->calloc = sarp::jem_calloc;
    mem->realloc = sarp::jem_realloc;
  }
}
