#include <llarp/mem.h>
#include <jemalloc/jemalloc.h>

namespace llarp
{
  static void * jem_malloc(size_t sz, size_t align)
  {
    return mallocx(sz, MALLOCX_ALIGN(align));
  }

  static void jem_free(void * ptr)
  {
    if(ptr) free(ptr);
  }
}

extern "C" {
  void llarp_mem_jemalloc()
  {
    llarp_g_mem.alloc = llarp::jem_malloc;
    llarp_g_mem.free = llarp::jem_free;
  }
}
