#include <llarp/mem.h>
#include <jemalloc/jemalloc.h>

namespace llarp
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
  void llarp_mem_jemalloc()
  {
    llarp_g_mem.malloc = llarp::jem_malloc;
    llarp_g_mem.free = llarp::jem_free;
    llarp_g_mem.calloc = llarp::jem_calloc;
    llarp_g_mem.realloc = llarp::jem_realloc;
  }
}
